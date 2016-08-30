LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := dtls-prebuilt
LOCAL_SRC_FILES := $(MY_COAP_DTLS_OUTPUT_ROOT)/lib/libtinydtls.a
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := coap-prebuilt
LOCAL_SRC_FILES := $(MY_COAP_DTLS_OUTPUT_ROOT)/lib/libcoap-1.a
LOCAL_STATIC_LIBRARIES := dtls-prebuilt
include $(PREBUILT_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE    := jl_coap_dtls
LOCAL_SRC_FILES := android/coap_api_jni.c
LOCAL_STATIC_LIBRARIES := coap-prebuilt
LOCAL_C_INCLUDES += $(MY_COAP_INC_ROOT)
LOCAL_C_INCLUDES += android/coap_api_jni.h
LOCAL_C_INCLUDES += android/coap_list.h
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)
