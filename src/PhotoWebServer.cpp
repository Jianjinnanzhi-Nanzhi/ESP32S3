#include "PhotoWebServer.h"

#include <algorithm>

static const char* WEB_TAG = "PhotoWebServer";

static bool isJpegFileName(const String& name)
{
  String lower = name;
  lower.toLowerCase();
  return lower.endsWith(".jpg") || lower.endsWith(".jpeg");
}

PhotoWebServer* PhotoWebServer::instance_ = NULL;

PhotoWebServer::PhotoWebServer(ResourceMutex& mutex)
    : mutex_(mutex), server_(NULL)
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
  if (!mutex_.lock(5000))
  {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "busy", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  File root = LittleFS.open("/");
  if (!root || !root.isDirectory())
  {
    mutex_.unlock();
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  std::vector<PhotoEntry> photos;
  File file = root.openNextFile();
  while (file)
  {
    String raw = String(file.name());
    if (!file.isDirectory() && isJpegFileName(raw))
    {
      PhotoEntry entry;
      entry.name = raw.startsWith("/") ? raw.substring(1) : raw;
      entry.ts = parsePhotoTs(entry.name);
      entry.size = file.size();
      photos.push_back(entry);
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
  mutex_.unlock();

  std::sort(photos.begin(), photos.end(),
            [](const PhotoEntry& a, const PhotoEntry& b)
            {
              if (a.ts == b.ts)
              {
                return a.name > b.name;
              }
              return a.ts > b.ts;
            });

  httpd_resp_set_type(req, "text/html");
  httpd_resp_send_chunk(
      req,
      "<html><body><h1>Photos</h1><p><a href=\"/delete_all\">Delete All "
      "Photos</a></p><ul>",
      HTTPD_RESP_USE_STRLEN);

  char line[320];
  for (size_t i = 0; i < photos.size(); ++i)
  {
    int n = snprintf(line, sizeof(line),
                     "<li><a href=\"/photo?name=%s\">%s</a> (%u bytes) "
                     "<a href=\"/delete?name=%s\">[Delete]</a></li>",
                     photos[i].name.c_str(), photos[i].name.c_str(),
                     (unsigned int)photos[i].size, photos[i].name.c_str());
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
  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
  {
    sendBadRequest(req, "query param missing");
    return ESP_FAIL;
  }

  char nameParam[128];
  if (httpd_query_key_value(query, "name", nameParam, sizeof(nameParam)) !=
      ESP_OK)
  {
    sendBadRequest(req, "name param missing");
    return ESP_FAIL;
  }

  String name = String(nameParam);
  if (!isValidName(name))
  {
    sendBadRequest(req, "invalid name");
    return ESP_FAIL;
  }

  if (!mutex_.lock(5000))
  {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "busy", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  File file = LittleFS.open(toPath(name).c_str(), "r");
  if (!file)
  {
    mutex_.unlock();
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }

  httpd_resp_set_type(req, "image/jpeg");
  size_t chunkSize = 0;
  while ((chunkSize = file.read(chunkBuf_, sizeof(chunkBuf_))) > 0)
  {
    if (httpd_resp_send_chunk(req, (const char*)chunkBuf_, chunkSize) != ESP_OK)
    {
      file.close();
      mutex_.unlock();
      return ESP_FAIL;
    }
  }

  file.close();
  mutex_.unlock();
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t PhotoWebServer::handleDeletePhoto(httpd_req_t* req)
{
  char query[128];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK)
  {
    sendBadRequest(req, "query param missing");
    return ESP_FAIL;
  }

  char nameParam[128];
  if (httpd_query_key_value(query, "name", nameParam, sizeof(nameParam)) !=
      ESP_OK)
  {
    sendBadRequest(req, "name param missing");
    return ESP_FAIL;
  }

  String name = String(nameParam);
  if (!isValidName(name))
  {
    sendBadRequest(req, "invalid name");
    return ESP_FAIL;
  }

  if (!mutex_.lock(5000))
  {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "busy", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  LittleFS.remove(toPath(name).c_str());
  mutex_.unlock();

  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

esp_err_t PhotoWebServer::handleDeleteAll(httpd_req_t* req)
{
  if (!mutex_.lock(5000))
  {
    httpd_resp_set_status(req, "503 Service Unavailable");
    httpd_resp_send(req, "busy", HTTPD_RESP_USE_STRLEN);
    return ESP_FAIL;
  }

  File root = LittleFS.open("/");
  if (!root || !root.isDirectory())
  {
    mutex_.unlock();
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  std::vector<String> toDelete;
  File file = root.openNextFile();
  while (file)
  {
    String raw = String(file.name());
    if (!file.isDirectory() && isJpegFileName(raw))
    {
      toDelete.push_back(raw.startsWith("/") ? raw : (String("/") + raw));
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();

  size_t removedCount = 0;
  size_t failedCount = 0;
  for (size_t i = 0; i < toDelete.size(); ++i)
  {
    if (LittleFS.remove(toDelete[i].c_str()))
    {
      ++removedCount;
    }
    else
    {
      ++failedCount;
      ESP_LOGW(WEB_TAG, "Failed to remove: %s", toDelete[i].c_str());
    }
  }
  mutex_.unlock();

  ESP_LOGI(WEB_TAG,
           "Delete all in LittleFS done. target=%u removed=%u failed=%u",
           (unsigned int)toDelete.size(), (unsigned int)removedCount,
           (unsigned int)failedCount);

  httpd_resp_set_status(req, "302 Found");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

uint64_t PhotoWebServer::parsePhotoTs(const String& name)
{
  uint64_t ts = 0;
  for (size_t i = 0; i < name.length(); ++i)
  {
    char c = name.charAt(i);
    if (c >= '0' && c <= '9')
    {
      ts = ts * 10ULL + (uint64_t)(c - '0');
    }
  }
  return ts;
}

String PhotoWebServer::toPath(const String& name)
{
  return name.startsWith("/") ? name : (String("/") + name);
}

bool PhotoWebServer::isValidName(const String& name)
{
  if (name.length() == 0 || name.indexOf("..") >= 0 || name.indexOf('/') >= 0)
  {
    return false;
  }
  return name.endsWith(".jpg");
}

void PhotoWebServer::sendBadRequest(httpd_req_t* req, const char* message)
{
  httpd_resp_set_status(req, "400 Bad Request");
  httpd_resp_send(req, message, HTTPD_RESP_USE_STRLEN);
}