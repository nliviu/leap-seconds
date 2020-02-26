/*
 * Copyright 2019 Liviu Nicolescu <nliviu@gmail.com>
 * All rights reserved
 *
 * Licensed under the Apache License, Version 2.0 (the ""License"");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an ""AS IS"" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mgos.h"

static void ev_handler(struct mg_connection *c, int ev, void *evd,
                       void *cb_arg) {
  static FILE *leap = NULL;
  const struct http_message *msg = (const struct http_message *) (evd);
  switch (ev) {
    case MG_EV_CONNECT:
      LOG(LL_INFO, ("%s:%d - err=%d", __FUNCTION__, __LINE__, *(int *) evd));
      if (*(int *) evd != 0) {
        c->flags |= MG_F_CLOSE_IMMEDIATELY;
        return;
      }
      if (leap == NULL) {
        leap = fopen("leap-seconds.list", "w");
        if (leap == NULL) {
          LOG(LL_ERROR, ("Failed to open leap-seconds.list!"));
          c->flags |= MG_F_CLOSE_IMMEDIATELY;
        }
      }
      break;
    case MG_EV_HTTP_REPLY: {
      LOG(LL_INFO, ("%s:%d - MG_EV_HTTP_REPLY - len=%d", __FUNCTION__, __LINE__,
                    (int) msg->body.len));
      if ((msg->resp_code == 200) && (leap != NULL) && (msg->body.len != 0)) {
        fwrite(msg->body.p, 1, msg->body.len, leap);
      }
      c->flags |= MG_F_CLOSE_IMMEDIATELY;
      break;
    }
    case MG_EV_HTTP_CHUNK: {
      if ((leap != NULL) && (msg->body.len != 0)) {
        LOG(LL_INFO, ("%s:%d - MG_EV_HTTP_CHUNK - body.len=%d", __FUNCTION__,
                      __LINE__, (int) msg->body.len));
        /* TODO: process Transfer-Encoding: chunked data */
        fwrite(msg->body.p, 1, msg->body.len, leap);
      }
      c->flags |= MG_F_DELETE_CHUNK;
      break;
    }
    case MG_EV_CLOSE:
      if (leap != NULL) {
        fflush(leap);
        fclose(leap);
        leap = NULL;
      }
      break;
  }
  (void) cb_arg;
}

static void init_application() {
  const char *server = "https://www.ietf.org/timezones/data/leap-seconds.list";
  LOG(LL_INFO, ("Connecting to %s", server));
  struct mg_mgr *mgr = mgos_get_mgr();
  struct mg_connection *conn =
      mg_connect_http(mgr, ev_handler, NULL, server, NULL, NULL);
  if (conn == NULL) {
    LOG(LL_ERROR, ("Failed to connect to %s", server));
  }
}

static void net_event_handler(int ev, void *evd, void *arg) {
  const struct mgos_net_event_data *data =
      (const struct mgos_net_event_data *) evd;
  static bool init = false;
  switch (ev) {
    case MGOS_NET_EV_IP_ACQUIRED: {
      struct mgos_net_ip_info ip_info;
      if (data != NULL) {
        mgos_net_get_ip_info(data->if_type, data->if_instance, &ip_info);
        char ip[16], gw[16];
        memset(ip, 0, sizeof(ip));
        memset(gw, 0, sizeof(gw));
        mgos_net_ip_to_str(&ip_info.ip, ip);
        mgos_net_ip_to_str(&ip_info.gw, gw);
        char *nameserver = mgos_get_nameserver();
        LOG(LL_INFO, ("Got IP - ip=%s, gw=%s, dns=%s", ip, gw,
                      nameserver ? nameserver : "NULL"));
        if (NULL != nameserver) {
          free(nameserver);
        }
      }
      if (!init) {
        init = true;
        init_application();
      }

      break;
    }
  }
  (void) arg;
}

enum mgos_app_init_result mgos_app_init(void) {
  mgos_event_add_group_handler(MGOS_EVENT_GRP_NET, net_event_handler, NULL);
  return MGOS_APP_INIT_SUCCESS;
}
