/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Optoacoupler Check (Heltec Mesh Node T114)     *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

#include <Arduino.h>

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

// ====== DEFAULT PINS (Heltec Mesh Node T114 V2.0) ======
#ifndef T114_OPTO_PM_PIN
  #define T114_OPTO_PM_PIN 33  // PC817_PIN, GPIO ENTRY OPTOCOUPLER
#endif
#ifndef T114_OPTO_PM_LED
  #define T114_OPTO_PM_LED 7  // EXTERN LED INDICATOR
#endif
#ifndef T114_OPTO_PM_PULLUP
  #define T114_OPTO_PM_PULLUP 0  // 0 -> WITHOUT INTERNAL PULLUP
#endif
#ifndef T114_OPTO_PM_DEBOUNCE_MS
  #define T114_OPTO_PM_DEBOUNCE_MS 50
#endif
#ifndef T114_OPTO_PM_PRINT_PERIOD_MS
  #define T114_OPTO_PM_PRINT_PERIOD_MS 500
#endif

// ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
#ifndef T114_OPTO_PM_LOG_LEVEL
  #define T114_OPTO_PM_LOG_LEVEL 1
#endif

#if T114_OPTO_PM_LOG_LEVEL >= 1
  #define T114_OPTO_PM_LOGI(...) LOG_INFO(__VA_ARGS__)
#else
  #define T114_OPTO_PM_LOGI(...)                                                                   \
    do {                                                                                           \
    } while (0)
#endif

#if T114_OPTO_PM_LOG_LEVEL >= 2
  #define T114_OPTO_PM_LOGD(...) LOG_DEBUG(__VA_ARGS__)
#else
  #define T114_OPTO_PM_LOGD(...)                                                                   \
    do {                                                                                           \
    } while (0)
#endif
// =================================================

class T114OptoPMModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  T114OptoPMModule();

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