LOCAL_PATH := $(call my-dir)

MY_APP_JNI_ROOT := $(realpath $(LOCAL_PATH))

ifeq ($(TARGET_ARCH_ABI),armeabi)
MY_COAP_DTLS_OUTPUT_ROOT := $(realpath $(LOCAL_PATH)/libcoap/build/armv5)
MY_COAP_INC_ROOT := $(realpath $(MY_COAP_DTLS_OUTPUT_ROOT)/include/coap)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
MY_COAP_DTLS_OUTPUT_ROOT := $(realpath $(LOCAL_PATH)/libcoap/build/armv7a)
MY_COAP_INC_ROOT := $(realpath $(MY_COAP_DTLS_OUTPUT_ROOT)/include/coap)
endif

ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
MY_COAP_DTLS_OUTPUT_ROOT := $(realpath $(LOCAL_PATH)/libcoap/build/arm64)
MY_COAP_INC_ROOT := $(realpath $(MY_COAP_DTLS_OUTPUT_ROOT)/include/coap)
endif

include $(call all-subdir-makefiles)