/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Optoacoupler & Reboot (Heltec Wireless Stick V3) *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/
#pragma once
#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)

  #include <Arduino.h>

  #include "concurrency/OSThread.h"
  #include "mesh/SinglePortModule.h"
  #include "mesh/generated/meshtastic/portnums.pb.h"

  // ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
  #ifndef WS3_OPTO_FLAG_LOG_LEVEL
    #define WS3_OPTO_FLAG_LOG_LEVEL 1
  #endif
  #if WS3_OPTO_FLAG_LOG_LEVEL >= 1
    #define WS3_OPTOF_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define WS3_OPTOF_LOGI(...)                                                                    \
      do {                                                                                         \
      } while (0)
  #endif
  #if WS3_OPTO_FLAG_LOG_LEVEL >= 2
    #define WS3_OPTOF_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define WS3_OPTOF_LOGD(...)                                                                    \
      do {                                                                                         \
      } while (0)
  #endif

  // ======= PINS/CONFIG  =======
  #ifndef WS3_OPTO_PM_PIN
    #define WS3_OPTO_PM_PIN 38
  #endif
  #ifndef WS3_OPTO_PM_LED
    #define WS3_OPTO_PM_LED 37
  #endif
  #ifndef WS3_OPTO_PM_PULLUP
    #define WS3_OPTO_PM_PULLUP 0
  #endif
  #ifndef WS3_OPTO_PM_DEBOUNCE_MS
    #define WS3_OPTO_PM_DEBOUNCE_MS 50
  #endif
  #ifndef WS3_OPTO_PM_PRINT_PERIOD_MS
    #define WS3_OPTO_PM_PRINT_PERIOD_MS 500
  #endif

  // ======= WRITE POLICY AND PERSISTENCE =======

  // DEFAULT VALUE
  #ifndef WS3_FLAG_DEFAULT
    #define WS3_FLAG_DEFAULT 0xCAFEBABEUL
  #endif

  // SIGN AND FLAG STATUS
  #ifndef WS3_FLAG_OPTO_POWER_OK
    #define WS3_FLAG_OPTO_POWER_OK 0xAABBCC01UL
  #endif
  #ifndef WS3_FLAG_OPTO_POWER_DOWN
    #define WS3_FLAG_OPTO_POWER_DOWN 0xAABBCC00UL
  #endif

  // EVADE REPEATED WRITES
  #ifndef WS3_OPTO_MIN_WRITE_MS
    #define WS3_OPTO_MIN_WRITE_MS 2000
  #endif

  // REBOOT ON CHANGE
  #ifndef WS3_OPTO_REBOOT_ON_CHANGE
    #define WS3_OPTO_REBOOT_ON_CHANGE 0  // [1=REBOOT, 0=NO REBOOT]
  #endif

class Ws3OptoFlagBridgeModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  Ws3OptoFlagBridgeModule();

  int32_t        runOnce() override;
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  void initOnce();
  void handleEdge(bool powerOk);
  void printStatus(bool powerOk);

  bool     ready_       = false;
  bool     lastPowerOk_ = true;
  uint32_t tNextPrint_  = 0;
  uint32_t tWriteGuard_ = 0;  // SAVE WRITE GUARD TIMESTAMP
};

#endif