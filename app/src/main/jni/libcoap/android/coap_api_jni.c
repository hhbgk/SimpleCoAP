#include "coap_api_jni.h"

int flags = 0;

static unsigned char _token_data[8];
str the_token = { 0, _token_data };

typedef unsigned char method_t;
method_t method = 1;                    /* the method we are using in our requests */

static str payload = { 0, NULL };       /* optional payload to send */

unsigned char msgtype = COAP_MESSAGE_CON; /* usually, requests are sent confirmable */

coap_block_t block = { .num = 0, .m = 0, .szx = 6 };

unsigned int wait_seconds = 10;//90;         /* default timeout in seconds */
coap_tick_t max_wait;                   /* global timeout (changed by set_timeout()) */

unsigned int obs_seconds = 30;          /* default observe time */
coap_tick_t obs_wait = 0;               /* timeout for current subscription */
int observe = 0;                        /* set to 1 if resource is being observed */

#define min(a,b) ((a) < (b) ? (a) : (b))

static coap_list_t *optlist = NULL;

#define JNI_CLASS_IJKPLAYER     "com/hhbgk/coap/api/CoAPClient"
#define NELEM(x) ((int) (sizeof(x) / sizeof((x)[0])))

#ifdef __GNUC__
    #define UNUSED_PARAM __attribute__ ((unused))
#else /* not a GCC */
    #define UNUSED_PARAM
#endif /* GCC */

static JavaVM* g_jvm = NULL;
static jobject g_obj = NULL;
static coap_client client;
static jmethodID on_data_rcv_method_id;

static void *msg_runnable(void *);

static inline void set_timeout(coap_tick_t *timer, const unsigned int seconds) {
    *timer = seconds * 1000;
}

static int append_to_output(struct coap_context_t *ctx, coap_pdu_t *received, const unsigned char *data, size_t len) {
	short token = -1;
	if(received && received->hdr->token_length >= 2){
		token = (short)(((unsigned char)received->hdr->token[1]) << 8 |((unsigned char)received->hdr->token[0]));
    }
    //logw("append_to_output:mid=%d, len=%d, data=%s, token=%d",received->hdr->id, len, data, token);
    JNIEnv *env = NULL;
    if((*g_jvm)->AttachCurrentThread(g_jvm, &env, NULL) != JNI_OK){
        loge("%s: AttachCurrentThread() failed", __FUNCTION__);
        return -1;
    }

    jclass clazz = (*env)->GetObjectClass(env, g_obj);
    if(clazz == NULL) {
        if ((*env)->ThrowNew(env, "java/lang/NullPointerException", "Unable to find exception class") != JNI_OK) {
            return -1;
        }
    }

    jbyteArray jArray = (*env)->NewByteArray(env, len);
    (*env)->SetByteArrayRegion(env, jArray, 0, len, (jbyte*)data);
    (*env)->CallVoidMethod(env, g_obj, on_data_rcv_method_id, ctx->message_id, token, jArray);
    (*env)->DeleteLocalRef(env, jArray);

    return 0;
}

static coap_tid_t clear_obs(coap_context_t *ctx,const coap_endpoint_t *local_interface, const coap_address_t *remote) {
	logd("%s", __func__);
    coap_pdu_t *pdu;
    coap_list_t *option;
    coap_tid_t tid = COAP_INVALID_TID;
    unsigned char buf[2];

    /* create bare PDU w/o any option  */
    pdu = coap_pdu_init(msgtype, COAP_REQUEST_GET, coap_new_message_id(ctx), COAP_MAX_PDU_SIZE);

    if (!pdu) {
        return tid;
    }

    if (!coap_add_token(pdu, the_token.length, the_token.s)) {
        loge("cannot add token");
        goto error;
    }

    for (option = optlist; option; option = option->next ) {
        coap_option *o = (coap_option *)(option->data);
        if (COAP_OPTION_KEY(*o) == COAP_OPTION_URI_HOST) {
            if (!coap_add_option(pdu, COAP_OPTION_KEY(*o), COAP_OPTION_LENGTH(*o),COAP_OPTION_DATA(*o))) {
                goto error;
            }
            break;
        }
    }

    if (!coap_add_option(pdu,COAP_OPTION_OBSERVE,coap_encode_var_bytes(buf, COAP_OBSERVE_CANCEL),buf)) {
        coap_log(LOG_CRIT, "cannot add option Observe: %u", COAP_OBSERVE_CANCEL);
        goto error;
    }

    for (option = optlist; option; option = option->next ) {
        coap_option *o = (coap_option *)(option->data);
        switch (COAP_OPTION_KEY(*o)) {
            case COAP_OPTION_URI_PORT :
            case COAP_OPTION_URI_PATH :
            case COAP_OPTION_URI_QUERY :
                if (!coap_add_option (pdu,COAP_OPTION_KEY(*o),COAP_OPTION_LENGTH(*o),COAP_OPTION_DATA(*o))) {
                    goto error;
                }
                break;
            default:
                ;
        }
    }

    if (pdu->hdr->type == COAP_MESSAGE_CON)
        tid = coap_send_confirmed(ctx, local_interface, remote, pdu);
    else
        tid = coap_send(ctx, local_interface, remote, pdu);

    if (tid == COAP_INVALID_TID) {
        logi("clear_obs: error sending new request");
        coap_delete_pdu(pdu);
    } else if (pdu->hdr->type != COAP_MESSAGE_CON)
        coap_delete_pdu(pdu);

    return tid;
error:

    coap_delete_pdu(pdu);
    return tid;
}

