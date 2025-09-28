/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Slave RS485  (Heltec Mesh Node T114)           *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#if defined(T114_RS485_SLAVE_ENABLE)

  #include "T114Rs485SlaveModule.h"
  #define LOG_TAG "t114_rs485_slave"
  #include "configuration.h"

  // ================ LOG GATE =================
  /*
    0 = OFF (background_thread_only),
    1 = INFO,
    2 = DEBUG
  */
  #ifndef T114_RS485_LOG_LEVEL
    #define T114_RS485_LOG_LEVEL 1
  #endif
  #if T114_RS485_LOG_LEVEL >= 1
    #define T114_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define T114_LOGI(...)
  #endif
  #if T114_RS485_LOG_LEVEL >= 2
    #define T114_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define T114_LOGD(...)
  #endif

T114Rs485SlaveModule::T114Rs485SlaveModule()
    : SinglePortModule("Rs485Slave_T114", kPort), concurrency::OSThread("Rs485Slave_T114") {
  T114_LOGI("CONSTRUCTOR_T114Rs485SlaveModule\n");
}

void T114Rs485SlaveModule::initOnce() {
  T114_LOGI("SETUP_T114Rs485SlaveModule\n");

  // RS485 PIN DIRECTION
  pinMode(RS485_PIN_DIR, OUTPUT);
  setTx(false);  // 'RX' BY DEFAULT

  // LEDs
  pinMode(LED_RED_PIN, OUTPUT);
  pinMode(LED_AMBER_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_AMBER_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);

  // UART
  beginUart();

  // TIMES (8N1 ≈ 10 bits per character)
  t_bit_us_  = (1000000UL / RS485_BAUD);
  t_char_us_ = t_bit_us_ * 10;

  // INITIAL STATE
  applyCase(currentCase_);
  tLastHB_ = millis();

  ready_ = true;

  T114_LOGI("RS485 init: baud=%lu RX=%d TX=%d DIR=%d, NODE_ID=%d\n",
            (unsigned long) RS485_BAUD,
            RS485_PIN_RX,
            RS485_PIN_TX,
            RS485_PIN_DIR,
            NODE_ID);
}

void T114Rs485SlaveModule::beginUart() {
  #if defined(ARCH_ESP32)
  // IN ESP32 WE CAN PASS PINS TO BEGIN (IN CASE IT IS EVER PORTED)
  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_PIN_RX, RS485_PIN_TX);
  #else
  // IN nRF52 (Heltec T114) WE HAVE setPins()
  Serial1.setPins(RS485_PIN_RX, RS485_PIN_TX);
  Serial1.begin(RS485_BAUD);
  #endif
}

inline void T114Rs485SlaveModule::setTx(bool en) {
  digitalWrite(RS485_PIN_DIR, en ? HIGH : LOW);
}

uint8_t T114Rs485SlaveModule::computeXOR(const char* s, size_t n) {
  uint8_t cs = 0;
  for (size_t i = 0; i < n; ++i)
    cs ^= (uint8_t) s[i];
  return cs;
}

uint8_t T114Rs485SlaveModule::greenNode(uint8_t c) {
  if (c == 1)
    return 1;
  if (c == 2)
    return 0;
  return 2;  // NONE
}

void T114Rs485SlaveModule::applyCase(uint8_t c) {
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_AMBER_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);

  if (greenNode(c) == NODE_ID) {
    digitalWrite(LED_GREEN_PIN, HIGH);
  } else {
    digitalWrite(LED_RED_PIN, HIGH);
  }
}

void T114Rs485SlaveModule::applyAmber() {
  digitalWrite(LED_RED_PIN, LOW);
  digitalWrite(LED_AMBER_PIN, HIGH);
  digitalWrite(LED_GREEN_PIN, LOW);
}

void T114Rs485SlaveModule::sendFrame(const char* buf, size_t len) {
  setTx(true);
  delayMicroseconds(t_bit_us_ * 2);

  Serial1.write((const uint8_t*) buf, len);
  Serial1.flush();

  delayMicroseconds(t_char_us_);
  setTx(false);
  delayMicroseconds(t_bit_us_ * 2);
}

void T114Rs485SlaveModule::handleLine(const String& line) {
  /* FORMATS:
   * ---------------
   * "A,<ID>*<CS>"
   * "S,<CASE>*<CS>"
   */
  int comma = line.indexOf(',');
  int star  = line.indexOf('*');
  if (comma < 0 || star < 0 || star <= comma + 1)
    return;

  String cmd    = line.substring(0, comma);
  String argStr = line.substring(comma + 1, star);
  String csHex  = line.substring(star + 1);

  // CALCULATE 'XOR' OF THE "PAYLOAD" (EVERYTHING BEFORE THE '*')
  uint8_t csCalc = computeXOR(line.c_str(), (size_t) star);
  uint8_t csRecv = (uint8_t) strtoul(csHex.c_str(), nullptr, 16);

  T114_LOGD("RX ‹%s› csCalc=0x%02X csRecv=0x%02X\n", line.c_str(), csCalc, csRecv);
  if (csCalc != csRecv)
    return;

  if (cmd == "A") {
    uint8_t id = (uint8_t) strtoul(argStr.c_str(), nullptr, 10);
    if (id == NODE_ID) {
      applyAmber();
      T114_LOGI("✓ AMBER ON (id=%u)\n", id);
    }
    return;
  }

  if (cmd == "S") {
    uint8_t c    = (uint8_t) strtoul(argStr.c_str(), nullptr, 10);
    currentCase_ = c;
    applyCase(c);
    T114_LOGI("✓ CASE %u APPLIED\n", c);
    return;
  }
}

void T114Rs485SlaveModule::handleRx() {
  while (Serial1.available()) {
    char ch = (char) Serial1.read();
    if (ch == '\n' || ch == '\r') {
      if (rxBuf_.length()) {
        String line = rxBuf_;
        rxBuf_.remove(0);
        line.trim();
        if (line.length())
          handleLine(line);
      }
    } else {
      if (rxBuf_.length() < 240)
        rxBuf_ += ch;
      else
        rxBuf_.remove(0);
    }
  }
}

int32_t T114Rs485SlaveModule::runOnce() {
  static uint32_t tAlive = 0;

  if (!ready_)
    initOnce();

  handleRx();

  const uint32_t now = millis();

  // HEARTBEAT
  if (now - tLastHB_ >= HB_INTERVAL_MS) {
    tLastHB_ = now;
    localCounter_++;

    char payload[32];
    int  L = snprintf(payload, sizeof(payload), "H,%u,%u", NODE_ID, localCounter_);
    if (L < 0)
      L = 0;
    if ((size_t) L > sizeof(payload))
      L = sizeof(payload);

    uint8_t cs = computeXOR(payload, (size_t) L);

    char frame[48];
    int  F = snprintf(frame, sizeof(frame), "%s*%02X\n", payload, cs);
    if (F < 0)
      F = 0;
    if ((size_t) F > sizeof(frame))
      F = sizeof(frame);

    sendFrame(frame, (size_t) F);
    T114_LOGD("► HB %u (node %u)\n", localCounter_, NODE_ID);
  }

  // ALIVE MESSAGE (~3s)
  if (now - tAlive > 3000) {
    T114_LOGI("alive: %lu ms\n", (unsigned long) now);
    tAlive = now;
  }

  // THREAD PERIOD
  return 25;  // ms
}

#endif  // T114_RS485_SLAVE_ENABLE