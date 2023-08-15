#include "gstobstetrics.h"
#include <uuid/uuid.h>

#include <algorithm>
#include <sys/stat.h>

GST_DEBUG_CATEGORY_STATIC (gst_this_plugin_debug);
#define GST_CAT_DEFAULT gst_this_plugin_debug
#define JSON_MAX_SIZE 40960000

/** set the user metadata type */
#define NVDS_USER_FRAME_META_OBSTETRICS (nvds_get_user_meta_type("HEBIN.OBSTETRICS.USER_META"))

/* enable to write transformed cvmat to files */
static GQuark _dsmeta_quark = 0;

/* Enum to identify properties */
enum
{
  PROP_0,
  PROP_UNIQUE_ID,
  PROP_GPU_DEVICE_ID,
  // add roperties here
  // pipeline配置文件
  PROP_PIPELINE_CFG_PATH,
  // 开启全新会诊，用于初始化变量
  PROP_RESET,
  // 截屏
  PROP_SCREENSHOT,
  // 文件存储路径
  PROP_OUTPUT_DIR,
  // 医生测量
  PROP_MANUAL,
  // 医生测量类别
  PROP_MEASURE_NAME,
  // 医生测量图片路径
  PROP_IMG_PATH,
  // 跳帧发送
  PROP_SKIP_FRAME
};

#define CHECK_NVDS_MEMORY_AND_GPUID(object, surface)  \
  ({ int _errtype=0;\
   do {  \
    if ((surface->memType == NVBUF_MEM_DEFAULT || surface->memType == NVBUF_MEM_CUDA_DEVICE) && \
        (surface->gpuId != object->gpu_id))  { \
    GST_ELEMENT_ERROR (object, RESOURCE, FAILED, \
        ("Input surface gpu-id doesnt match with configured gpu-id for element," \
         " please allocate input using unified memory, or use same gpu-ids"),\
        ("surface-gpu-id=%d,%s-gpu-id=%d",surface->gpuId,GST_ELEMENT_NAME(object),\
         object->gpu_id)); \
    _errtype = 1;\
    } \
    } while(0); \
    _errtype; \
  })


/* Default values for properties */
#define DEFAULT_UNIQUE_ID 101
#define DEFAULT_GPU_ID 0
// add roperties here
// pipeline配置文件
#define DEFAULT_PIPELINE_CFG_PATH (gchar*)""
// 开启全新会诊，用于初始化变量
#define DEFAULT_RESET 0
// 截屏
#define DEFAULT_SCREENSHOT 0
// 文件存储路径
#define DEFAULT_OUTPUT_DIR (gchar*)""
// 医生测量
#define DEFAULT_MANUAL 0
// 医生测量类别
#define DEFAULT_MEASURE_NAME (gchar*)""
// 医生测量图片路径
#define DEFAULT_IMG_PATH (gchar*)""
// 跳帧发送
#define DEFAULT_SKIP_FRAME 0

/** set the user metadata type */
// #define NVDS_USER_FRAME_META_EXAMPLE (nvds_get_user_meta_type("HARBINGER.OBSTETRICS.QUALITY_METADATA"))

// #define CHECK_NPP_STATUS(npp_status,error_str) do { \
//   if ((npp_status) != NPP_SUCCESS) { \
//     g_print ("Error: %s in %s at line %d: NPP Error %d\n", \
//         error_str, __FILE__, __LINE__, npp_status); \
//     goto error; \
//   } \
// } while (0)

#define CHECK_CUDA_STATUS(cuda_status,error_str) do { \
  if ((cuda_status) != cudaSuccess) { \
    g_print ("Error: %s in %s at line %d (%s)\n", \
        error_str, __FILE__, __LINE__, cudaGetErrorName(cuda_status)); \
    goto error; \
  } \
} while (0)


/* By default NVIDIA Hardware allocated memory flows through the pipeline. We
 * will be processing on this type of memory only. */
#define GST_CAPS_FEATURE_MEMORY_NVMM "memory:NVMM"
static GstStaticPadTemplate gst_this_plugin_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{RGBA}")));

static GstStaticPadTemplate gst_this_plugin_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_NVMM,
            "{RGBA}")));


/* Define our element type. Standard GObject/GStreamer boilerplate stuff */
#define gst_this_plugin_parent_class parent_class
G_DEFINE_TYPE (GstThisPlugin, gst_this_plugin, GST_TYPE_BASE_TRANSFORM);

static void gst_this_plugin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_this_plugin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_this_plugin_set_caps (GstBaseTransform * btrans,
    GstCaps * incaps, GstCaps * outcaps);
static gboolean gst_this_plugin_start (GstBaseTransform * btrans);
static gboolean gst_this_plugin_stop (GstBaseTransform * btrans);

