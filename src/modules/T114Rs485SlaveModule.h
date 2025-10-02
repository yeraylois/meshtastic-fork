/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Slave RS485  (Heltec Mesh Node T114)           *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

#if defined(T114_RS485_SLAVE_ENABLE)

  #include <Arduino.h>

  #include "concurrency/OSThread.h"
  #include "mesh/SinglePortModule.h"
  #include "mesh/generated/meshtastic/portnums.pb.h"

  // ================== DEFAULT CONFIG (OVERRIDE -> -D ...) ==================
  #ifndef RS485_PIN_RX
    #define RS485_PIN_RX 9  // RX UART1
  #endif
  #ifndef RS485_PIN_TX
    #define RS485_PIN_TX 10  // TX UART1
  #endif
  #ifndef RS485_PIN_DIR
    #define RS485_PIN_DIR 8  // GPIO (DE & /RE)
  #endif
  #ifndef RS485_BAUD
    #define RS485_BAUD 9600
  #endif

  #ifndef LED_RED_PIN
    #define LED_RED_PIN 46
  #endif
  #ifndef LED_AMBER_PIN
    #define LED_AMBER_PIN 44
  #endif
  #ifndef LED_GREEN_PIN
    #define LED_GREEN_PIN 7
  #endif

  #ifndef NODE_ID
    #define NODE_ID 1  // [1..N]
  #endif

  #ifndef HB_INTERVAL_MS
    #define HB_INTERVAL_MS 1000  // HEARTBEAT INTERVAL
  #endif
// ==============================================================================

class T114Rs485SlaveModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  T114Rs485SlaveModule();

  // MESH-MODULE
  void initOnce();

  // OSTHREAD
  int32_t runOnce() override;

  // SinglePortModule (NOT CONSUME MESH TRAFFIC, FOR NOW)
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }

  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  // HELPERS
  inline void setTx(bool en);
  void        beginUart();

  // RS485
  void handleRx();
  void handleLine(const String& s);
  void sendFrame(const char* buf, size_t len);

  // SEMAPHORE LOGIC
  static uint8_t greenNode(uint8_t c);

  /*
   1     → node1 GREEN,
   2     → node2 GREEN,
   OTHER → NONE
  */
  void applyCase(uint8_t c);  // SET (GREEN, RED)
  void applyAmber();          // SET (AMBER)

  // UTILITIES
  static uint8_t computeXOR(const char* s, size_t n);

  // STATE
  bool   ready_ = false;
  String rxBuf_;

  // HEARTBEAT
  uint16_t localCounter_ = 0;
  uint32_t tLastHB_      = 0;

  // CURRENT CASE (1..3)
  uint8_t currentCase_ = 1;

  // TIMES (DERIVED FROM BAUDRATE)
  uint32_t t_bit_us_  = 0;
  uint32_t t_char_us_ = 0;
};

#endif  // T114_RS485_SLAVE_ENABLE