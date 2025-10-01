/**************************************************************
 *  Project : Blackout Traffic Light System                   *
 *  File    : Test Environment Stubs (.inc, single test unit) *
 *  Author  : Yeray Lois Sanchez                              *
 *  Email   : yerayloissanchez@gmail.com                      *
 **************************************************************/

#include <cinttypes>
#include <cstdint>
#include <cstdio>

uint32_t                 g_now_ms = 0;
extern "C" unsigned long millis() {
  return g_now_ms;
}
extern "C" void delay(unsigned long ms) {
  g_now_ms += (uint32_t) ms;
}

/* =========================================
 *  MINIMAL MESHTASTIC-LIKE CORE DEFINITIONS
 * ========================================= */

typedef enum { meshtastic_PortNum_PRIVATE_APP = 999 } meshtastic_PortNum;

enum class ProcessMessage { CONTINUE, STOP };

/* ===================================
 *  MINIMAL SINGLE PORT MODULE STUB
 * =================================== */

class SinglePortModule {
public:
  SinglePortModule(const char* /*name*/, meshtastic_PortNum /*port*/) {}
  virtual ~SinglePortModule() = default;

  // DEFAULT NO-OP HANDLER: ALWAYS CONTINUES
  virtual ProcessMessage handleReceived(const void*) {
    return ProcessMessage::CONTINUE;
  }
};

/* =========================
 *  COOPERATIVE THREAD STUB
 * ========================= */

namespace concurrency {
class OSThread {
public:
  explicit OSThread(const char* /*name*/) {}
  virtual ~OSThread() = default;

  // DEFAULT NO-OP TICK
  virtual int32_t runOnce() {
    return 0;
  }
};
}  // namespace concurrency

/* ===============================
 *  POWER MONITOR STUB
 * =============================== */

uint32_t g_pm_debounce_ms = 50;
bool     g_pm_invert      = false;
int      g_pm_pin         = 33;
int      g_pm_led         = 7;
bool     g_pm_pullup      = false;
bool     g_pm_power_ok    = true;   // SIMULATED "MAINS OK" STATE
bool     g_pm_led_state   = false;  // SIMULATED STATUS LED
uint32_t g_pm_last_change = 0;

void PM_setDebounce(uint32_t ms) {
  g_pm_debounce_ms = ms;
}
void PM_invertLogic(bool inv) {
  g_pm_invert = inv;
}
void PM_init(int pin, int led, bool pullup) {
  g_pm_pin    = pin;
  g_pm_led    = led;
  g_pm_pullup = pullup;
}
bool PM_isPowerOkRaw() {
  return g_pm_power_ok ^ g_pm_invert;
}
bool PM_isPowerOk() {
  return PM_isPowerOkRaw();
}  // NO REAL DEBOUNCE IN TESTS
void PM_updateLED() {
  g_pm_led_state = PM_isPowerOk();
}
void PM_forcePowerOk(bool ok) {
  if (g_pm_power_ok != ok) {
    g_pm_power_ok    = ok;
    g_pm_last_change = millis();
  }
}

/* =================================
 *  T114 FLAG STORE (TEST DOUBLE)
 *  - INJECTABLE FAILURE SWITCH TO
 * ================================= */

namespace T114FlagStore {
uint32_t g_flag     = 0xCAFEBABEUL;  // DEFAULT FLAG
bool     g_hasBegun = false;

bool g_force_write_fail = false;  // FORCE WRITE ERROR WHEN TRUE
bool g_reset_on_begin   = false;

void begin() {
  if (g_hasBegun)
    return;
  g_hasBegun = true;
  if (g_reset_on_begin) {
    g_flag = 0xCAFEBABEUL;  // SIMULATE ERASE ON BEGIN
  }
}
uint32_t get() {
  return g_flag;
}
bool write(uint32_t v) {
  if (g_force_write_fail)
    return false;  // COVER HANDLEEDGE() ERROR ELSE-BRANCH
  g_flag = v;
  return true;
}
void print() {
  std::printf("Flag=0x%08" PRIX32 "\n", g_flag);
}
}  // namespace T114FlagStore

/* ==================================
 *  MCU RESET STUB (FOR REBOOT PATH)
 * ================================== */

bool            g_reset_called = false;
extern "C" void NVIC_SystemReset(void) {
  g_reset_called = true;
}