static GstFlowReturn gst_this_plugin_transform_ip (GstBaseTransform *
    btrans, GstBuffer * inbuf);

/* Custom function*/
void attach_objectmeta(_GstThisPlugin *this_plugin, NvDsFrameMeta *frame_meta, cv::Rect rect, std::string text, gfloat score, bool display,
                       NvOSD_ColorParams rect_color, NvOSD_ColorParams text_color);
void *set_metadata_ptr(gchar*);
static gpointer copy_user_meta(gpointer data, gpointer user_data);
static void release_user_meta(gpointer data, gpointer user_data);


/* Install properties, set sink and src pad capabilities, override the required
 * functions of the base class, These are common to all instances of the
 * element.
 */
static void
gst_this_plugin_class_init (GstThisPluginClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseTransformClass *gstbasetransform_class;

  /* Indicates we want to use DS buf api */
  g_setenv ("DS_NEW_BUFAPI", "1", TRUE);

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;
  gstbasetransform_class = (GstBaseTransformClass *) klass;

  /* Overide base class functions */
  gobject_class->set_property = GST_DEBUG_FUNCPTR (gst_this_plugin_set_property);
  gobject_class->get_property = GST_DEBUG_FUNCPTR (gst_this_plugin_get_property);

  gstbasetransform_class->set_caps = GST_DEBUG_FUNCPTR (gst_this_plugin_set_caps);
  gstbasetransform_class->start = GST_DEBUG_FUNCPTR (gst_this_plugin_start);
  gstbasetransform_class->stop = GST_DEBUG_FUNCPTR (gst_this_plugin_stop);

  gstbasetransform_class->transform_ip =
      GST_DEBUG_FUNCPTR (gst_this_plugin_transform_ip);

  /* Install properties */
  g_object_class_install_property (gobject_class, PROP_UNIQUE_ID,
      g_param_spec_uint ("unique-id",
          "Unique ID",
          "Unique ID for the element. Can be used to identify output of the"
          " element", 0, G_MAXUINT, DEFAULT_UNIQUE_ID, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)));

  g_object_class_install_property (gobject_class, PROP_GPU_DEVICE_ID,
      g_param_spec_uint ("gpu-id",
          "Set GPU Device ID",
          "Set GPU Device ID", 0,
          G_MAXUINT, 0,
          GParamFlags
          (G_PARAM_READWRITE |
              G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));             
  
  // add roperties here
  // pipeline配置文件
  g_object_class_install_property (gobject_class, PROP_PIPELINE_CFG_PATH,
      g_param_spec_string ("pipeline-cfg-path", "Pipeline config path",
          "Path to the pipeline config path",
          DEFAULT_PIPELINE_CFG_PATH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));
  
  // 开启全新会诊，用于初始化变量
  g_object_class_install_property (gobject_class, PROP_RESET,
      g_param_spec_boolean ("reset", "Reset",
          "Start a new consultation and reset variables.",
          DEFAULT_RESET,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_PLAYING))); 

  // 截屏
  g_object_class_install_property (gobject_class, PROP_SCREENSHOT,
      g_param_spec_boolean ("screenshot", "Screenshot",
          "Doctor manually takes a screenshot.",
          DEFAULT_SCREENSHOT,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_PLAYING)));
  // 文件输出路径
  g_object_class_install_property (gobject_class, PROP_OUTPUT_DIR,
      g_param_spec_string ("output-dir", "Output dir",
          "File output path",
          DEFAULT_OUTPUT_DIR,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  // 医生测量
  g_object_class_install_property (gobject_class, PROP_MANUAL,
      g_param_spec_boolean ("manual", "Manual",
          "Doctor manually measure.",
          DEFAULT_MANUAL,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_PLAYING)));

  // 医生测量名称
  g_object_class_install_property (gobject_class, PROP_MEASURE_NAME,
      g_param_spec_string ("measure-name", "Measure Name",
          "Measure Name",
          DEFAULT_MEASURE_NAME,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  // 医生测量图片路径
  g_object_class_install_property (gobject_class, PROP_IMG_PATH,
      g_param_spec_string ("img-path", "Img Path",
          "Img Path",
          DEFAULT_IMG_PATH,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              GST_PARAM_MUTABLE_READY)));

  // 跳帧发送
  g_object_class_install_property (gobject_class, PROP_SKIP_FRAME,
      g_param_spec_uint ("skip-frame",
          "Skip Frame Send Json",
          "Skip Frame Send Json", 0, G_MAXUINT, DEFAULT_SKIP_FRAME, (GParamFlags)
          (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_READY)));

  /* Set sink and src pad capabilities */
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_this_plugin_src_template));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&gst_this_plugin_sink_template));

  /* Set metadata describing the element */
  gst_element_class_set_details_simple (gstelement_class,
      "Obstetrics plugin",
      "Obstetrics plugin",
      "Process a 3rdparty example algorithm on objects / full frame",
      "NVIDIA Corporation. Post on Deepstream for Tesla forum for any queries "
      "@ https://devtalk.nvidia.com/default/board/209/");
}

