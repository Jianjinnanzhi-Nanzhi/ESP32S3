#include "PhotoWebServer.h"

#include <stdlib.h>

#include <algorithm>


static const char* WEB_TAG = "PhotoWebServer";

PhotoWebServer* PhotoWebServer::instance_ = NULL;

PhotoWebServer::PhotoWebServer(MemoryPhotoStore& store)
    : store_(store), server_(NULL)
{
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
      "<html><body><h1>Photos In RAM</h1><p><a href=\"/delete_all\">Delete "
      "All</a></p><ul>",
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

  httpd_resp_send_chunk(req, "</ul></body></html>", HTTPD_RESP_USE_STRLEN);
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