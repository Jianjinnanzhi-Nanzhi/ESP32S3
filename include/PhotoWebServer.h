#ifndef PHOTO_WEB_SERVER_H
#define PHOTO_WEB_SERVER_H

#include <Arduino.h>

#include <vector>

#include "MemoryPhotoStore.h"
#include "esp_http_server.h"
#include "esp_log.h"

class PhotoWebServer
{
 public:
  explicit PhotoWebServer(MemoryPhotoStore& store);
  bool begin(uint16_t port = 80);
  void notifyNewFrame();

 private:
  static esp_err_t indexHandler(httpd_req_t* req);
  static esp_err_t photoHandler(httpd_req_t* req);
  static esp_err_t wsHandler(httpd_req_t* req);
  static esp_err_t deletePhotoHandler(httpd_req_t* req);
  static esp_err_t deleteAllHandler(httpd_req_t* req);

  esp_err_t handleIndex(httpd_req_t* req);
  esp_err_t handlePhoto(httpd_req_t* req);
  esp_err_t handleWs(httpd_req_t* req);
  esp_err_t handleDeletePhoto(httpd_req_t* req);
  esp_err_t handleDeleteAll(httpd_req_t* req);

  bool getLatestFrame(uint8_t** outBuf, size_t* outLen, uint32_t* outId);
  esp_err_t sendLatestFrameToClient(httpd_req_t* req);
  void broadcastLatestToWsClients();
  static void wsBroadcastWork(void* arg);

  static bool parseIdParam(httpd_req_t* req, uint32_t* outId);
  static void sendBadRequest(httpd_req_t* req, const char* message);

  MemoryPhotoStore& store_;
  httpd_handle_t server_;
  uint8_t chunkBuf_[1024];

  static PhotoWebServer* instance_;
};

#endif