static void
gst_this_plugin_init (GstThisPlugin * this_plugin)
{
  GstBaseTransform *btrans = GST_BASE_TRANSFORM (this_plugin);

  /* We will not be generating a new buffer. Just adding / updating
   * metadata. */
  gst_base_transform_set_in_place (GST_BASE_TRANSFORM (btrans), TRUE);
  /* We do not want to change the input caps. Set to passthrough. transform_ip
   * is still called. */
  gst_base_transform_set_passthrough (GST_BASE_TRANSFORM (btrans), TRUE);

  /* Initialize all property variables to default values */
  this_plugin->unique_id = DEFAULT_UNIQUE_ID;
  this_plugin->gpu_id = DEFAULT_GPU_ID;
  // add roperties here
  // pipeline配置文件
  this_plugin->pipeline_cfg_path = g_strdup (DEFAULT_PIPELINE_CFG_PATH);
  // 开启全新会诊，用于初始化变量
  this_plugin->reset = DEFAULT_RESET;
  // 截屏
  this_plugin->screenshot = DEFAULT_SCREENSHOT;
  // 文件输出路径
  this_plugin->output_dir = g_strdup (DEFAULT_OUTPUT_DIR);
  // 医生测量
  this_plugin->manual = DEFAULT_MANUAL;
  // 医生测量名称
  this_plugin->measure_name = g_strdup (DEFAULT_MEASURE_NAME);
  // 医生测量图片路径
  this_plugin->img_path = g_strdup (DEFAULT_IMG_PATH);
  // 跳帧发送
  this_plugin->skip_frame = DEFAULT_SKIP_FRAME;


  /* This quark is required to identify NvDsMeta when iterating through
   * the buffer metadatas */
  if (!_dsmeta_quark)
    _dsmeta_quark = g_quark_from_static_string (NVDS_META_STRING);
}

/* Function called when a property of the element is set. Standard boilerplate.
 */
