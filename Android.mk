# This file is generated by androgenizer for:
# [ ] NDK
# [x] system

LOCAL_PATH:=$(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE:=librngd

LOCAL_MODULE_TAGS:=optional 

LOCAL_SRC_FILES := \
	fips.c

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H

LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)
include $(CLEAR_VARS)

LOCAL_MODULE:=rngd

LOCAL_MODULE_TAGS:=optional 

LOCAL_SRC_FILES := \
	rngd.c \
	rngd_entsource.c \
	rngd_linux.c \
	util.c \
	rngd_rdrand.c \
	rdrand_asm.S

LOCAL_SHARED_LIBRARIES:=\
	librngd

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H

LOCAL_PRELINK_MODULE := false
include $(BUILD_EXECUTABLE)
include $(CLEAR_VARS)

LOCAL_MODULE:=rngtest

LOCAL_MODULE_TAGS:=optional 

LOCAL_SRC_FILES := \
	stats.c \
	rngtest.c

LOCAL_SHARED_LIBRARIES:=\
	librngd

LOCAL_CFLAGS := \
	-DHAVE_CONFIG_H

LOCAL_PRELINK_MODULE := false
include $(BUILD_EXECUTABLE)