static int resolve_address(const str *server, struct sockaddr *dst) {

  struct addrinfo *res, *ainfo;
  struct addrinfo hints;
  static char addrstr[256];
  int error, len=-1;

  memset(addrstr, 0, sizeof(addrstr));
  if (server->length)
    memcpy(addrstr, server->s, server->length);
  else
    memcpy(addrstr, "localhost", 9);

  memset ((char *)&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_family = AF_UNSPEC;

  error = getaddrinfo(addrstr, NULL, &hints, &res);

  if (error != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
    loge("getaddrinfo: %s\n", gai_strerror(error));
    return error;
  }

  for (ainfo = res; ainfo != NULL; ainfo = ainfo->ai_next) {
    switch (ainfo->ai_family) {
    case AF_INET6:
    case AF_INET:
      len = ainfo->ai_addrlen;
      memcpy(dst, ainfo->ai_addr, len);
      goto finish;
    default:
      ;
    }
  }

 finish:
  freeaddrinfo(res);
  return len;
}

static void message_handler(struct coap_context_t *ctx, const coap_endpoint_t *local_interface, const coap_address_t *remote,
                coap_pdu_t *sent,
                coap_pdu_t *received,
                const coap_tid_t id UNUSED_PARAM) {
    logd("%s",__func__);
//    logi("%s: tid=%d, received id=%d, sent id=%d",__func__, id, received->hdr->id,sent->hdr->id);
	coap_pdu_t *pdu = NULL;
	coap_opt_t *block_opt;
	coap_opt_iterator_t opt_iter;
	unsigned char buf[4];
	coap_list_t *option;
	size_t len;
	unsigned char *databuf;
	coap_tid_t tid;

	int type = received->hdr->type;
	logw("type=%s", type==COAP_MESSAGE_CON ? "CON":type==COAP_MESSAGE_ACK?"ACK":type==COAP_MESSAGE_NON?"NON":type==COAP_MESSAGE_RST?"RST":"Unknown");

	if (received->hdr->type == COAP_MESSAGE_RST) {
		logw("got RST\n");
		return;
	}

    /* output the received data, if any */
    if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {

        /* set obs timer if we have successfully subscribed a resource */
        if (sent && coap_check_option(received, COAP_OPTION_SUBSCRIPTION, &opt_iter)) {
            logi("observation relationship established, set timeout to %d\n", obs_seconds);
            set_timeout(&obs_wait, obs_seconds);
            observe = 1;
        }

		/* There is no block option set, just read the data and we are done. */
		if (coap_get_data(received, &len, &databuf))
			append_to_output(ctx, received, databuf, len);
    } else {      /* no 2.05 */
        /* check if an error was signaled and output payload if so */
        if (COAP_RESPONSE_CLASS(received->hdr->code) >= 4) {
            loge("no %d.%02d", (received->hdr->code >> 5), received->hdr->code & 0x1F);
            if (coap_get_data(received, &len, &databuf)) {
                char receive_data[len];
                strncpy(receive_data, databuf, len);
                loge("%s", databuf);
            }
        }
    }

    /* finally send new request, if needed */
    if (pdu && coap_send(ctx, local_interface, remote, pdu) == COAP_INVALID_TID) {
        logi("message_handler: error sending response");
    }
    coap_delete_pdu(pdu);
}

/**
 * Calculates decimal value from hexadecimal ASCII character given in
 * @p c. The caller must ensure that @p c actually represents a valid
 * heaxdecimal character, e.g. with isxdigit(3).
 *
 * @hideinitializer
 */
#define hexchar_to_dec(c) ((c) & 0x40 ? ((c) & 0x0F) + 9 : ((c) & 0x0F))

/**
 * Decodes percent-encoded characters while copying the string @p seg
 * of size @p length to @p buf. The caller of this function must
 * ensure that the percent-encodings are correct (i.e. the character
 * '%' is always followed by two hex digits. and that @p buf provides
 * sufficient space to hold the result. This function is supposed to
 * be called by make_decoded_option() only.
 *
 * @param seg     The segment to decode and copy.
 * @param length  Length of @p seg.
 * @param buf     The result buffer.
 */
static void decode_segment(const unsigned char *seg, size_t length, unsigned char *buf) {
  while (length--) {

    if (*seg == '%') {
      *buf = (hexchar_to_dec(seg[1]) << 4) + hexchar_to_dec(seg[2]);

      seg += 2; length -= 2;
    } else {
      *buf = *seg;
    }

    ++buf; ++seg;
  }
}

/**
 * Runs through the given path (or query) segment and checks if
 * percent-encodings are correct. This function returns @c -1 on error
 * or the length of @p s when decoded.
 */
static int check_segment(const unsigned char *s, size_t length) {
  size_t n = 0;

  while (length) {
    if (*s == '%') {
      if (length < 2 || !(isxdigit(s[1]) && isxdigit(s[2])))
        return -1;

      s += 2;
      length -= 2;
    }

    ++s; ++n; --length;
  }

  return n;
}

static int cmdline_input(char *text, str *buf) {
  int len;
  len = check_segment((unsigned char *)text, strlen(text));

  if (len < 0)
    return 0;

  buf->s = (unsigned char *)coap_malloc(len);
  if (!buf->s)
    return 0;

  buf->length = len;
  decode_segment((unsigned char *)text, strlen(text), buf->s);
  return 1;
}

static ssize_t cmdline_read_user(char *arg, unsigned char *buf, size_t maxlen) {
  size_t len = strnlen(arg, maxlen);
  if (len) {
    memcpy(buf, arg, len);
    return len;
  }
  return -1;
}

static ssize_t cmdline_read_key(char *arg, unsigned char *buf, size_t maxlen) {
  size_t len = strnlen(arg, maxlen);
  if (len) {
    memcpy(buf, arg, len);
    return len;
  }
  return -1;
}

static coap_context_t *get_context(const char *node, const char *port, int secure) {
    //logi("%s: node %s, port %s, secure %d", __func__, node, port, secure);
    coap_context_t *ctx = NULL;
    int s;
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int ep_type;

    ctx = coap_new_context(NULL);
    if (!ctx) {
        loge("coap_new_context: error.");
        return NULL;
    }

    ep_type = secure ? COAP_ENDPOINT_DTLS : COAP_ENDPOINT_NOSEC;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Coap uses UDP */
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST | AI_NUMERICSERV;//| AI_ALL;

    s = getaddrinfo(node, port, &hints, &result);
    //logw("getaddrinfo: result code:%d,%s, port:%s", s, node, port);
    if ( s != 0 ) {
        loge("getaddrinfo: %s", gai_strerror(s));
        return NULL;
    }

    /* iterate through results until success */
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        coap_address_t addr;
        coap_endpoint_t *endpoint;

        if (rp->ai_addrlen <= sizeof(addr.addr)) {
            coap_address_init(&addr);
            addr.size = rp->ai_addrlen;
            //addr.addr.sin.sin_port = htons(5683);
            memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);

            endpoint = coap_new_endpoint(&addr, ep_type);
            if (endpoint) {
                coap_attach_endpoint(ctx, endpoint);
                goto finish;
            } else {
                loge("cannot create endpoint\n");
                continue;
            }
        }
    }

    loge("no context available for interface '%s'\n", node);
    coap_free_context(ctx);
    ctx = NULL;

    finish:
    freeaddrinfo(result);
    return ctx;
}

