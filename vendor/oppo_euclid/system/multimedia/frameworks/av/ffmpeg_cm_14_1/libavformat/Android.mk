#
# Copyright (C) 2013 The Android-x86 Open Source Project
#
# Licensed under the GNU General Public License Version 2 or later.
# You may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.gnu.org/licenses/gpl.html
#

LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
FFMPEG_MULTILIB := 32
include $(LOCAL_PATH)/../android/build.mk

LOCAL_C_INCLUDES +=			\
	external/openssl/include	\
	external/zlib

LOCAL_SHARED_LIBRARIES +=		\
	libcrypto			\
	libssl				\
	libz                \
	libavutil           \
	libavcodec

LOCAL_MULTILIB := $(FFMPEG_MULTILIB)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
FFMPEG_MULTILIB := 64
include $(LOCAL_PATH)/../android/build.mk

LOCAL_C_INCLUDES +=			\
	external/openssl/include	\
	external/zlib

LOCAL_SHARED_LIBRARIES +=		\
	libcrypto			\
	libssl				\
	libz                \
	libavutil           \
	libavcodec

LOCAL_MULTILIB := $(FFMPEG_MULTILIB)
include $(BUILD_SHARED_LIBRARY)


#Add for ffmpeg omx
include $(CLEAR_VARS)
LOCAL_MODULE        :=  libavformat_vendor
LOCAL_PROPRIETARY_MODULE := true
LOCAL_INSTALLED_MODULE_STEM := libavformat.so
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MULTILIB      := 32
LOCAL_REQUIRED_MODULES := libavformat
LOCAL_SRC_FILES     := ../../../../../$(TARGET_OUT)/lib/libavformat.so
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE        :=  libavformat_vendor
LOCAL_PROPRIETARY_MODULE := true
LOCAL_INSTALLED_MODULE_STEM := libavformat.so
LOCAL_MODULE_CLASS  := SHARED_LIBRARIES
LOCAL_MODULE_SUFFIX := .so
LOCAL_STRIP_MODULE  := false
LOCAL_MULTILIB      := 64
LOCAL_REQUIRED_MODULES := libavformat
LOCAL_SRC_FILES     := ../../../../../$(TARGET_OUT)/lib64/libavformat.so
include $(BUILD_PREBUILT)