static void
gst_this_plugin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (object);
  switch (prop_id) {
    case PROP_UNIQUE_ID:
      this_plugin->unique_id = g_value_get_uint (value);
      break;
    case PROP_GPU_DEVICE_ID:
      this_plugin->gpu_id = g_value_get_uint (value);
      break;           
    // add roperties here
    // pipeline配置文件
    case PROP_PIPELINE_CFG_PATH:
      this_plugin->pipeline_cfg_path = g_value_dup_string (value);
      break;
    // 开启全新会诊，用于初始化变量
    case PROP_RESET:
      this_plugin->reset = g_value_get_boolean (value);
      break;
    // 截屏
    case PROP_SCREENSHOT:
      this_plugin->screenshot = g_value_get_boolean (value);
      break;
    // 文件输出路径
    case PROP_OUTPUT_DIR:
      this_plugin->output_dir = g_value_dup_string (value);
      break;
    // 医生测量
    case PROP_MANUAL:
      this_plugin->manual = g_value_get_boolean (value);
      break;
    // 医生测量名称
    case PROP_MEASURE_NAME:
      this_plugin->measure_name = g_value_dup_string (value);
      break;
    // 医生测量图片路径
    case PROP_IMG_PATH:
      this_plugin->img_path = g_value_dup_string (value);
      break;
    // 跳帧发送
    case PROP_SKIP_FRAME:
      this_plugin->skip_frame = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Function called when a property of the element is requested. Standard
 * boilerplate.
 */
static void
gst_this_plugin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (object);

  switch (prop_id) {
    case PROP_UNIQUE_ID:
      g_value_set_uint (value, this_plugin->unique_id);
      break;
    case PROP_GPU_DEVICE_ID:
      g_value_set_uint (value, this_plugin->gpu_id);
      break;    
    // add roperties here
    // pipeline配置文件
    case PROP_PIPELINE_CFG_PATH:
      g_value_set_string (value, this_plugin->pipeline_cfg_path);
      break;
    // 开启全新会诊，用于初始化变量
    case PROP_RESET:
      g_value_set_boolean (value, this_plugin->reset);
      break;
    // 截屏
    case PROP_SCREENSHOT:
      g_value_set_boolean (value, this_plugin->screenshot);
      break;
    // 文件输出路径
    case PROP_OUTPUT_DIR:
      g_value_set_string (value, this_plugin->output_dir);
      break;
    // 医生测量
    case PROP_MANUAL:
      g_value_set_boolean (value, this_plugin->manual);
      break;
    // 医生测量名称
    case PROP_MEASURE_NAME:
      g_value_set_string (value, this_plugin->measure_name);
      break;
    // 医生测量图片路径
    case PROP_IMG_PATH:
      g_value_set_string (value, this_plugin->img_path);
      break;
    // 跳帧发送
    case PROP_SKIP_FRAME:
      g_value_set_uint (value, this_plugin->skip_frame);
      break; 
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/**
 * Initialize all resources and start the output thread
 */
static gboolean
gst_this_plugin_start (GstBaseTransform * btrans)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (btrans);
  NvBufSurfaceCreateParams create_params;

  GstQuery *queryparams = NULL;
  guint batch_size = 1;

  CHECK_CUDA_STATUS (cudaSetDevice (this_plugin->gpu_id),
      "Unable to set cuda device");

  this_plugin->batch_size = 1;
  queryparams = gst_nvquery_batch_size_new ();
  if (gst_pad_peer_query (GST_BASE_TRANSFORM_SINK_PAD (btrans), queryparams)
      || gst_pad_peer_query (GST_BASE_TRANSFORM_SRC_PAD (btrans), queryparams)) {
    if (gst_nvquery_batch_size_parse (queryparams, &batch_size)) {
      this_plugin->batch_size = batch_size;
    }
  }
  GST_DEBUG_OBJECT (this_plugin, "Setting batch-size %d \n",
      this_plugin->batch_size);
  gst_query_unref (queryparams);

  CHECK_CUDA_STATUS (cudaStreamCreate (&this_plugin->cuda_stream),
      "Could not create cuda stream");

  // 初始化pipeline
  this_plugin->obstetrics_pipeline = new Pipeline(this_plugin->pipeline_cfg_path);
  if (!this_plugin->obstetrics_pipeline)
    goto error;

  this_plugin->spl_update = {};
  this_plugin->tmp_skip_frame = 0;

  GST_DEBUG_OBJECT (this_plugin, "created this_plugin success!\n");

  return TRUE;
error:

  if (this_plugin->cuda_stream) {
    cudaStreamDestroy (this_plugin->cuda_stream);
    this_plugin->cuda_stream = NULL;
  }

  // 删除pipeline
  delete this_plugin->obstetrics_pipeline;
  this_plugin->obstetrics_pipeline = NULL;

  return FALSE;
}

/**
 * Stop the output thread and free up all the resources
 */
static gboolean
gst_this_plugin_stop (GstBaseTransform * btrans)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (btrans);

  if (this_plugin->cuda_stream)
    cudaStreamDestroy (this_plugin->cuda_stream);
  this_plugin->cuda_stream = NULL;

  // 删除pipeline实例
  delete this_plugin->obstetrics_pipeline;
  this_plugin->obstetrics_pipeline = NULL;  

  return TRUE;
}

/**
 * Called when source / sink pad capabilities have been negotiated.
 */
static gboolean
gst_this_plugin_set_caps (GstBaseTransform * btrans, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (btrans);
  /* Save the input video information, since this will be required later. */
  gst_video_info_from_caps (&this_plugin->video_info, incaps);

  return TRUE;
}

/**
 * Called when element recieves an input buffer from upstream element.
 */
