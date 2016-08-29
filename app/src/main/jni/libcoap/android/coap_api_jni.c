#include "coap_api_jni.h"

int flags = 0;

static unsigned char _token_data[8];
str the_token = { 0, _token_data };

typedef unsigned char method_t;
method_t method = 1;                    /* the method we are using in our requests */

static str payload = { 0, NULL };       /* optional payload to send */

unsigned char msgtype = COAP_MESSAGE_CON; /* usually, requests are sent confirmable */

coap_block_t block = { .num = 0, .m = 0, .szx = 6 };

unsigned int wait_seconds = 90;         /* default timeout in seconds */
coap_tick_t max_wait;                   /* global timeout (changed by set_timeout()) */

unsigned int obs_seconds = 30;          /* default observe time */
coap_tick_t obs_wait = 0;               /* timeout for current subscription */
int observe = 0;                        /* set to 1 if resource is being observed */

#define min(a,b) ((a) < (b) ? (a) : (b))

static coap_list_t *optlist = NULL;
/* Request URI.
 * TODO: associate the resources with transaction id and make it expireable */
static coap_uri_t uri;

/* reading is done when this flag is set */
static int ready = 0;

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

static int append_to_output(coap_pdu_t *received, const unsigned char *data, size_t len) {
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
    (*env)->CallVoidMethod(env, g_obj, on_data_rcv_method_id, (jint)received->hdr->id, token, jArray);
    (*env)->DeleteLocalRef(env, jArray);

    return 0;
}

static int order_opts(void *a, void *b) {
	logd("%s", __func__);
    coap_option *o1, *o2;

    if (!a || !b)
        return a < b ? -1 : 1;

    o1 = (coap_option *)(((coap_list_t *)a)->data);
    o2 = (coap_option *)(((coap_list_t *)b)->data);

    return (COAP_OPTION_KEY(*o1) < COAP_OPTION_KEY(*o2)) ? -1
           : (COAP_OPTION_KEY(*o1) != COAP_OPTION_KEY(*o2));
}

