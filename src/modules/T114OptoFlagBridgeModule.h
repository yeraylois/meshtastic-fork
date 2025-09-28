/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Optoacoupler & Reboot (Heltec Mesh Node T114)    *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/
#pragma once
#if defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)

  #include <Arduino.h>
  #include <inttypes.h>

  #include "concurrency/OSThread.h"
  #include "mesh/SinglePortModule.h"
  #include "mesh/generated/meshtastic/portnums.pb.h"

  // ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
  #ifndef T114_OPTO_FLAG_LOG_LEVEL
    #define T114_OPTO_FLAG_LOG_LEVEL 1
  #endif
  #if T114_OPTO_FLAG_LOG_LEVEL >= 1
    #define T114_OPTOF_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define T114_OPTOF_LOGI(...)                                                                   \
      do {                                                                                         \
      } while (0)
  #endif
  #if T114_OPTO_FLAG_LOG_LEVEL >= 2
    #define T114_OPTOF_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define T114_OPTOF_LOGD(...)                                                                   \
      do {                                                                                         \
      } while (0)
  #endif

  // ======= PINS/CONFIG (T114 v2.0) =======
  #ifndef T114_OPTO_PM_PIN
    #define T114_OPTO_PM_PIN 33
  #endif
  #ifndef T114_OPTO_PM_LED
    #define T114_OPTO_PM_LED 7
  #endif
  #ifndef T114_OPTO_PM_PULLUP
    #define T114_OPTO_PM_PULLUP 0
  #endif
  #ifndef T114_OPTO_PM_DEBOUNCE_MS
    #define T114_OPTO_PM_DEBOUNCE_MS 50
  #endif
  #ifndef T114_OPTO_PM_PRINT_PERIOD_MS
    #define T114_OPTO_PM_PRINT_PERIOD_MS 500
  #endif

  // ======= WRITE POLICY / FLAGS =======
  #ifndef T114_FLAG_DEFAULT
    #define T114_FLAG_DEFAULT 0xCAFEBABEUL
  #endif
  #ifndef T114_FLAG_OPTO_POWER_OK
    #define T114_FLAG_OPTO_POWER_OK 0xAABBCC01UL
  #endif
  #ifndef T114_FLAG_OPTO_POWER_DOWN
    #define T114_FLAG_OPTO_POWER_DOWN 0xAABBCC00UL
  #endif
  #ifndef T114_OPTO_MIN_WRITE_MS
    #define T114_OPTO_MIN_WRITE_MS 2000
  #endif
  #ifndef T114_OPTO_REBOOT_ON_CHANGE
    #define T114_OPTO_REBOOT_ON_CHANGE 0  // [1=REBOOT, 0=NO REBOOT]
  #endif

class T114OptoFlagBridgeModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  T114OptoFlagBridgeModule();

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
  uint32_t tWriteGuard_ = 0;  // NO-DEBOUNCE WRITE
};

#endif  // BOARD_HELTEC_MESH_NODE_T114_V2_0