static GstFlowReturn
gst_this_plugin_transform_ip (GstBaseTransform * btrans, GstBuffer * inbuf)
{
  GstThisPlugin *this_plugin = GST_THISPLUGIN (btrans);
  GstMapInfo in_map_info;
  GstFlowReturn flow_ret = GST_FLOW_ERROR;

  NvBufSurface *surface = NULL;
  NvDsBatchMeta *batch_meta = NULL;
  NvDsFrameMeta *frame_meta = NULL;
  NvDsMetaList * l_frame = NULL;

  NvDsUserMeta *user_meta = NULL;
  NvDsMetaType user_meta_type = NVDS_USER_FRAME_META_OBSTETRICS;

  NvDsMetaList * l_obj = NULL;
  NvDsObjectMeta *obj_meta = NULL;

  NvDsMetaList * l_cls = NULL;
  NvDsClassifierMeta * cls_meta = NULL;

  NvDsLabelInfoList * l_info = NULL;
  NvDsLabelInfo * info = NULL;

  this_plugin->frame_num++;

  CHECK_CUDA_STATUS (cudaSetDevice (this_plugin->gpu_id),
      "Unable to set cuda device");

  memset (&in_map_info, 0, sizeof (in_map_info));
  if (!gst_buffer_map (inbuf, &in_map_info, GST_MAP_READ)) {
    g_print ("Error: Failed to map gst buffer\n");
    goto error;
  }

  nvds_set_input_system_timestamp (inbuf, GST_ELEMENT_NAME (this_plugin));
  surface = (NvBufSurface *) in_map_info.data;

  GST_DEBUG_OBJECT (this_plugin,
      "Processing Frame %" G_GUINT64_FORMAT " Surface %p",
      this_plugin->frame_num, surface);

  if (CHECK_NVDS_MEMORY_AND_GPUID (this_plugin, surface))
    goto error;

  batch_meta = gst_buffer_get_nvds_batch_meta (inbuf);
  if (batch_meta == nullptr) {
    GST_ELEMENT_ERROR (this_plugin, STREAM, FAILED,
        ("NvDsBatchMeta not found for input buffer."), (NULL));
    return GST_FLOW_ERROR;
  }

  for (l_frame = batch_meta->frame_meta_list; l_frame != NULL;
    l_frame = l_frame->next)
  {

    frame_meta = (NvDsFrameMeta *) (l_frame->data);

    cv::Mat in_mat, bgr_mat;
    /* Map the buffer so that it can be accessed by CPU */
    if (surface->surfaceList[frame_meta->batch_id].mappedAddr.addr[0] == NULL){
      if (NvBufSurfaceMap (surface, frame_meta->batch_id, 0, NVBUF_MAP_READ_WRITE) != 0){
        GST_ELEMENT_ERROR (this_plugin, STREAM, FAILED,
            ("%s:buffer map to be accessed by CPU failed", __func__), (NULL));
        return GST_FLOW_ERROR;
      }
    }
    /* Cache the mapped data for CPU access */
    NvBufSurfaceSyncForCpu (surface, frame_meta->batch_id, 0);

    bgr_mat = cv::Mat(cv::Size(surface->surfaceList[frame_meta->batch_id].planeParams.width[0], 
                              surface->surfaceList[frame_meta->batch_id].planeParams.height[0]), 
                              CV_8UC3);
    
    switch (surface->surfaceList[frame_meta->batch_id].colorFormat){
      case NVBUF_COLOR_FORMAT_NV12:
        in_mat = 
          cv::Mat (surface->surfaceList[frame_meta->batch_id].planeParams.height[0]*3/2,
          surface->surfaceList[frame_meta->batch_id].planeParams.width[0], CV_8UC1,
          surface->surfaceList[frame_meta->batch_id].mappedAddr.addr[0],
          surface->surfaceList[frame_meta->batch_id].planeParams.pitch[0]);
        #if (CV_MAJOR_VERSION >= 4)
          cv::cvtColor (in_mat, bgr_mat, cv::COLOR_YUV2BGR_NV12);
        #else
          cv::cvtColor (in_mat, bgr_mat, CV_YUV2BGR_NV12);
        #endif
        break;
      case NVBUF_COLOR_FORMAT_RGBA:
        in_mat =
          cv::Mat (surface->surfaceList[frame_meta->batch_id].planeParams.height[0],
          surface->surfaceList[frame_meta->batch_id].planeParams.width[0], CV_8UC4,
          surface->surfaceList[frame_meta->batch_id].mappedAddr.addr[0],
          surface->surfaceList[frame_meta->batch_id].planeParams.pitch[0]);
          #if (CV_MAJOR_VERSION >= 4)
            cv::cvtColor (in_mat, bgr_mat, cv::COLOR_RGBA2BGR);
          #else
            cv::cvtColor (in_mat, bgr_mat, CV_RGBA2BGR);
          #endif
          break;
      default:
        g_print("colorFormat is not supported.\n");
        goto error;
    }
    if (NvBufSurfaceUnMap (surface, frame_meta->batch_id, 0)){
      GST_ELEMENT_ERROR (this_plugin, STREAM, FAILED,
        ("%s:buffer unmap to be accessed by CPU failed", __func__), (NULL));
    }

    // 收到reset, 清空spl_update变量
    bool reset = false;
    if (this_plugin->reset) {
      this_plugin->reset = false;
      reset = true;
    }

    // 定义uuid变量
    uuid_t uuid;
    char uuid_str[36];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuid_str);
    std::string img_path = std::string(this_plugin->output_dir)+"/"+std::string(uuid_str)+".png";

    // 是否发送json
    bool send_analysis = false;
    bool send_attention = false;
    // 定义json变量
    Json::Int64 timestamp = frame_meta->ntp_timestamp/1000000;
    // analysis
    Json::Value analysisdata;
    analysisdata["cmd"] = "analysis";
    analysisdata["timestamp"] = timestamp;
    analysisdata["component"] = "obstetrics";
    analysisdata["imagepath"] = img_path;
    analysisdata["shooter"] = "ai";
    analysisdata["planes"].resize(0);
    // attention
    Json::Value attentiondata;
    attentiondata["cmd"] = "attention";
    attentiondata["timestamp"] = timestamp;
    attentiondata["component"] = "obstetrics";
    attentiondata["imagepath"] = "";
    attentiondata["filepath"] = "obstetrics";
    attentiondata["action"] = "ai";

    // 截屏
    if (this_plugin->screenshot) {
      send_attention = true;
      attentiondata["action"] = "doctor";
      send_analysis = true;
      analysisdata["shooter"] = "doctor";
      this_plugin->screenshot = false;
    }

    std::vector<Detect> dets;
    // 解析检测结果
    for (l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next)
    {
      obj_meta = (NvDsObjectMeta *) (l_obj->data);
      std::string label = obj_meta->obj_label;
      cv::Rect bbox(obj_meta->rect_params.left, obj_meta->rect_params.top, obj_meta->rect_params.width, obj_meta->rect_params.height);
      dets.push_back({obj_meta->obj_label, obj_meta->confidence, bbox});
    }

    // 清空检测结果
    for (l_obj = frame_meta->obj_meta_list; l_obj != NULL;)
    {
      obj_meta = (NvDsObjectMeta *) (l_obj->data);
      // 删框子
      l_obj = l_obj->next;
      nvds_remove_obj_meta_from_frame(frame_meta, obj_meta);
    }
    
    cv::Mat img;
    if (this_plugin->manual) {
      this_plugin->manual = false;

      send_analysis = true;
      analysisdata["cmd"] = "manual_measure";
      analysisdata["shooter"] = "doctor";
      analysisdata["imagepath"] = this_plugin->img_path;
      analysisdata["measure"].resize(0);

      MResult mresult;
      std::map<std::string, std::string> measure_name_dict;
      img = cv::imread(this_plugin->img_path);
      this_plugin->obstetrics_pipeline->manualmeasure(mresult, this_plugin->measure_name, measure_name_dict, img);
      if (mresult.flag) {
        for (auto biological_indicators : mresult.biological_indicators) {
          Json::Value measuredata;
          measuredata["name"] = measure_name_dict[biological_indicators.first];
          measuredata["value"] = biological_indicators.second;
          // 画线
          if (mresult.lines.size()!=0) {
            for (auto line : mresult.lines) {
              if (biological_indicators.first.compare(line.first) == 0) {
                measuredata["line"]["x1"] = line.second.first.x;
                measuredata["line"]["y1"] = line.second.first.y;
                measuredata["line"]["x2"] = line.second.second.x;
                measuredata["line"]["y2"] = line.second.second.y;
              }
            }
          }
          analysisdata["measure"].append(measuredata);
        }
      }
    } 
    else {
      img = bgr_mat.clone();
      std::vector<PResult> presults;
      this_plugin->obstetrics_pipeline->apply(presults, dets, reset, img);

      for (auto det : dets) {
        attach_objectmeta(this_plugin, frame_meta, det.rect, det.name, det.prob, true, {0, 1, 0, 1}, {0, 1, 0, 1});
      }

      if (presults.size() != 0) {
        send_analysis = true;
      }

      for (int i=0; i<presults.size(); i++) {
        PResult presult = presults[i];
        Json::Value planedata;
        if (presult.has_spl) {
          SPlane spl = presult.spl;
          planedata["name"] = spl.name;
          planedata["score"] = spl.score;
          planedata["threshold"] = spl.threshold;
          planedata["sum_score"] = spl.sum_score;
          planedata["position"]["x"] = spl.organ.rect.x;
          planedata["position"]["y"] = spl.organ.rect.y;
          planedata["position"]["width"] = spl.organ.rect.width;
          planedata["position"]["height"] = spl.organ.rect.height;

          if (spl.flag) {
            attach_objectmeta(this_plugin, frame_meta, spl.organ.rect, spl.name, spl.score, true, {1, 0, 0, 1}, {1, 0, 0, 1});
            planedata["standard"] = 1;
          } else {
            planedata["standard"] = 0;
          }
          
          // 解剖结构信息
          std::map<std::string, std::vector<Detect>> anatomy_dets = spl.anatomy_dets;
          std::map<std::string, int> anatomy_num = spl.anatomy_num;
          std::map<std::string, int>::iterator iter;
          for (iter = anatomy_num.begin(); iter != anatomy_num.end(); iter++) {
            // std::cout << iter->first << ": " << iter->second << std::endl;
            for (int i=0; i<iter->second; i++) {
              Json::Value anatomydata;
              anatomydata["name"] = iter->first;
              if (i<anatomy_dets[iter->first].size()) {
                anatomydata["detected"] = 1;
                anatomydata["position"]["x"] = anatomy_dets[iter->first][i].rect.x;
                anatomydata["position"]["y"] = anatomy_dets[iter->first][i].rect.y;
                anatomydata["position"]["width"] = anatomy_dets[iter->first][i].rect.width;
                anatomydata["position"]["height"] = anatomy_dets[iter->first][i].rect.height;
              } else {
                anatomydata["detected"] = 0;
                anatomydata["position"]["x"] = -1;
                anatomydata["position"]["y"] = -1;
                anatomydata["position"]["width"] = -1;
                anatomydata["position"]["height"] = -1;
              }
              planedata["anatomy"].append(anatomydata);
            }
          }
        }
        
        // 测量信息
        if (presult.has_measure) {
          MResult mresult = presult.mresult;
          if (mresult.flag) {
            for (auto biological_indicators : mresult.biological_indicators) {
              Json::Value measuredata;
              measuredata["name"] = presult.measure_name_dict[biological_indicators.first];
              measuredata["measured"] = true;
              measuredata["value"] = biological_indicators.second;
              // 画线
              if (mresult.lines.size()!=0) {
                for (auto line : mresult.lines) {
                  if (biological_indicators.first.compare(line.first) == 0) {
                    measuredata["line"]["x1"] = line.second.first.x;
                    measuredata["line"]["y1"] = line.second.first.y;
                    measuredata["line"]["x2"] = line.second.second.x;
                    measuredata["line"]["y2"] = line.second.second.y;
                  }
                }
              }
              // 画椭圆
              if (mresult.ellipses.size()!=0) {
                for (auto ellipse : mresult.ellipses) {
                  if (biological_indicators.first.compare(ellipse.first) == 0) {
                    measuredata["ellipse"]["cx"] = ellipse.second.center.x;
                    measuredata["ellipse"]["cy"] = ellipse.second.center.y;
                    measuredata["ellipse"]["long_axis"] = ellipse.second.size.height;
                    measuredata["ellipse"]["short_axis"] = ellipse.second.size.width;
                    measuredata["ellipse"]["angle"] = ellipse.second.angle;
                  }
                }
              }
              planedata["measure"].append(measuredata);
            }
          } else {
            for (auto iter=presult.measure_name_dict.begin(); iter!=presult.measure_name_dict.end(); iter++) {
              Json::Value measuredata;
              measuredata["name"] = iter->second;
              measuredata["measured"] = false;
              measuredata["value"] = -1;
              planedata["measure"].append(measuredata);
            }
          }
        }

        // 标准面发送条件
        if (presult.need_send) {
          send_attention = true;
        }

        // 当标准面可以识别, 但测量不出来, 标准面也不标准, 保证保存的标准面一定包含测量
        // if (presult.has_spl && presult.spl.flag && presult.has_measure && (presult.mresult.flag==0)) {
        //   planedata["threshold"] = 0.0f;
        //   send_attention = false;
        // }

        analysisdata["planes"].append(planedata);
      }
      
      // 跳帧发送结果
      if (this_plugin->tmp_skip_frame!=this_plugin->skip_frame) {
        send_analysis = false;
      } else {
        this_plugin->tmp_skip_frame = 0;
      }
      this_plugin->tmp_skip_frame += 1;
      
    }

    // 发送json
    std::string str_json;
    // analysis
    if (send_analysis || send_attention) {
      user_meta = nvds_acquire_user_meta_from_pool(batch_meta);
      str_json = analysisdata.toStyledString();
      std::cout << str_json << std::endl;
      user_meta->user_meta_data = (void *)set_metadata_ptr(const_cast<char *>(str_json.c_str()));
      user_meta->base_meta.meta_type = user_meta_type;
      user_meta->base_meta.copy_func = (NvDsMetaCopyFunc)copy_user_meta;
      user_meta->base_meta.release_func = (NvDsMetaReleaseFunc)release_user_meta;
      nvds_add_user_meta_to_frame(frame_meta, user_meta);
    }
    // attention
    if (send_attention) {
      cv::imwrite(img_path, bgr_mat);
      // if (attentiondata.isMember("planes")) {
      //   attentiondata.removeMember("planes");
      // }
      user_meta = nvds_acquire_user_meta_from_pool(batch_meta);
      str_json = attentiondata.toStyledString();
      std::cout << str_json << std::endl;
      user_meta->user_meta_data = (void *)set_metadata_ptr(const_cast<char *>(str_json.c_str()));
      user_meta->base_meta.meta_type = user_meta_type;
      user_meta->base_meta.copy_func = (NvDsMetaCopyFunc)copy_user_meta;
      user_meta->base_meta.release_func = (NvDsMetaReleaseFunc)release_user_meta;
      nvds_add_user_meta_to_frame(frame_meta, user_meta);
    }

  }

  flow_ret = GST_FLOW_OK;

