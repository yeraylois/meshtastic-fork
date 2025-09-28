/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Util    : Flag Store (Heltec Wireless Stick V3)            *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/

#include "Ws3FlagStore.h"

#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)

  #define LOG_TAG "flag_store_ws3"
  #include "configuration.h"

extern "C" {
  #include "nvs.h"
  #include "nvs_flash.h"
}

void* Ws3FlagStore::handle_ = nullptr;
bool  Ws3FlagStore::inited_ = false;

// ENSURE NVS IS INITIALIZED
void Ws3FlagStore::ensureNvsInit() {
  if (inited_)
    return;
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    err = nvs_flash_init();
  }
  inited_ = true;
  WS3_FLAG_LOGD("[FlagStore] NVS init err=0x%x\n", (unsigned) err);
}

// BEGIN NVS
bool Ws3FlagStore::begin() {
  ensureNvsInit();
  if (handle_)
    return true;
  nvs_handle_t h   = 0;
  esp_err_t    err = nvs_open(WS3_FLAG_NVS_NAMESPACE, NVS_READWRITE, &h);
  if (err != ESP_OK) {
    WS3_FLAG_LOGI("[FlagStore] nvs_open('%s') fail=0x%x\n", WS3_FLAG_NVS_NAMESPACE, (unsigned) err);
    return false;
  }
  handle_ = (void*) h;
  WS3_FLAG_LOGD("[FlagStore] open ns='%s'\n", WS3_FLAG_NVS_NAMESPACE);
  return true;
}

bool Ws3FlagStore::isReady() {
  return handle_ != nullptr;
}

bool Ws3FlagStore::read(uint32_t& out) {
  if (!handle_ && !begin())
    return false;
  uint32_t  v   = 0;
  esp_err_t err = nvs_get_u32((nvs_handle_t) handle_, WS3_FLAG_NVS_KEY, &v);
  if (err == ESP_OK) {
    out = v;
    WS3_FLAG_LOGD("[FlagStore] read %s=0x%08" PRIX32 " (%" PRIu32 ")\n", WS3_FLAG_NVS_KEY, v, v);
    return true;
  }
  if (err != ESP_ERR_NVS_NOT_FOUND)
    WS3_FLAG_LOGI("[FlagStore] read err=0x%x\n", (unsigned) err);
  return false;
}

uint32_t Ws3FlagStore::get() {
  uint32_t v = 0;
  if (read(v))
    return v;
  return def();
}

bool Ws3FlagStore::write(uint32_t value) {
  if (!handle_ && !begin())
    return false;
  esp_err_t err = nvs_set_u32((nvs_handle_t) handle_, WS3_FLAG_NVS_KEY, value);
  if (err != ESP_OK) {
    WS3_FLAG_LOGI("[FlagStore] set err=0x%x\n", (unsigned) err);
    return false;
  }
  err = nvs_commit((nvs_handle_t) handle_);
  if (err != ESP_OK) {
    WS3_FLAG_LOGI("[FlagStore] commit err=0x%x\n", (unsigned) err);
    return false;
  }
  WS3_FLAG_LOGI(
      "[FlagStore] write %s=0x%08" PRIX32 " (%" PRIu32 ")\n", WS3_FLAG_NVS_KEY, value, value);
  return true;
}

bool Ws3FlagStore::erase() {
  if (!handle_ && !begin())
    return false;
  esp_err_t err = nvs_erase_key((nvs_handle_t) handle_, WS3_FLAG_NVS_KEY);
  if (err == ESP_ERR_NVS_NOT_FOUND) {
    WS3_FLAG_LOGD("[FlagStore] erase: key does not exist\n");
    return true;  // CLEANED
  }
  if (err != ESP_OK) {
    WS3_FLAG_LOGI("[FlagStore] erase err=0x%x\n", (unsigned) err);
    return false;
  }
  err = nvs_commit((nvs_handle_t) handle_);
  if (err != ESP_OK) {
    WS3_FLAG_LOGI("[FlagStore] commit err=0x%x\n", (unsigned) err);
    return false;
  }
  WS3_FLAG_LOGI("[FlagStore] erase OK\n");
  return true;
}

bool Ws3FlagStore::writeDefault() {
  return write(def());
}

void Ws3FlagStore::print() {
  uint32_t v = 0;
  if (read(v)) {
    LOG_INFO("[FlagStore] %s='%s' %s=0x%08" PRIX32 " (%" PRIu32 ")\n",
             "ns",
             WS3_FLAG_NVS_NAMESPACE,
             WS3_FLAG_NVS_KEY,
             v,
             v);
  } else {
    LOG_INFO("[FlagStore] %s='%s' %s does not exist → default=0x%08" PRIX32 " (%" PRIu32 ")\n",
             "ns",
             WS3_FLAG_NVS_NAMESPACE,
             WS3_FLAG_NVS_KEY,
             def(),
             def());
  }
}

#endif