static void jni_native_init(JNIEnv *env, jobject thiz){
    //保存全局JVM以便在子线程中使用
    (*env)->GetJavaVM(env,&g_jvm);
    //不能直接赋值(g_obj = thiz)
    g_obj = (*env)->NewGlobalRef(env, thiz);
    jclass clazz = (*env)->GetObjectClass(env, thiz);
    if(clazz == NULL) {
        if ((*env)->ThrowNew(env, "java/lang/NullPointerException", "Unable to find exception class") != JNI_OK) {
            loge("Unable to find exception class");
            return;
        }
    }

    on_data_rcv_method_id = (*env)->GetMethodID(env, clazz, "onDataReceived", "(SS[B)V");
    if(!on_data_rcv_method_id){
        loge("The calling class does not implement all necessary interface methods");
    }
}

static jboolean jni_coap_setup(JNIEnv *env, jobject thiz, jstring str_ip, jboolean isSecure) {
	logi("%s", __func__);

	static char addr[INET6_ADDRSTRLEN];
	void *addrptr = NULL;
	int result = -1;
	coap_pdu_t  *request;
	static str server;
	unsigned short port = COAP_DEFAULT_PORT;
	char port_str[NI_MAXSERV] = "5683";
	int opt, res;
	coap_log_t log_level = 6;//LOG_WARNING;
	coap_tid_t tid = COAP_INVALID_TID;
	unsigned char user[MAX_USER], key[MAX_KEY];
	ssize_t user_length = 0, key_length = 0;

	coap_dtls_set_log_level(log_level);
	coap_set_log_level(log_level);

	const char *c_ip = (*env)->GetStringUTFChars(env, str_ip, NULL);
	server.s = c_ip;
	server.length = strlen(c_ip);
	/* resolve destination address where server should be sent */
	res = resolve_address(&server, &client.dst.addr.sa);
	if (res < 0) {
	loge("failed to resolve address\n");
		goto fail;
	}
	client.dst.size = res;
	client.dst.addr.sin.sin_port = htons(port);

	/* add Uri-Host if server address differs from uri.host */
	if(client.dst.addr.sa.sa_family != AF_INET){
		goto fail;
	}
	client.secure = isSecure;
	/* create context for IPv4 */
	client.ctx = get_context("0.0.0.0", port_str, client.secure);
	if (!client.ctx) {
		loge("cannot create context\n");
		goto err;
	}

    //Add secure support
    if(client.secure){
        user_length = cmdline_read_user("Client_identity", user, MAX_USER);
        key_length = cmdline_read_key("secretPSK", key, MAX_KEY);

        if ((user_length < 0) || (key_length < 0)) {
            loge("Invalid user name or key specified\n");
            goto err;
        }

        if (user_length > 0) {
            coap_keystore_item_t *psk;
            psk = coap_keystore_new_psk(NULL, 0, user, (size_t)user_length, key, (size_t)key_length, 0);
            if (!psk || !coap_keystore_store_item(client.ctx->keystore, psk, NULL)) {
                logw(LOG_WARNING, "cannot store key\n");
            }
        }
    }

    coap_register_option(client.ctx, COAP_OPTION_BLOCK2);
    coap_register_response_handler(client.ctx, message_handler);

    ////Create thread for listening remote device messages
   int thread_ret = pthread_create(&client.msg_thread, NULL, msg_runnable, NULL);
    if(0 != thread_ret) {
        loge("can't create thread");
        goto fail;
    }

    return JNI_TRUE;

err:
    loge("-------error-------");
    coap_delete_list(optlist);
    coap_free_context( client.ctx );
fail:
    loge("-------fail-------");
    return JNI_FALSE;
}

