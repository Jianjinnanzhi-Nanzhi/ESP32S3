#include "PhotoWebServer.h"

#include <stdlib.h>
#include <string.h>

static const char* WEB_TAG = "PhotoWebServer";

PhotoWebServer* PhotoWebServer::instance_ = NULL;

PhotoWebServer::PhotoWebServer(MemoryPhotoStore& store)
    : store_(store), server_(NULL)
{
}

void PhotoWebServer::notifyNewFrame()
{
  if (server_ == NULL)
  {
    return;
  }
  httpd_queue_work(server_, wsBroadcastWork, this);
}

bool PhotoWebServer::begin(uint16_t port)
{
  instance_ = this;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  config.stack_size = 10240;

  if (httpd_start(&server_, &config) != ESP_OK)
  {
    ESP_LOGE(WEB_TAG, "Failed to start web server");
    return false;
  }

  httpd_uri_t indexUri = {.uri = "/",
                          .method = HTTP_GET,
                          .handler = indexHandler,
                          .user_ctx = NULL};
  httpd_uri_t wsUri = {.uri = "/ws",
                       .method = HTTP_GET,
                       .handler = wsHandler,
                       .user_ctx = NULL,
                       .is_websocket = true,
                       .handle_ws_control_frames = false,
                       .supported_subprotocol = NULL};

  httpd_register_uri_handler(server_, &indexUri);
  httpd_register_uri_handler(server_, &wsUri);

  ESP_LOGI(WEB_TAG, "Photo web server started on port %u", (unsigned int)port);
  return true;
}

