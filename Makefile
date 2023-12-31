################################################################################
# Copyright (c) 2017-2020, NVIDIA CORPORATION.  All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
#################################################################################
#enable this flag to use optimized dsexample plugin
#it can also be exported from command line

# CUDA_VER?=
# ifeq ($(CUDA_VER),)
#   $(error "CUDA_VER is not set")
# endif
CUDA_VER = 10.2
TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)
CXX:= g++

SRCS:= ${wildcard *.cpp}
SRCS+= ${wildcard src/*.cpp}
INCS:= $(wildcard *.h)
LIB:=libnvdsgst_obstetrics.so

NVDS_VERSION:=6.0

CFLAGS+= -fPIC -DDS_VERSION=\"6.0.0\" \
	 -I /usr/local/cuda-$(CUDA_VER)/include \
	 -I ../../includes \
	 -I include

GST_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/gst-plugins/
LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/

LIBS := -shared -Wl,-no-undefined \
	-L /usr/local/cuda-$(CUDA_VER)/lib64/ -lcudart -ldl \
	-ljsoncpp \
	-lnppc -lnppig -lnpps -lnppicc -lnppidei \
  	-lnvinfer \
	-luuid \
	-L$(LIB_INSTALL_DIR) -lnvdsgst_helper -lnvdsgst_meta -lnvds_meta -lnvbufsurface -lnvbufsurftransform\
	-Wl,-rpath,$(LIB_INSTALL_DIR)

OBJS:= $(SRCS:.cpp=.o)
ifeq ($(TARGET_DEVICE),aarch64)
	PKGS:= gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0 opencv4
else
	PKGS:= gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0 opencv
endif

CFLAGS+=$(shell pkg-config --cflags $(PKGS))
LIBS+=$(shell pkg-config --libs $(PKGS))

all: $(LIB)

%.o: %.cpp $(INCS) Makefile
	@echo $(CFLAGS)
	$(CXX) -c -o $@ $(CFLAGS) $<

$(LIB): $(OBJS) $(DEP) Makefile
	@echo $(CFLAGS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

install: $(LIB)
	cp -rv $(LIB) $(GST_INSTALL_DIR)

clean:
	$(foreach N, $(DIRS),make clean -C $(N);)
	@echo $(DEP)
	rm -rf $(OBJS) $(LIB)