static jshort jni_coap_request(JNIEnv *env, jobject thiz, jint method, jshort token, jstring url, jobjectArray stringArray, jstring text) {
	logd("%s", __func__);
	jlong data[2] = { 0 };
	coap_pdu_t *pdu;
	coap_tid_t tid = COAP_INVALID_TID;

	if (!(pdu = coap_new_pdu()))
		goto ERR_OUT;

	pdu->hdr->type = msgtype;
	pdu->hdr->id = coap_new_message_id(client.ctx);
	pdu->hdr->code = method;

	int length = (*env)->GetStringLength(env, url);
	if (NULL == url || length == 0) {
		goto ERR_OUT;
	}

	if(token != NULL){//Add token
		logi("token=%d, token size=%d", token, sizeof(token));
		pdu->hdr->token_length = sizeof(token);
		if (!coap_add_token(pdu, sizeof(token), &token)) {
			loge("cannot add token to request");
		}
	}

	///Add observer
	coap_add_option(pdu, COAP_OPTION_SUBSCRIPTION, 0, NULL);

	///Add URI
	const char *c_url = (*env)->GetStringUTFChars(env, url, NULL);
	logi("url:%s %s", method == 1 ? "GET" : method == 2 ? "POST" : "unknown", c_url);
	coap_add_option(pdu, COAP_OPTION_URI_PATH, strlen(c_url), c_url);
	(*env)->ReleaseStringUTFChars(env, url, c_url);

	if(stringArray != NULL){///Add query
		int i;
		int stringCount = (*env)->GetArrayLength(env, stringArray);
		for(i = 0; i < stringCount; i ++){
			jstring string = (jstring) ((*env)->GetObjectArrayElement(env, stringArray, i));
			const char *c_query = (*env)->GetStringUTFChars(env, string, NULL);
			coap_add_option ( pdu, COAP_OPTION_URI_QUERY,strlen(c_query), c_query);
			logi("c_query %d: %s",i, c_query);
			(*env)->ReleaseStringUTFChars(env, string, c_query);
		}
	}

	if(NULL != text ) {///Add payload
		length = (*env)->GetStringLength(env, text);
		if(length > 0){
			const char *c_text = (*env)->GetStringUTFChars(env, text, NULL);
			logi("Payload:%s", c_text);
			if(cmdline_input(c_text, &payload)){
				coap_add_data(pdu, payload.length, payload.s);
			} else{
				loge("Cannot add payload to request");
			}
			(*env)->ReleaseStringUTFChars(env, text, c_text);
		} else{
			logw("Length of payload is 0");
		}
	} else {
		logi("No payload");
	}

	if (pdu->hdr->type == COAP_MESSAGE_CON){
		tid = coap_send_confirmed(client.ctx, client.ctx->endpoint, &client.dst, pdu);
	} else {
		tid = coap_send(client.ctx, client.ctx->endpoint, &client.dst, pdu);
	}
	//logi("tid=%d, message_id=%d", tid, client.ctx->message_id);

	if (pdu->hdr->type != COAP_MESSAGE_CON || tid == COAP_INVALID_TID){
		coap_delete_pdu(pdu);
	}
	return (jshort) client.ctx->message_id;
ERR_OUT:
	return -1;
}