esp_err_t PhotoWebServer::indexHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleIndex(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::wsHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleWs(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::handleIndex(httpd_req_t* req)
{
  httpd_resp_set_type(req, "text/html");
  const char* html =
      "<!doctype html><html><head><meta charset=\"utf-8\"><meta "
      "name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
      "<title>ESP32S3 WS Stream</title><style>body{font-family:Arial,sans-"
      "serif;max-width:820px;margin:18px auto;padding:0 "
      "12px;}img{display:block;"
      "width:100%;max-width:760px;border:1px solid #bbb;border-radius:8px;}"
      "#status{font-size:14px;color:#444;margin:10px 0;}</style></head><body>"
      "<h1>ESP32S3 WebSocket Stream</h1><div id=\"status\">Connecting...</div>"
      "<img id=\"live\" alt=\"stream\"><script>(function(){const statusEl="
      "document.getElementById('status');const "
      "imgEl=document.getElementById('live');"
      "let ws;function connect(){const proto=(location.protocol==='https:')?"
      "'wss://':'ws://';ws=new "
      "WebSocket(proto+location.host+'/ws');ws.binaryType="
      "'arraybuffer';ws.onopen=function(){statusEl.textContent='Connected';"
      "ws.send('latest');};ws.onmessage=function(ev){if(typeof "
      "ev.data==='string')"
      "{statusEl.textContent=ev.data;return;}const blob=new Blob([ev.data],"
      "{type:'image/"
      "jpeg'});if(imgEl.dataset.url){URL.revokeObjectURL(imgEl.dataset.url);}"
      "imgEl.src=URL.createObjectURL(blob);imgEl.dataset.url=imgEl.src;"
      "statusEl.textContent="
      "'Updated: '+new Date().toLocaleTimeString();};ws.onclose=function(){"
      "statusEl.textContent='Disconnected, "
      "retrying...';setTimeout(connect,1200);};"
      "ws.onerror=function(){statusEl.textContent='WebSocket "
      "error';};}connect();})();"
      "</script></body></html>";
  httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
  return ESP_OK;
}

esp_err_t PhotoWebServer::handleWs(httpd_req_t* req)
{
  if (req->method == HTTP_GET)
  {
    return ESP_OK;
  }

  httpd_ws_frame_t wsFrame;
  memset(&wsFrame, 0, sizeof(wsFrame));
  wsFrame.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t rc = httpd_ws_recv_frame(req, &wsFrame, 0);
  if (rc != ESP_OK)
  {
    return rc;
  }

  if (wsFrame.len > 256)
  {
    return ESP_FAIL;
  }

  uint8_t payload[257];
  wsFrame.payload = payload;
  rc = httpd_ws_recv_frame(req, &wsFrame, wsFrame.len);
  if (rc != ESP_OK)
  {
    return rc;
  }

  payload[wsFrame.len] = '\0';
  if (wsFrame.type == HTTPD_WS_TYPE_TEXT)
  {
    if (strcmp((const char*)payload, "latest") == 0)
    {
      return sendLatestFrameToClient(req);
    }

    httpd_ws_frame_t textReply;
    memset(&textReply, 0, sizeof(textReply));
    textReply.type = HTTPD_WS_TYPE_TEXT;
    const char* msg = "send: latest";
    textReply.payload = (uint8_t*)msg;
    textReply.len = strlen(msg);
    return httpd_ws_send_frame(req, &textReply);
  }

  return ESP_OK;
}

bool PhotoWebServer::getLatestFrame(uint8_t** outBuf, size_t* outLen,
                                    uint32_t* outId)
{
  if (outBuf == NULL || outLen == NULL)
  {
    return false;
  }

  *outBuf = NULL;
  *outLen = 0;
  if (outId != NULL)
  {
    *outId = 0;
  }

  std::vector<MemoryPhotoMeta> photos;
  if (!store_.getRecent(photos, 1) || photos.empty())
  {
    return false;
  }

  if (outId != NULL)
  {
    *outId = photos[0].id;
  }
  return store_.cloneFrameById(photos[0].id, outBuf, outLen);
}

esp_err_t PhotoWebServer::sendLatestFrameToClient(httpd_req_t* req)
{
  if (req == NULL)
  {
    return ESP_FAIL;
  }

  uint8_t* cloned = NULL;
  size_t len = 0;
  if (!getLatestFrame(&cloned, &len, NULL))
  {
    return ESP_FAIL;
  }

  httpd_ws_frame_t outFrame;
  memset(&outFrame, 0, sizeof(outFrame));
  outFrame.type = HTTPD_WS_TYPE_BINARY;
  outFrame.payload = cloned;
  outFrame.len = len;

  esp_err_t rc = httpd_ws_send_frame(req, &outFrame);
  free(cloned);
  return rc;
}

void PhotoWebServer::wsBroadcastWork(void* arg)
{
  PhotoWebServer* self = static_cast<PhotoWebServer*>(arg);
  if (self != NULL)
  {
    self->broadcastLatestToWsClients();
  }
}

void PhotoWebServer::broadcastLatestToWsClients()
{
  if (server_ == NULL)
  {
    return;
  }

  uint8_t* cloned = NULL;
  size_t len = 0;
  uint32_t id = 0;
  if (!getLatestFrame(&cloned, &len, &id))
  {
    return;
  }

  int clientFds[8];
  size_t clientCount = sizeof(clientFds) / sizeof(clientFds[0]);
  if (httpd_get_client_list(server_, &clientCount, clientFds) != ESP_OK)
  {
    free(cloned);
    return;
  }

  httpd_ws_frame_t outFrame;
  memset(&outFrame, 0, sizeof(outFrame));
  outFrame.type = HTTPD_WS_TYPE_BINARY;
  outFrame.payload = cloned;
  outFrame.len = len;

  size_t wsSent = 0;
  for (size_t i = 0; i < clientCount; ++i)
  {
    int fd = clientFds[i];
    if (httpd_ws_get_fd_info(server_, fd) != HTTPD_WS_CLIENT_WEBSOCKET)
    {
      continue;
    }

    if (httpd_ws_send_frame_async(server_, fd, &outFrame) == ESP_OK)
    {
      ++wsSent;
    }
  }

  ESP_LOGI(WEB_TAG, "Broadcast frame id=%u len=%u to %u ws clients",
           (unsigned int)id, (unsigned int)len, (unsigned int)wsSent);
  free(cloned);
}
