/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Talker RS485  (Heltec Wireless Stick V3)       *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

// IF THE MODULE IS NOT ENABLED, WE PROVIDE A HARMLESS STUB.
#if !defined(WS3_RS485_TALKER_ENABLE)

class Ws3Rs485TalkerModule {
public:
  Ws3Rs485TalkerModule() = default;
};

#else  // ===== REAL MODULE ENABLED =====

  #include <Arduino.h>

  #include "concurrency/OSThread.h"
  #include "mesh/SinglePortModule.h"
  #include "mesh/generated/meshtastic/portnums.pb.h"

  // ======= DEFAULT PINS FOR Heltec Wireless Stick V3 (ESP32-S3) =======
  #ifndef WS3_RS485_PIN_RX
    #define WS3_RS485_PIN_RX 34
  #endif
  #ifndef WS3_RS485_PIN_TX
    #define WS3_RS485_PIN_TX 33
  #endif
  #ifndef WS3_RS485_PIN_DIR
    #define WS3_RS485_PIN_DIR 21  // DE/RE (HIGH = TX, LOW = RX)
  #endif
  #ifndef WS3_RS485_BAUD
    #define WS3_RS485_BAUD 9600
  #endif

  // MASTER LEDs (Red, Amber, Green) IN WSV3
  #ifndef WS3_LED_RED_PIN
    #define WS3_LED_RED_PIN 47
  #endif
  #ifndef WS3_LED_AMBER_PIN
    #define WS3_LED_AMBER_PIN 48
  #endif
  #ifndef WS3_LED_GREEN_PIN
    #define WS3_LED_GREEN_PIN 46
  #endif

  // ======= TIMINGS (ms) =======
  #ifndef RS_HB_TIMEOUT_MS
    #define RS_HB_TIMEOUT_MS 3000
  #endif
  #ifndef RS_T_INTERVAL_MS
    #define RS_T_INTERVAL_MS 1000
  #endif
  #ifndef RS_CASE_INTERVAL_MS
    #define RS_CASE_INTERVAL_MS 15000
  #endif
  #ifndef RS_AMBER_INTERVAL_MS
    #define RS_AMBER_INTERVAL_MS 3000
  #endif

  // ======= SLAVES =======
  #ifndef RS_NUM_SLAVES
    #define RS_NUM_SLAVES 2
  #endif

class Ws3Rs485TalkerModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  Ws3Rs485TalkerModule();

  // CORPORATIVE THREAD
  int32_t runOnce() override;

  // NO MESH TRAFFIC HANDLING
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  // INITIALIZATION
  void        initOnce();
  void        beginUart();
  inline void setTx(bool en);

  // RS-485 HELPERS
  static uint8_t computeXOR(const uint8_t* data, size_t len);
  void           sendFrame(const char* buf, size_t len);

  // LOGIC CASES / LED
  static uint8_t greenNode(uint8_t c);
  /*
   1 → slave1 (id=1),
   2 → master (id=0),
   3 → slave2 (id=2)
  */
  void applyCaseToMaster(uint8_t c);
  void applyAmberToMaster(uint8_t offNode);

  // RX
  void pumpRx();
  void handleLine(const char* lineZ);  // PROCESS "H,<id>,<cnt>*CS"

  // GENERAL STATE
  bool ready_ = false;

  // COMMUNICATION
  uint32_t                t_bit_us_  = 0;  // 1 BIT AT THIS BAUD
  uint32_t                t_char_us_ = 0;  // 1 CHARACTER (8N1 ~10 BITS)
  static constexpr size_t RX_MAX     = 192;
  char                    rxBuf_[RX_MAX + 1]{};
  size_t                  rxLen_ = 0;

  // SLAVES
  uint8_t  slaves_[RS_NUM_SLAVES] = {1, 2};
  uint32_t lastHB_[RS_NUM_SLAVES]{};
  bool     online_[RS_NUM_SLAVES]{};

  // MASTER VARIABLES
  uint16_t masterCounter_ = 0;
  uint32_t tLastT_        = 0;
  uint32_t tLastCase_     = 0;
  uint32_t tAmberStart_   = 0;
  uint32_t tDetect_       = 0;
  uint8_t  caseIndex_     = 1;  // (1..3)
  uint8_t  nextCase_      = 1;
  bool     inAmberPhase_  = false;
};

#endif