static coap_pdu_t *coap_new_request(coap_context_t *ctx,method_t m,coap_list_t **options,unsigned char *data,size_t length) {
	logd("%s", __func__);
    coap_pdu_t *pdu;
    coap_list_t *opt;

    if ( ! ( pdu = coap_new_pdu() ) )
        return NULL;

    pdu->hdr->type = msgtype;
    pdu->hdr->id = coap_new_message_id(ctx);
    pdu->hdr->code = m;

    pdu->hdr->token_length = the_token.length;
    if ( !coap_add_token(pdu, the_token.length, the_token.s)) {
        logi("cannot add token to request\n");
    }

    logi("payload length=%d", length);
    if (length) {
        if ((flags & FLAGS_BLOCK) == 0)
            coap_add_data(pdu, length, data);
        else
            coap_add_block(pdu, length, data, block.num, block.szx);
    }

    return pdu;
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

static inline int check_token(coap_pdu_t *received) {
	return received->hdr->token_length == the_token.length
			&& memcmp(received->hdr->token, the_token.s, the_token.length) == 0;
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

    switch(received->hdr->type){
        case COAP_MESSAGE_CON: /* confirmable message (requires ACK/RST) */
            logw("=====received type===CON");
            break;
        case COAP_MESSAGE_NON: /* non-confirmable message (one-shot message) */
            logw("=====received type===NON");
            break;
        case COAP_MESSAGE_ACK: /* used to acknowledge confirmable messages */
            logw("=====received type===ACK");
            break;
        case COAP_MESSAGE_RST: /* indicates error in received messages */
            logw("=====received type===RST");
            break;
    }

	if (received->hdr->type == COAP_MESSAGE_RST) {
		logw("got RST\n");
		return;
	}

    logw("COAP_RESPONSE_CLASS code:%d", COAP_RESPONSE_CLASS(received->hdr->code));
    /* output the received data, if any */
    if (COAP_RESPONSE_CLASS(received->hdr->code) == 2) {

        /* set obs timer if we have successfully subscribed a resource */
        if (sent && coap_check_option(received, COAP_OPTION_SUBSCRIPTION, &opt_iter)) {
            logi("observation relationship established, set timeout to %d\n", obs_seconds);
            set_timeout(&obs_wait, obs_seconds);
            observe = 1;
        }

        /* Got some data, check if block option is set. Behavior is undefined if both, Block1 and Block2 are present. */
        block_opt = coap_check_option(received, COAP_OPTION_BLOCK2, &opt_iter);
        logi("check block2: block_opt:%d", block_opt);
        if (block_opt) { /* handle Block2 */
            unsigned short blktype = opt_iter.type;

            /* TODO: check if we are looking at the correct block number */
            if (coap_get_data(received, &len, &databuf))
                append_to_output(received, databuf, len);

            if(COAP_OPT_BLOCK_MORE(block_opt)) {
                /* more bit is set */
                logi("found the M bit, block size is %u, block nr. %u\n",
                      COAP_OPT_BLOCK_SZX(block_opt),
                      coap_opt_block_num(block_opt));

                /* create pdu with request for next block */
                pdu = coap_new_request(ctx, method, NULL, NULL, 0); /* first, create bare PDU w/o any option  */
                if ( pdu ) {
                    /* add URI components from optlist */
                    for (option = optlist; option; option = option->next ) {
                        coap_option *o = (coap_option *)(option->data);
                        switch (COAP_OPTION_KEY(*o)) {
                            case COAP_OPTION_URI_HOST :
                            case COAP_OPTION_URI_PORT :
                            case COAP_OPTION_URI_PATH :
                            case COAP_OPTION_URI_QUERY :
                                coap_add_option (pdu,COAP_OPTION_KEY(*o),COAP_OPTION_LENGTH(*o),COAP_OPTION_DATA(*o));
                                break;
                            default:
                                ;     /* skip other options */
                        }
                    }

                    /* finally add updated block option from response, clear M bit */
                    /* blocknr = (blocknr & 0xfffffff7) + 0x10; */
                    logi("query block %d", (coap_opt_block_num(block_opt) + 1));
                    coap_add_option(pdu,blktype,coap_encode_var_bytes(buf,
                                                          ((coap_opt_block_num(block_opt) + 1) << 4) |
                                                          COAP_OPT_BLOCK_SZX(block_opt)), buf);

                    if (pdu->hdr->type == COAP_MESSAGE_CON)
                        tid = coap_send_confirmed(ctx, local_interface, remote, pdu);
                    else
                        tid = coap_send(ctx, local_interface, remote, pdu);

                    if (tid == COAP_INVALID_TID) {
                        logi("message_handler: error sending new request");
                        coap_delete_pdu(pdu);
                    } else {
                        set_timeout(&max_wait, wait_seconds);
                        if (pdu->hdr->type != COAP_MESSAGE_CON)
                            coap_delete_pdu(pdu);
                    }

                    return;
                }
            }
        } else { /* no Block2 option */
            block_opt = coap_check_option(received, COAP_OPTION_BLOCK1, &opt_iter);
            logi("check block1: block_opt:%d", block_opt);

            if (block_opt) { /* handle Block1 */
                block.szx = COAP_OPT_BLOCK_SZX(block_opt);
                block.num = coap_opt_block_num(block_opt);

                logi("found Block1, block size is %u, block nr. %u\n", block.szx, block.num);

                if (payload.length <= (block.num+1) * (1 << (block.szx + 4))) {
                    logi("upload ready\n");
                    ready = 1;
                    return;
                }

                /* create pdu with request for next block */
                pdu = coap_new_request(ctx, method, NULL, NULL, 0); /* first, create bare PDU w/o any option  */
                if (pdu) {

                    /* add URI components from optlist */
                    for (option = optlist; option; option = option->next ) {
                        coap_option *o = (coap_option *)(option->data);
                        switch (COAP_OPTION_KEY(*o)) {
                            case COAP_OPTION_URI_HOST :
                            case COAP_OPTION_URI_PORT :
                            case COAP_OPTION_URI_PATH :
                            case COAP_OPTION_CONTENT_FORMAT :
                            case COAP_OPTION_URI_QUERY :
                                coap_add_option (pdu,COAP_OPTION_KEY(*o),COAP_OPTION_LENGTH(*o),COAP_OPTION_DATA(*o));
                                break;
                            default:
                                ;     /* skip other options */
                        }
                    }

                    /* finally add updated block option from response, clear M bit */
                    /* blocknr = (blocknr & 0xfffffff7) + 0x10; */
                    block.num++;
                    block.m = ((block.num+1) * (1 << (block.szx + 4)) < payload.length);

                    logi("send block %d\n", block.num);
                    coap_add_option(pdu, COAP_OPTION_BLOCK1, coap_encode_var_bytes(buf,(block.num << 4) | (block.m << 3) | block.szx), buf);

                    coap_add_block(pdu, payload.length,payload.s, block.num,block.szx);
                    coap_show_pdu(pdu);
                    if (pdu->hdr->type == COAP_MESSAGE_CON)
                        tid = coap_send_confirmed(ctx, local_interface, remote, pdu);
                    else
                        tid = coap_send(ctx, local_interface, remote, pdu);

                    if (tid == COAP_INVALID_TID) {
                        logi("message_handler: error sending new request");
                        coap_delete_pdu(pdu);
                    } else {
                        set_timeout(&max_wait, wait_seconds);
                        if (pdu->hdr->type != COAP_MESSAGE_CON)
                            coap_delete_pdu(pdu);
                    }

                    return;
                }
            } else {
                logw("There is no block option set, len=%d", len);
                /* There is no block option set, just read the data and we are done. */
                if (coap_get_data(received, &len, &databuf))
                    append_to_output(received, databuf, len);
            }
        }
    } else {      /* no 2.05 */
        /* check if an error was signaled and output payload if so */
        if (COAP_RESPONSE_CLASS(received->hdr->code) >= 4) {
            loge("no %d.%02d", (received->hdr->code >> 5), received->hdr->code & 0x1F);
            if (coap_get_data(received, &len, &databuf)) {
                char receive_data[len];
                strncpy(receive_data, databuf, len);
                loge("%s", databuf);
            }
            fprintf(stderr, "\n");
        }
    }

    /* finally send new request, if needed */
    if (pdu && coap_send(ctx, local_interface, remote, pdu) == COAP_INVALID_TID) {
        logi("message_handler: error sending response");
    }
    coap_delete_pdu(pdu);

    /* our job is done, we can exit at any time */
    ready = coap_check_option(received, COAP_OPTION_SUBSCRIPTION, &opt_iter) == NULL;
}

static coap_list_t *new_option_node(unsigned short key, unsigned int length, unsigned char *data) {
	logd("%s", __func__);
  coap_list_t *node;

  node = coap_malloc(sizeof(coap_list_t) + sizeof(coap_option) + length);

  if (node) {
    coap_option *option;
    option = (coap_option *)(node->data);
    COAP_OPTION_KEY(*option) = key;
    COAP_OPTION_LENGTH(*option) = length;
    memcpy(COAP_OPTION_DATA(*option), data, length);
  } else {
    logw("new_option_node: malloc\n");
  }

  return node;
}

static unsigned short get_default_port(const coap_uri_t *u) {
	return coap_uri_scheme_is_secure(u) ? COAPS_DEFAULT_PORT : COAP_DEFAULT_PORT;
}

static void cmdline_uri(char *arg) {
  unsigned char portbuf[2];
#define BUFSIZE 40
  unsigned char _buf[BUFSIZE];
  unsigned char *buf = _buf;
  size_t buflen;
  int res;

  {      /* split arg into Uri-* options */
    coap_split_uri((unsigned char *)arg, strlen(arg), &uri );
    loge("%s: ============%d, %d, %s", __func__, uri.port, get_default_port(&uri), uri.path.s);

    if (uri.port != get_default_port(&uri)) {
      coap_insert(&optlist,new_option_node(COAP_OPTION_URI_PORT,coap_encode_var_bytes(portbuf, uri.port),portbuf));
    }

    if (uri.path.length) {
      buflen = BUFSIZE;
      res = coap_split_path(uri.path.s, uri.path.length, buf, &buflen);
      logw("res========%d", res);
      while (res--){
        coap_insert(&optlist,new_option_node(COAP_OPTION_URI_PATH,COAP_OPT_LENGTH(buf),COAP_OPT_VALUE(buf)));

        buf += COAP_OPT_SIZE(buf);
      }
    }

    if (uri.query.length) {
      buflen = BUFSIZE;
      buf = _buf;
      res = coap_split_query(uri.query.s, uri.query.length, buf, &buflen);

      while (res--) {
        coap_insert(&optlist,new_option_node(COAP_OPTION_URI_QUERY,COAP_OPT_LENGTH(buf),COAP_OPT_VALUE(buf)));

        buf += COAP_OPT_SIZE(buf);
      }
    }
  }
}

static inline void cmdline_token(char *arg) {
  strncpy((char *)the_token.s, arg, min(sizeof(_token_data), strlen(arg)));
  the_token.length = strlen(arg);
}

static void cmdline_subscribe(unsigned int obs_time) {
  obs_seconds = obs_time;
  coap_insert(&optlist, new_option_node(COAP_OPTION_SUBSCRIPTION, 0, NULL));
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
    logi("%s: node %s, port %s, secure %d", __func__, node, port, secure);
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

    on_data_rcv_method_id = (*env)->GetMethodID(env, clazz, "onDataReceived", "(IS[B)V");
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

static jlongArray jni_coap_new_request(JNIEnv *env, jobject thiz, jint method, jshort token, jstring url, jobjectArray stringArray, jstring text) {
	logd("%s", __func__);
	jlong data[2] = { 0 };
	coap_pdu_t *pdu;

	if (!(pdu = coap_new_pdu()))
		return NULL;

	pdu->hdr->type = msgtype;
	pdu->hdr->id = coap_new_message_id(client.ctx);
	pdu->hdr->code = method;

	int length = (*env)->GetStringLength(env, url);
	if (NULL == url || length == 0) {
		return 0;
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

	jlongArray result = (*env)->NewLongArray(env, 2);
	data[0] = (jlong) pdu;
	data[1] = (jlong) pdu->hdr->id;
	(*env)->SetLongArrayRegion(env, result, 0, 2, data);
	return ((jlongArray) result);
}

static jboolean jni_coap_request(JNIEnv *env, jobject thiz, jlong request_addr) {
	logd("%s", __func__);
	if (request_addr == 0) {
		return JNI_FALSE;
	}
	coap_pdu_t *request = (coap_pdu_t *) request_addr;

	coap_tid_t tid = COAP_INVALID_TID;
	int result = -1;
	if (request->hdr->type == COAP_MESSAGE_CON)
		tid = coap_send_confirmed(client.ctx, client.ctx->endpoint, &client.dst,
				request);
	else
		tid = coap_send(client.ctx, client.ctx->endpoint, &client.dst, request);
	logi("coap_request: id=%d, tid=%d, %s", request->hdr->id, tid);

	if (request->hdr->type != COAP_MESSAGE_CON || tid == COAP_INVALID_TID)
		coap_delete_pdu(request);

	logi("-------finish-------msg id:%d", request->hdr->id);
	return JNI_TRUE;
	err: loge("-------error-------");
	return JNI_FALSE;
}

void *msg_runnable(void *arg) {
	logd("msg_runnable");
	int result = -1;

	set_timeout(&max_wait, wait_seconds);
	coap_tick_t start, now;
	coap_ticks(&start);
	//while (!(ready && coap_can_exit(client.ctx)))
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
			if (start + wait_seconds * COAP_TICKS_PER_SECOND < now) {
				ready = 1;
			}

			if ((unsigned int) result < max_wait) {
				loge("max_wait=%d", max_wait);
				max_wait -= result;
			}
		}
	}

	logi("msg_runnable exit.....");
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
    { "_new_request",       "(ISLjava/lang/String;[Ljava/lang/String;Ljava/lang/String;)[J",      (void *) jni_coap_new_request },
    { "_request",           "(J)Z",                            (void *) jni_coap_request },
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
