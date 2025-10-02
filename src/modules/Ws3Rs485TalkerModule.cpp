/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Talker RS485  (Heltec Wireless Stick V3)       *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#if defined(WS3_RS485_TALKER_ENABLE)

  #include "Ws3Rs485TalkerModule.h"
  #define LOG_TAG "ws3_rs485_master"
  #include <Arduino.h>
  #include <stdio.h>
  #include <string.h>

  #include "configuration.h"
  #if defined(ARDUINO_ARCH_ESP32)
    #include <HardwareSerial.h>
  #endif

  // ================ LOG GATE =================
  /*
    0 = OFF (background_thread_only),
    1 = INFO,
    2 = DEBUG
  */
  #ifndef WS3_RS485_LOG_LEVEL
    #define WS3_RS485_LOG_LEVEL 1
  #endif
  #if WS3_RS485_LOG_LEVEL >= 1
    #define WS3RS_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define WS3RS_LOGI(...)
  #endif
  #if WS3_RS485_LOG_LEVEL >= 2
    #define WS3RS_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define WS3RS_LOGD(...)
  #endif

// ================ CONSTRUCTOR =================
Ws3Rs485TalkerModule::Ws3Rs485TalkerModule()
    : SinglePortModule("Rs485Talker_WS3", kPort), concurrency::OSThread("Rs485Talker_WS3") {
  WS3RS_LOGI("CONSTRUCTOR_Ws3Rs485TalkerModule\n");
}

// ================     INIT    =================
void Ws3Rs485TalkerModule::initOnce() {
  WS3RS_LOGI("SETUP: Ws3Rs485TalkerModule\n");

  // DE/RE PINS
  pinMode(WS3_RS485_PIN_DIR, OUTPUT);

  setTx(false);  // RX MODE

  // UART
  beginUart();

  // LEDs
  pinMode(WS3_LED_RED_PIN, OUTPUT);
  pinMode(WS3_LED_AMBER_PIN, OUTPUT);
  pinMode(WS3_LED_GREEN_PIN, OUTPUT);

  t_bit_us_  = (1000000UL / WS3_RS485_BAUD);
  t_char_us_ = t_bit_us_ * 10;

  // INITIAL STATE
  applyCaseToMaster(caseIndex_);
  const uint32_t now = millis();
  tLastT_            = now;
  tLastCase_         = now;
  for (uint8_t i = 0; i < RS_NUM_SLAVES; ++i) {
    lastHB_[i] = now;
    online_[i] = false;
  }

  ready_ = true;

  WS3RS_LOGI("RS485 init: baud=%lu RX=GPIO%d TX=GPIO%d DIR=GPIO%d | LED(R,A,G)=(%d,%d,%d)\n",
             (unsigned long) WS3_RS485_BAUD,
             WS3_RS485_PIN_RX,
             WS3_RS485_PIN_TX,
             WS3_RS485_PIN_DIR,
             WS3_LED_RED_PIN,
             WS3_LED_AMBER_PIN,
             WS3_LED_GREEN_PIN);
}

void Ws3Rs485TalkerModule::beginUart() {
  // UART1 WITH EXPLICIT PIN MAPPING FOR ESP32-S3
  Serial1.begin(WS3_RS485_BAUD, SERIAL_8N1, WS3_RS485_PIN_RX, WS3_RS485_PIN_TX);
}

inline void Ws3Rs485TalkerModule::setTx(bool en) {
  digitalWrite(WS3_RS485_PIN_DIR, en ? HIGH : LOW);
}

// ================ UTILITIES =================
uint8_t Ws3Rs485TalkerModule::computeXOR(const uint8_t* data, size_t len) {
  uint8_t cs = 0;
  while (len--)
    cs ^= *data++;
  return cs;
}

void Ws3Rs485TalkerModule::sendFrame(const char* buf, size_t len) {
  setTx(true);
  delayMicroseconds(t_bit_us_ * 2);  // tDE ≈ 2 bit-times

  Serial1.write(reinterpret_cast<const uint8_t*>(buf), len);
  Serial1.flush();

  delayMicroseconds(t_char_us_);
  setTx(false);
  delayMicroseconds(t_bit_us_ * 2);
}

