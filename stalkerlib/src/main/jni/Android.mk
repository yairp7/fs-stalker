LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog

LOCAL_MODULE := libfsmonitor
LOCAL_MODULE_TAGS := optional

# LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_EXTENSION := .c

ifeq ($(TARGET_SIMULATOR),true)
$(error STLPort not suitable for the simulator! $(LOCAL_PATH))
else

ifdef LOCAL_NDK_VERSION
stlport_NDK_VERSION_ROOT := $(HISTORICAL_NDK_VERSIONS_ROOT)/android-ndk-r$(LOCAL_NDK_VERSION)/$(BUILD_OS)/platforms/android-$(LOCAL_SDK_VERSION)/arch-$(TARGET_ARCH)
LOCAL_C_INCLUDES := \
	$(stlport_NDK_VERSION_ROOT) \
	external/stlport/stlport \
	$(LOCAL_C_INCLUDES)
else
LOCAL_C_INCLUDES := \
	external/stlport/stlport \
	$(LOCAL_C_INCLUDES)
endif

endif

LOCAL_SRC_FILES := android-lib.c utils.c dir.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -D_GLIBCXX_PERMIT_BACKWARD_HASH

LOCAL_LDFLAGS += -shared

# compile library
include $(BUILD_SHARED_LIBRARY)

#################################

include $(CLEAR_VARS)

LOCAL_LDLIBS := -llog

LOCAL_MODULE := fs-monitor
LOCAL_MODULE_TAGS := optional

LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_C_EXTENSION := .c

LOCAL_SRC_FILES := main.c utils.c

LOCAL_C_INCLUDES := $(LOCAL_PATH)/include

LOCAL_CFLAGS := -D_GLIBCXX_PERMIT_BACKWARD_HASH -DSUPPORT_ANDROID_21

LOCAL_LDFLAGS += -fPIC -pie

# compile executable
include $(BUILD_EXECUTABLE)