#include "PhotoWebServer.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>

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
  httpd_uri_t photoUri = {.uri = "/photo",
                          .method = HTTP_GET,
                          .handler = photoHandler,
                          .user_ctx = NULL};
  httpd_uri_t wsUri = {.uri = "/ws",
                       .method = HTTP_GET,
                       .handler = wsHandler,
                       .user_ctx = NULL,
                       .is_websocket = true,
                       .handle_ws_control_frames = false,
                       .supported_subprotocol = NULL};
  httpd_uri_t deleteUri = {.uri = "/delete",
                           .method = HTTP_GET,
                           .handler = deletePhotoHandler,
                           .user_ctx = NULL};
  httpd_uri_t deleteAllUri = {.uri = "/delete_all",
                              .method = HTTP_GET,
                              .handler = deleteAllHandler,
                              .user_ctx = NULL};

  httpd_register_uri_handler(server_, &indexUri);
  httpd_register_uri_handler(server_, &photoUri);
  httpd_register_uri_handler(server_, &wsUri);
  httpd_register_uri_handler(server_, &deleteUri);
  httpd_register_uri_handler(server_, &deleteAllUri);

  ESP_LOGI(WEB_TAG, "Photo web server started on port %u", (unsigned int)port);
  return true;
}

esp_err_t PhotoWebServer::indexHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleIndex(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::photoHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handlePhoto(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::wsHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleWs(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::deletePhotoHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleDeletePhoto(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::deleteAllHandler(httpd_req_t* req)
{
  return (instance_ != NULL) ? instance_->handleDeleteAll(req) : ESP_FAIL;
}

esp_err_t PhotoWebServer::handleIndex(httpd_req_t* req)
{
  std::vector<MemoryPhotoMeta> photos;
  if (!store_.getRecent(photos, 0))
  {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "busy", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send_chunk(
      req,
      "<!doctype html><html><head><meta charset=\"utf-8\"><meta "
      "name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><title>ESP32S3 "
      "Photos</title>"
      "<style>body{font-family:Arial,sans-serif;max-width:900px;margin:20px "
      "auto;padding:0 "
      "12px;}img{width:100%;max-width:640px;border:1px solid "
      "#ccc;border-radius:8px;}"
      "#status{font-size:14px;color:#555;margin:8px "
      "0;}ul{line-height:1.6;}</style></head>"
      "<body><h1>Photos In RAM</h1><p><a href=\"/delete_all\">Delete "
      "All</a></p>"
      "<h2>Live (WebSocket)</h2><div id=\"status\">Connecting...</div>"
      "<img id=\"live\" alt=\"latest frame\"><ul>",
      HTTPD_RESP_USE_STRLEN);

  char line[320];
  for (size_t i = 0; i < photos.size(); ++i)
  {
    int n = snprintf(line, sizeof(line),
                     "<li><a href=\"/photo?id=%u\">frame_%u.jpg</a> "
                     "(%u bytes, ts=%llu) "
                     "<a href=\"/delete?id=%u\">[Delete]</a></li>",
                     (unsigned int)photos[i].id, (unsigned int)photos[i].id,
                     (unsigned int)photos[i].len,
                     (unsigned long long)photos[i].tsMs,
                     (unsigned int)photos[i].id);
    if (n > 0)
    {
      httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
    }
  }

  httpd_resp_send_chunk(
      req,
      "</ul><script>"
      "(function(){"
      "const statusEl=document.getElementById('status');"
      "const imgEl=document.getElementById('live');"
      "let ws;"
      "function connect(){"
      "const proto=(location.protocol==='https:')?'wss://':'ws://';"
      "ws=new WebSocket(proto+location.host+'/ws');"
      "ws.binaryType='arraybuffer';"
      "ws.onopen=function(){statusEl.textContent='Connected, waiting for "
      "push...';ws.send('latest');};"
      "ws.onmessage=function(ev){"
      "if(typeof ev.data==='string'){statusEl.textContent=ev.data;return;}"
      "const blob=new Blob([ev.data],{type:'image/jpeg'});"
      "if(imgEl.dataset.url){URL.revokeObjectURL(imgEl.dataset.url);}"
      "imgEl.src=URL.createObjectURL(blob);"
      "imgEl.dataset.url=imgEl.src;"
      "statusEl.textContent='Updated: '+new Date().toLocaleTimeString();"
      "};"
      "ws.onclose=function(){statusEl.textContent='Disconnected, retrying...';"
      "setTimeout(connect,1200);};"
      "ws.onerror=function(){statusEl.textContent='WebSocket error';};"
      "}"
      "connect();"
      "})();"
      "</script></body></html>",
      HTTPD_RESP_USE_STRLEN);
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t PhotoWebServer::handlePhoto(httpd_req_t* req)
{
  uint32_t id = 0;
  if (!parseIdParam(req, &id))
  {
    sendBadRequest(req, "id param missing or invalid");
    return ESP_FAIL;
  }

  uint8_t* cloned = NULL;
  size_t len = 0;
  if (!store_.cloneFrameById(id, &cloned, &len))
  {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  size_t offset = 0;
  while (offset < len)
  {
    size_t chunkSize = len - offset;
    if (chunkSize > sizeof(chunkBuf_))
    {
      chunkSize = sizeof(chunkBuf_);
    }
    memcpy(chunkBuf_, cloned + offset, chunkSize);
    if (httpd_resp_send_chunk(req, (const char*)chunkBuf_, chunkSize) != ESP_OK)
    {
      free(cloned);
      return ESP_FAIL;
    }
    offset += chunkSize;
  }
  free(cloned);
  httpd_resp_send_chunk(req, NULL, 0);
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

esp_err_t PhotoWebServer::handleDeletePhoto(httpd_req_t* req)
{
  uint32_t id = 0;
  if (!parseIdParam(req, &id))
  {
    sendBadRequest(req, "id param missing or invalid");
    return ESP_FAIL;
  }

  store_.deleteById(id);

  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t PhotoWebServer::handleDeleteAll(httpd_req_t* req)
{
  size_t removed = store_.clearAll();
  ESP_LOGI(WEB_TAG, "Delete all in RAM done. removed=%u",
           (unsigned int)removed);

  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

bool PhotoWebServer::parseIdParam(httpd_req_t* req, uint32_t* outId)
{
  if (outId == NULL)
  {
    return false;
  }

  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
  {
    return false;
  }

  char idParam[32];
  if (httpd_query_key_value(query, "id", idParam, sizeof(idParam)) != ESP_OK)
  {
    return false;
  }

  char* endPtr = NULL;
  unsigned long value = strtoul(idParam, &endPtr, 10);
  if (endPtr == idParam || *endPtr != '\0' || value == 0)
  {
    return false;
  }

  *outId = (uint32_t)value;
  return true;
}

void PhotoWebServer::sendBadRequest(httpd_req_t* req, const char* message)
{
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
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
