/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Optoacoupler Check (Heltec Wireless Stick V3)  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

#include <Arduino.h>

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// ====== DEFAULT PINS (Heltec Wireless Stick V3 (ESP32-S3)) ======
#ifndef WS3_OPTO_PM_PIN
  #define WS3_OPTO_PM_PIN 38  // PC817_PIN, GPIO ENTRY OPTOCOUPLER
#endif
#ifndef WS3_OPTO_PM_LED
  #define WS3_OPTO_PM_LED 37  // EXTERN LED INDICATOR
#endif
#ifndef WS3_OPTO_PM_PULLUP
  #define WS3_OPTO_PM_PULLUP 0  // 0 -> WITHOUT INTERNAL PULLUP
#endif
#ifndef WS3_OPTO_PM_DEBOUNCE_MS
  #define WS3_OPTO_PM_DEBOUNCE_MS 50
#endif
#ifndef WS3_OPTO_PM_PRINT_PERIOD_MS
  #define WS3_OPTO_PM_PRINT_PERIOD_MS 500
#endif

// ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
#ifndef WS3_OPTO_PM_LOG_LEVEL
  #define WS3_OPTO_PM_LOG_LEVEL 1
#endif
#if WS3_OPTO_PM_LOG_LEVEL >= 1
  #define WS3_OPTO_LOGI(...) LOG_INFO(__VA_ARGS__)
#else
  #define WS3_OPTO_LOGI(...)                                                                       \
    do {                                                                                           \
    } while (0)
#endif

#if WS3_OPTO_PM_LOG_LEVEL >= 2
  #define WS3_OPTO_LOGD(...) LOG_DEBUG(__VA_ARGS__)
#else
  #define WS3_OPTO_LOGD(...)                                                                       \
    do {                                                                                           \
    } while (0)
#endif
// =================================================

class Ws3OptoPMModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  Ws3OptoPMModule();

  // COOPERATIVE THREAD
  int32_t runOnce() override;

  // NO MESH TRAFFIC CONSUMPTION
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  void initOnce();

  bool     ready_      = false;
  uint32_t tNextPrint_ = 0;
};