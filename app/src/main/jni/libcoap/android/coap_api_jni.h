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

#include "coap_config.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<pthread.h>

#include "coap.h"
#include "coap_dtls.h"

#ifdef __ANDROID__

#include <android/log.h>
#define TAG "coap_api_jni"
#define logd(...)  ((void)__android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__))
#define logi(...)  ((void)__android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__))
#define logw(...)  ((void)__android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__))
#define loge(...)  ((void)__android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__))

#else

#define LOGI(...)
#define LOGW(...)
#define LOGE(...)

#endif//__ANDROID__

#define MAX_USER 128 /* Maximum length of a user name (i.e., PSK identity) in bytes. */
#define MAX_KEY   64 /* Maximum length of a key (i.e., PSK) in bytes. */
#define FLAGS_BLOCK 0x01

typedef struct coap_client{
  coap_context_t *ctx;
  coap_address_t dst;
  void *addrptr;
  int secure;

  pthread_t msg_thread;
  int msg_thread_running;
}coap_client;

#define TRACE

#endif //COAP_DTLS_COAP_API_JNI_H
