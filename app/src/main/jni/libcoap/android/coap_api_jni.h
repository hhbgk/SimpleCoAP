//
// Created by bob on 16-8-3.
//

#ifndef COAP_DTLS_COAP_API_JNI_H
#define COAP_DTLS_COAP_API_JNI_H

#ifdef __ANDROID__
#include <android/log.h>
#include <jni.h>
#endif
#include <stdint.h>
#include <string.h>
#include <assert.h>

//#include "debug.h"

#ifdef __ANDROID__

#include <android/log.h>
#define TAG "coap_api_jni"
#define log_d(...)  ((void)__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))
#define log_i(...)  ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define log_w(...)  ((void)__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))
#define log_e(...)  ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

#else

#define LOGI(...)
#define LOGW(...)
#define LOGE(...)

#endif//__ANDROID__

#define TRACE

#endif //COAP_DTLS_COAP_API_JNI_H