error:
  nvds_set_output_system_timestamp (inbuf, GST_ELEMENT_NAME (this_plugin));
  gst_buffer_unmap (inbuf, &in_map_info);
  return flow_ret;
}

void attach_objectmeta(_GstThisPlugin *this_plugin, NvDsFrameMeta *frame_meta, cv::Rect rect, std::string text, gfloat score, bool display, 
                       NvOSD_ColorParams rect_color, NvOSD_ColorParams text_color)
{
  static gchar font_name[] = "Serif";
  gint result_class_id = 0;

  NvDsObjectMeta *obj_meta;
  NvDsBatchMeta *batch_meta = frame_meta->base_meta.batch_meta;

  nvds_acquire_meta_lock (batch_meta);
  // if (object_info.attributes.size () == 0 ||
  //         object_info.label.length() == 0)
  //   return;

  /* Attach only one object in the meta since this is a full frame
     * classification. */
  obj_meta = nvds_acquire_obj_meta_from_pool (batch_meta);

  obj_meta->unique_component_id = this_plugin->unique_id;
  obj_meta->confidence = score;

  /* This is an untracked object. Set tracking_id to -1. */
  obj_meta->object_id = UNTRACKED_OBJECT_ID;
  obj_meta->class_id = result_class_id;

  NvOSD_RectParams & rect_params = obj_meta->rect_params;
  NvOSD_TextParams & text_params = obj_meta->text_params;

  /* Assign bounding box coordinates. These can be overwritten if tracker
    * component is present in the pipeline */
  rect_params.left = rect.x;
  rect_params.top = rect.y;
  rect_params.width = rect.width;
  rect_params.height = rect.height;

   /* Preserve original positional bounding box coordinates of detector in the
    * frame so that those can be accessed after tracker */
  obj_meta->detector_bbox_info.org_bbox_coords.left = rect_params.left;
  obj_meta->detector_bbox_info.org_bbox_coords.top = rect_params.top;
  obj_meta->detector_bbox_info.org_bbox_coords.width = rect_params.width;
  obj_meta->detector_bbox_info.org_bbox_coords.height = rect_params.height;

  /* Semi-transparent yellow background. */
  rect_params.has_bg_color = 0;
  rect_params.bg_color = (NvOSD_ColorParams) {
  1, 1, 0, 0.4};
  /* Red border of width 1. */
  if (display) rect_params.border_width = 1;
  else rect_params.border_width = 0;
  // rect_params.border_color = (NvOSD_ColorParams) {
  // 1, 1, 1, 1};
  rect_params.border_color = rect_color;

  strcpy(obj_meta->obj_label, "characters");
  /* display_text requires heap allocated memory. */
  text_params.display_text = g_strdup (text.c_str());  
  /* Display text above the left top corner of the object. */
  text_params.x_offset = rect_params.left;
  text_params.y_offset = rect_params.top;
  /* Set black background for the text. */
  text_params.set_bg_clr = 0;
  text_params.text_bg_clr = (NvOSD_ColorParams) {
  0, 0, 0, 1};
  /* Font face, size and color. */
  text_params.font_params.font_name = font_name;
  if (display) text_params.font_params.font_size = 11;
  else text_params.font_params.font_size = 0;
  // text_params.font_params.font_color = (NvOSD_ColorParams) {
  // 1, 1, 1, 1};
  text_params.font_params.font_color = text_color;

  nvds_add_obj_meta_to_frame (frame_meta, obj_meta, obj_meta);
  nvds_release_meta_lock (batch_meta);

}

