SLIB:=LiveProxy

# make sure this is a relative path, it is used to build other relative paths
MODDIR=.

# relative path to root from here
ROOTREL=.

FFMPEGINC?=$(ROOTREL)/../ffmpeg
FFMPEGLIB?=$(ROOTREL)/../ffmpeg/bin -lavformat -lavcodec -lswscale -lavutil

MODINC= -I $(ROOTREL)/live/liveMedia/include -I $(ROOTREL)/live/groupsock/include -I $(ROOTREL)/live/UsageEnvironment/include -I $(ROOTREL)/live/BasicUsageEnvironment/include -I $(FFMPEGINC)
MODLIBS=-L$(FFMPEGLIB) ./live/liveMedia/libliveMedia.a ./live/groupsock/libgroupsock.a ./live/BasicUsageEnvironment/libBasicUsageEnvironment.a ./live/UsageEnvironment/libUsageEnvironment.a

include $(ROOTREL)/common.mk

all: init debug release