void *msg_runnable(void *arg) {
	logd("msg_runnable");
	pthread_detach(pthread_self());
	int result = -1;

	set_timeout(&max_wait, wait_seconds);
	coap_tick_t start, now;
	coap_ticks(&start);
	client.msg_thread_running = 1;
	while(client.msg_thread_running)
	{
		unsigned int wait_ms = observe ? min(obs_wait, max_wait) : max_wait;
		result = coap_run_once(client.ctx, wait_ms);
		if (result >= 0) {
			//logi("result=%d, max_wait=%d, obs_wait=%d, queue=%d, wait_ms=%d", result, max_wait, obs_wait, client.ctx->sendqueue, wait_ms);
			if ((unsigned int) result <= obs_wait) {
				obs_wait -= result;
				loge("obs_wait=%d", obs_wait);
			} else if (observe) {
				logi("clear observation relationship\n");
				clear_obs(client.ctx, client.ctx->endpoint, &client.dst); // FIXME: handle error case COAP_TID_INVALID

				/* make sure that the obs timer does not fire again */
				obs_wait = 0;
				observe = 0;
			}
			coap_ticks(&now);

			if ((unsigned int) result < max_wait) {
				loge("max_wait=%d", max_wait);
				max_wait -= result;
			}
		}
	}
	pthread_exit(NULL);
	logi("msg_runnable exit.....");
	return NULL;
}

static jboolean jni_coap_destroy(JNIEnv *env, jobject thiz){
    logi("%s", __func__);
    coap_delete_list(optlist);
    if(client.ctx){
        client.msg_thread_running = 0;
        coap_free_context( client.ctx );
    }
    return JNI_TRUE;
}
static JNINativeMethod g_methods[] = {
    { "nativeInit",         "()V",                                               (void *) jni_native_init },
    { "_setup",             "(Ljava/lang/String;Z)Z",                            (void *) jni_coap_setup },
    { "_request",       "(ISLjava/lang/String;[Ljava/lang/String;Ljava/lang/String;)S",      (void *) jni_coap_request },
    { "_destroy",           "()Z",                                               (void *) jni_coap_destroy },
};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved){
    JNIEnv* env = NULL;

    if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        return -1;
    }
    assert(env != NULL);

    // FindClass returns LocalReference
    jclass klass = (*env)->FindClass (env, JNI_CLASS_IJKPLAYER);
    if (klass == NULL) {
      return JNI_ERR;
    }
    (*env)->RegisterNatives(env, klass, g_methods, NELEM(g_methods) );

    return JNI_VERSION_1_4;
}