void *set_metadata_ptr(gchar* json)
{
  int i = 0;
  gchar *user_metadata = (gchar*)g_malloc0(JSON_MAX_SIZE);
  strncpy(user_metadata, json, strlen(json));
  return (void *)user_metadata;
}

/* copy function set by user. "data" holds a pointer to NvDsUserMeta*/
static gpointer copy_user_meta(gpointer data, gpointer user_data)
{
  NvDsUserMeta *user_meta = (NvDsUserMeta *)data;
  gchar *src_user_metadata = (gchar*)user_meta->user_meta_data;
  gchar *dst_user_metadata = (gchar*)g_malloc0(JSON_MAX_SIZE);
  memcpy(dst_user_metadata, src_user_metadata, JSON_MAX_SIZE);
  return (gpointer)dst_user_metadata;
}

/* release function set by user. "data" holds a pointer to NvDsUserMeta*/
static void release_user_meta(gpointer data, gpointer user_data)
{
  NvDsUserMeta *user_meta = (NvDsUserMeta *) data;
  if(user_meta->user_meta_data) {
    g_free(user_meta->user_meta_data);
    user_meta->user_meta_data = NULL;
  }
}

/**
 * Boiler plate for registering a plugin and an element.
 */
static gboolean
this_plugin_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_this_plugin_debug, "obstetrics", 0,
      "obstetrics plugin");

  return gst_element_register (plugin, "obstetrics", GST_RANK_PRIMARY,
      GST_TYPE_THISPLUGIN);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    nvdsgst_obstetrics,
    DESCRIPTION, this_plugin_plugin_init, DS_VERSION, LICENSE, BINARY_PACKAGE, URL)
