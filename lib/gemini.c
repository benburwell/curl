/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) 1998 - 2020, Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.haxx.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ***************************************************************************/

#include "curl_setup.h"

#ifndef CURL_DISABLE_GEMINI

#include "urldata.h"
#include <curl/curl.h>
#include "transfer.h"
#include "sendf.h"
#include "connect.h"
#include "progress.h"
#include "gemini.h"
#include "select.h"
#include "strdup.h"
#include "url.h"
#include "escape.h"
#include "warnless.h"
#include "curl_printf.h"
#include "curl_memory.h"
#include "vtls/vtls.h"
/* The last #include file should be: */
#include "memdebug.h"

/*
 * Forward declarations.
 */

static CURLcode gemini_do(struct connectdata *conn, bool *done);
static CURLcode gemini_connecting(struct connectdata *conn, bool *done);
CURLcode Curl_gemini_connect(struct connectdata *conn, bool *done);

/*
 * Gemini protocol handler.
 */

const struct Curl_handler Curl_handler_gemini = {
  "GEMINI",                             /* scheme */
  ZERO_NULL,                            /* setup_connection */
  gemini_do,                            /* do_it */
  ZERO_NULL,                            /* done */
  ZERO_NULL,                            /* do_more */
  Curl_gemini_connect,                  /* connect_it */
  gemini_connecting,                    /* connecting */
  ZERO_NULL,                            /* doing */
  ZERO_NULL,                            /* proto_getsock */
  ZERO_NULL,                            /* doing_getsock */
  ZERO_NULL,                            /* domore_getsock */
  ZERO_NULL,                            /* perform_getsock */
  ZERO_NULL,                            /* disconnect */
  ZERO_NULL,                            /* readwrite */
  ZERO_NULL,                            /* connection_check */
  PORT_GEMINI,                          /* defport */
  CURLPROTO_GEMINI,                     /* protocol */
  PROTOPT_NONE                          /* flags */
};

static CURLcode gemini_do(struct connectdata *conn, bool *done)
{
  CURLcode result = CURLE_OK;
  struct Curl_easy *data = conn->data;
  curl_socket_t sockfd = conn->sock[FIRSTSOCKET];
  char *sel = NULL;
  timediff_t timeout_ms;
  ssize_t amount, k;
  size_t len;
  int what;
  char *req = NULL;
  char *u = NULL;
  CURLUcode uc;

  *done = TRUE; /* unconditionally */

  /* format the request URL */
  if ((uc = curl_url_get(data->state.uh, CURLUPART_URL, &u, 0)) != CURLUE_OK) {
    return CURLE_OUT_OF_MEMORY;
  }

  /* build the request line */
  req = aprintf("%s\r\n", u);
  if (!req)
    return CURLE_OUT_OF_MEMORY;

  len = strlen(req);

  /* We use Curl_write instead of Curl_sendf to make sure the entire buffer is
     sent, which could be sizeable with long selectors. */
  k = curlx_uztosz(len);

  for(;;) {
    result = Curl_write(conn, sockfd, req, k, &amount);
    if(!result) { /* Which may not have written it all! */
      result = Curl_client_write(conn, CLIENTWRITE_HEADER, req, amount);
      if(result)
        break;

      k -= amount;
      sel += amount;
      if(k < 1)
        break; /* but it did write it all */
    }
    else
      break;

    timeout_ms = Curl_timeleft(conn->data, NULL, FALSE);
    if(timeout_ms < 0) {
      result = CURLE_OPERATION_TIMEDOUT;
      break;
    }
    if(!timeout_ms)
      timeout_ms = TIMEDIFF_T_MAX;

    /* Don't busyloop. The entire loop thing is a work-around as it causes a
       BLOCKING behavior which is a NO-NO. This function should rather be
       split up in a do and a doing piece where the pieces that aren't
       possible to send now will be sent in the doing function repeatedly
       until the entire request is sent.
    */
    what = SOCKET_WRITABLE(sockfd, timeout_ms);
    if(what < 0) {
      result = CURLE_SEND_ERROR;
      break;
    }
    else if(!what) {
      result = CURLE_OPERATION_TIMEDOUT;
      break;
    }
  }

  Curl_setup_transfer(data, FIRSTSOCKET, -1, FALSE, -1);
  return CURLE_OK;
}

static CURLcode gemini_connecting(struct connectdata *conn, bool *done)
{
  CURLcode result;
  DEBUGASSERT((conn) && (conn->handler->flags & PROTOPT_SSL));

  /* perform SSL initialization for this socket */
  result = Curl_ssl_connect_nonblocking(conn, FIRSTSOCKET, done);
  if(result)
    connclose(conn, "Failed TLS connection");

  return result;
}

/*
 * Curl_http_connect() performs HTTP stuff to do at connect-time, called from
 * the generic Curl_connect().
 */
CURLcode Curl_gemini_connect(struct connectdata *conn, bool *done)
{
  CURLcode result;

  /* perform SSL initialization */
  result = gemini_connecting(conn, done);
  if(result)
    return result;

  return CURLE_OK;
}
#endif /*CURL_DISABLE_GEMINI*/