// ================ LOGIC CASES =================
uint8_t Ws3Rs485TalkerModule::greenNode(uint8_t c) {
  /*
   1 → slave1 (id=1),
   2 → master (id=0),
   3 → slave2 (id=2)
  */

  if (c == 1)
    return 1;
  if (c == 2)
    return 0;
  return 2;
}

void Ws3Rs485TalkerModule::applyCaseToMaster(uint8_t c) {
  digitalWrite(WS3_LED_RED_PIN, LOW);
  digitalWrite(WS3_LED_AMBER_PIN, LOW);
  digitalWrite(WS3_LED_GREEN_PIN, LOW);

  if (c == 2)
    digitalWrite(WS3_LED_GREEN_PIN, HIGH);  // MASTER: GREEN
  else
    digitalWrite(WS3_LED_RED_PIN, HIGH);  // MASTER: RED
}

void Ws3Rs485TalkerModule::applyAmberToMaster(uint8_t offNode) {
  // ONLY SHOW AMBER IF THE ONE TURNING OFF IS THE MASTER ITSELF
  if (offNode == 0) {
    digitalWrite(WS3_LED_RED_PIN, LOW);
    digitalWrite(WS3_LED_AMBER_PIN, HIGH);
    digitalWrite(WS3_LED_GREEN_PIN, LOW);
  }
}

// ================ RX =================
void Ws3Rs485TalkerModule::pumpRx() {
  while (Serial1.available()) {
    char c = (char) Serial1.read();

    if (c == '\n' || c == '\r') {
      if (rxLen_ > 0) {
        rxBuf_[rxLen_] = '\0';

        // FINAL TRIM
        while (rxLen_
               && (rxBuf_[rxLen_ - 1] == '\r' || rxBuf_[rxLen_ - 1] == ' '
                   || rxBuf_[rxLen_ - 1] == '\t'))
          rxBuf_[--rxLen_] = '\0';
        if (rxLen_)
          handleLine(rxBuf_);
        rxLen_ = 0;
      }
      continue;
    }

    if (rxLen_ < RX_MAX)
      rxBuf_[rxLen_++] = c;
    else {
      // OVERFLOW: DISCARD LINE TO AVOID CORRUPTING PARSING
      rxLen_ = 0;
    }
  }
}

// ================ HANDLE LINE =================
void Ws3Rs485TalkerModule::handleLine(const char* lineZ) {
  // WAIT "H,<id>,<cnt>*CS"
  if (strncmp(lineZ, "H,", 2) != 0)
    return;

  const char* p1 = strchr(lineZ + 2, ',');
  if (!p1)
    return;
  const char* p2 = strchr(p1 + 1, ',');
  (void) p2;  // reservado si decides usar el campo
  const char* star = strchr(p1 + 1, '*');
  if (!star)
    return;

  // EXTRACT ID AND CNT
  char tmp[16];

  // ID
  size_t n1 = (size_t) (p1 - (lineZ + 2));
  if (n1 >= sizeof(tmp))
    return;
  memcpy(tmp, lineZ + 2, n1);
  tmp[n1]    = '\0';
  uint8_t id = (uint8_t) strtoul(tmp, nullptr, 10);

  // CNT
  size_t n2 = (size_t) (star - (p1 + 1));
  if (n2 >= sizeof(tmp))
    return;
  memcpy(tmp, p1 + 1, n2);
  tmp[n2]      = '\0';
  uint16_t cnt = (uint16_t) strtoul(tmp, nullptr, 10);

  // CHECKSUM RECEIVED
  uint8_t csRecv = (uint8_t) strtoul(star + 1, nullptr, 16);

  // VERIFY XOR OF PAYLOAD (BEFORE '*')
  const size_t payLen = (size_t) (star - lineZ);
  if (computeXOR(reinterpret_cast<const uint8_t*>(lineZ), payLen) != csRecv) {
    WS3RS_LOGD("HB checksum mismatch: '%s'\n", lineZ);
    return;
  }

  const uint32_t now = millis();
  for (uint8_t i = 0; i < RS_NUM_SLAVES; ++i) {
    if (slaves_[i] == id) {
      lastHB_[i] = now;
      if (!online_[i]) {
        online_[i] = true;
        WS3RS_LOGI("Node %u reconnected\n", id);
      }
      WS3RS_LOGD("HB node %u cnt=%u\n", id, (unsigned) cnt);
      break;
    }
  }
}

