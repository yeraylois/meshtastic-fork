/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Optocoupler & Flag Bridge (Heltec Mesh Node T114)*
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 **************************************************************/
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

  // ======= PIN / CONFIG (T114 v2.0) =======
  #ifndef T114_OPTO_PM_PIN
    #define T114_OPTO_PM_PIN 33  // OPTOCOUPLER COLLECTOR PIN (GPIO33)
  #endif

  // ======= DIAG / BYPASS =======
  #ifndef T114_OPTO_BYPASS_FILTER
    #define T114_OPTO_BYPASS_FILTER 0  // [0=USE FILTER, 1=MIRROR RAW(NOT RECOMMENDED)]
  #endif
  #ifndef T114_OPTO_FORCE_PINMODE_MS
    #define T114_OPTO_FORCE_PINMODE_MS 100  // PERIODIC RE-ASSERTION OF PINMODE
  #endif
  #ifndef T114_OPTO_DEBUG_PRINT_MS
    #define T114_OPTO_DEBUG_PRINT_MS 500  // PERIODIC DEBUG PRINT
  #endif

  // ======= FLAGS =======
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
    #define T114_OPTO_MIN_WRITE_MS 2000  // GUARD TIME BETWEEN FLAG WRITES (ms)
  #endif
  #ifndef T114_OPTO_REBOOT_ON_CHANGE
    #define T114_OPTO_REBOOT_ON_CHANGE 1  // REBOOT ONLY AFTER N SAMPLES CONFIRM CHANGE
  #endif

  // ======= PERSISTENCE FILTER (N samples + boot blind) =======
  #ifndef T114_OPTO_SAMPLE_MS
    #define T114_OPTO_SAMPLE_MS 50  // SAMPLING PERIOD (MS)
  #endif
  #ifndef T114_OPTO_N_LOSS
    #define T114_OPTO_N_LOSS 60  // ~3.0 s to accept 'BATTERY'
  #endif
  #ifndef T114_OPTO_N_GAIN
    #define T114_OPTO_N_GAIN 20  // ~1.0 s to accept 'CABLE'
  #endif
  #ifndef T114_OPTO_BOOT_BLIND_MS
    #define T114_OPTO_BOOT_BLIND_MS 1000  // IGNORE EARLY TRANSIENTS AT BOOT (MS)
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
  bool readRaw();  // LOW = CABLE, HIGH = BATTERY

  bool ready_ = false;

  // STABLE DECISION AND COUNTERS
  bool     stablePowerOk_ = true;
  uint16_t cntLoss_       = 0;
  uint16_t cntGain_       = 0;

  // TIMERS
  uint32_t tNextSample_      = 0;
  uint32_t tBootBlindEnd_    = 0;
  uint32_t tNextPrint_       = 0;
  uint32_t tWriteGuard_      = 0;
  uint32_t tPinModeReassert_ = 0;
};

#endif