/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 * -*- */

/* coap_list.c -- CoAP list structures
 *
 * Copyright (C) 2010,2011,2015 Olaf Bergmann <bergmann@tzi.org>
 *
 * This file is part of the CoAP library libcoap. Please see README for terms of
 * use.
 */

/* #include "coap_config.h" */

#include <stdio.h>
#include <string.h>

#include "debug.h"
#include "mem.h"
#include "coap_list.h"


int coap_insert(coap_list_t **head, coap_list_t *node) {
	logd("%s", __func__);
  if (!node) {
    loge( "cannot create option Proxy-Uri");
  } else {
    /* must append at the list end to avoid re-ordering of
     * options during sort */
    LL_APPEND((*head), node);
  }

  return node != NULL;
}

int
coap_delete(coap_list_t *node) {
	logd("%s", __func__);
  if (node) {
    coap_free(node);
  }
  return 1;
}

int coap_delete2(coap_list_t **head, coap_list_t *node) {
	if (node) {
		LL_DELETE(*head, node);
		return 1;
	} else{
		loge("delete node fail.");
		return -1;
	}
}
//LL_DELETE(head,del)
/*
int coap_list_count(coap_list_t **queue1, coap_list_t ** queue2){
	int count ;
	LL_COUNT((*queue1), (*queue2), &count);
	return count;
}
*/

void coap_delete_list(coap_list_t *queue) {
	logd("%s", __func__);
  coap_list_t *elt, *tmp;

  if (!queue)
    return;

  LL_FOREACH_SAFE(queue, elt, tmp) {
    coap_delete(elt);
  }
}