int32_t Ws3Rs485TalkerModule::runOnce() {
  if (!ready_)
    initOnce();

  const uint32_t now = millis();

  // BROADCAST T COUNTER EVERY RS_T_INTERVAL_MS
  if ((now - tLastT_) >= RS_T_INTERVAL_MS) {
    tLastT_ = now;
    masterCounter_++;

    char buf[48];
    int  L = snprintf(buf, sizeof(buf), "T,0,%u", masterCounter_);
    if (L < 0)
      L = 0;
    uint8_t cs = computeXOR(reinterpret_cast<const uint8_t*>(buf), (size_t) L);
    int     K  = snprintf(buf + L, sizeof(buf) - (size_t) L, "*%02X\n", cs);
    if (K > 0) {
      size_t total = (size_t) (L + K);
      sendFrame(buf, total);
      WS3RS_LOGD("Master ► T,0,%u\n", masterCounter_);
    }
  }

  // TRANSITION TO AMBER PHASE
  if (!inAmberPhase_ && (now - tLastCase_) >= RS_CASE_INTERVAL_MS) {
    // INIT AMBER PHASE
    inAmberPhase_ = true;
    tAmberStart_  = now;
    nextCase_     = (uint8_t) ((caseIndex_ % 3) + 1);

    // CHECK WHICH NODE WAS GREEN
    uint8_t offNode = greenNode(caseIndex_);

    // PUT AMBER TO MASTER
    applyAmberToMaster(offNode);

    // SEND "A,<offNode>*CS"
    char aBuf[24];
    int  A = snprintf(aBuf, sizeof(aBuf), "A,%u", offNode);
    if (A < 0)
      A = 0;
    uint8_t csA = computeXOR(reinterpret_cast<const uint8_t*>(aBuf), (size_t) A);
    int     KA  = snprintf(aBuf + A, sizeof(aBuf) - (size_t) A, "*%02X\n", csA);
    if (KA > 0)
      sendFrame(aBuf, (size_t) (A + KA));

    WS3RS_LOGI("Master ► A,%u (AMBER)\n", offNode);
  } else if (inAmberPhase_ && (now - tAmberStart_) >= RS_AMBER_INTERVAL_MS) {
    // COMPLETE TRANSITION TO NEXT CASE
    inAmberPhase_ = false;
    caseIndex_    = nextCase_;
    tLastCase_    = now;

    // APPLY NEW CASE TO MASTER
    applyCaseToMaster(caseIndex_);
    WS3RS_LOGI("Master ► APPLY CASE %u\n", caseIndex_);

    // BROADCAST "S,<case>*CS"
    char sBuf[24];
    int  S = snprintf(sBuf, sizeof(sBuf), "S,%u", caseIndex_);
    if (S < 0)
      S = 0;
    uint8_t csS = computeXOR(reinterpret_cast<const uint8_t*>(sBuf), (size_t) S);
    int     KS  = snprintf(sBuf + S, sizeof(sBuf) - (size_t) S, "*%02X\n", csS);
    if (KS > 0) {
      sendFrame(sBuf, (size_t) (S + KS));
      WS3RS_LOGD("Master ► S,%u\n", caseIndex_);
    }
  }

  // PROCESS RECEIVED FRAMES
  pumpRx();

  // CHECK FOR TIMEOUTS
  if ((now - tDetect_) >= RS_T_INTERVAL_MS) {
    tDetect_ = now;
    for (uint8_t i = 0; i < RS_NUM_SLAVES; ++i) {
      if (online_[i] && (now - lastHB_[i]) > RS_HB_TIMEOUT_MS) {
        uint8_t id = slaves_[i];
        online_[i] = false;

        WS3RS_LOGI("Node %u DOWN → resync\n", id);

        char rBuf[48];
        int  R = snprintf(rBuf, sizeof(rBuf), "T,%u,%u", id, masterCounter_);
        if (R < 0)
          R = 0;
        uint8_t csR = computeXOR(reinterpret_cast<const uint8_t*>(rBuf), (size_t) R);
        int     KR  = snprintf(rBuf + R, sizeof(rBuf) - (size_t) R, "*%02X\n", csR);
        if (KR > 0)
          sendFrame(rBuf, (size_t) (R + KR));
      }
    }
  }

  return 10;  // 10 ms → LOW LATENCY NON-BLOCKING
}

#endif  // WS3_RS485_TALKER_ENABLE