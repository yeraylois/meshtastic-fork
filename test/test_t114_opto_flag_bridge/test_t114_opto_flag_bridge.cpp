
/**************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   File    : T114 Opto Flag Bridge Module (Unit Test)       *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

// =============================
//  TEST BUILD CONFIGURATION
// =============================
#define BOARD_HELTEC_MESH_NODE_T114_V2_0 1

#ifndef T114_OPTO_REBOOT_ON_CHANGE
  #define T114_OPTO_REBOOT_ON_CHANGE 0
#endif

#define IS_RUNNING_TESTS 1

// =============================
//  STANDARD INCLUDES
// =============================
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

// ======================================
//  BRING TEST STUBS INTO THIS UNIT TEST
// ======================================
#include "test_env_stubs.inc"

// =============================
//  UNITY FRAMEWORK
// =============================
#include <unity.h>

// ====================================
//  NECESSARY MODULE CONSTANTS (PUBLIC)
// ====================================
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
#ifndef T114_OPTO_PM_DEBOUNCE_MS
  #define T114_OPTO_PM_DEBOUNCE_MS 50
#endif
#ifndef T114_OPTO_PM_PRINT_PERIOD_MS
  #define T114_OPTO_PM_PRINT_PERIOD_MS 500
#endif

/**************************************************************
 *  MODULE  : OPTOCOUPLER & REBOOT (HELTEC MESH NODE T114)
 **************************************************************/
#if defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)

class T114OptoFlagBridgeModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static const meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  T114OptoFlagBridgeModule()
      : SinglePortModule("OptoFlagModule_T114", kPort)
      , concurrency::OSThread("OptoFlagModule_T114") {
    std::printf("[I] CONSTRUCTOR_T114OptoFlagBridgeModule\n");
  }

  int32_t runOnce() override;

  ProcessMessage handleReceived(const void*) override {
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
  uint32_t tWriteGuard_ = 0;
};

static void T114_OPTOF_LOGI(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  std::vprintf(fmt, ap);
  va_end(ap);
}

static void T114_OPTOF_LOGD(const char*, ...) {}

/* ======================
 *  MODULE IMPLEMENTATION
 * ====================== */

void T114OptoFlagBridgeModule::initOnce() {
  T114_OPTOF_LOGI("SETUP (OPTO→FLAG BRIDGE T114)\n");

  PM_setDebounce(T114_OPTO_PM_DEBOUNCE_MS);
  PM_invertLogic(false);
  PM_init(/*pin*/ 33, /*led*/ 7, /*pullup*/ false);

  T114FlagStore::begin();

  lastPowerOk_ = PM_isPowerOk();
  printStatus(lastPowerOk_);

  const uint32_t currentFlag = T114FlagStore::get();
  if (currentFlag == T114_FLAG_DEFAULT) {
    const uint32_t v = lastPowerOk_ ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
    if (T114FlagStore::write(v)) {
      T114_OPTOF_LOGI("[OPTO→FLAG] INITIALIZED FLAG=0x%08" PRIX32 " (%s)\n",
                      v,
                      lastPowerOk_ ? "POWER_OK" : "POWER_DOWN");
    }
  } else {
    T114_OPTOF_LOGI("[OPTO→FLAG] EXISTING FLAG: ");
    T114FlagStore::print();
  }

  tNextPrint_  = millis();
  tWriteGuard_ = 0;
  ready_       = true;
}

void T114OptoFlagBridgeModule::printStatus(bool powerOk) {
  if (powerOk)
    T114_OPTOF_LOGI("POWER OK\n");
  else
    T114_OPTOF_LOGI("POWER DOWN! RUNNING ON BATTERY\n");
}

void T114OptoFlagBridgeModule::handleEdge(bool powerOk) {
  const uint32_t now = millis();
  if ((int32_t) (now - tWriteGuard_) < 0)
    return;

  const uint32_t v = powerOk ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
  if (T114FlagStore::write(v)) {
    T114_OPTOF_LOGI(
        "[OPTO→FLAG] CHANGE → FLAG=0x%08" PRIX32 " (%s)\n", v, powerOk ? "POWER_OK" : "POWER_DOWN");
    T114FlagStore::print();
  #if T114_OPTO_REBOOT_ON_CHANGE
    T114_OPTOF_LOGI("[OPTO→FLAG] REBOOTING DUE TO STATE CHANGE...\n");
    delay(120);
    NVIC_SystemReset();
  #endif
  } else {
    T114_OPTOF_LOGI("[OPTO→FLAG] ERROR WRITING FLAG\n");
  }
  tWriteGuard_ = now + T114_OPTO_MIN_WRITE_MS;
}

int32_t T114OptoFlagBridgeModule::runOnce() {
  if (!ready_)
    initOnce();

  PM_updateLED();
  const bool powerOk = PM_isPowerOk();

  if (powerOk != lastPowerOk_) {
    handleEdge(powerOk);
    lastPowerOk_ = powerOk;
  }

  const uint32_t now = millis();
  if ((int32_t) (now - tNextPrint_) >= 0) {
    printStatus(powerOk);
    tNextPrint_ = now + T114_OPTO_PM_PRINT_PERIOD_MS;
  }
  return 25;
}
#endif

/* =========================
 *  UNITY TESTS FOR MODULE
 * ========================= */

static void fast_forward(uint32_t ms) {
  g_now_ms += ms;
}

void setUp() {
  // RESET GLOBAL TEST STATE BEFORE EACH TEST
  g_now_ms                          = 0;
  T114FlagStore::g_flag             = T114_FLAG_DEFAULT;
  g_pm_led_state                    = false;
  g_reset_called                    = false;
  T114FlagStore::g_force_write_fail = false;
  g_pm_power_ok                     = true;
}

void tearDown() {}

/**
 * @brief INIT: DEFAULT FLAG + POWER OK → WRITES POWER_OK
 */
void test_init_writes_default_flag_power_ok() {
  T114FlagStore::g_flag = T114_FLAG_DEFAULT;
  g_pm_power_ok         = true;
  T114OptoFlagBridgeModule m;
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_OK, T114FlagStore::get());
  TEST_ASSERT_TRUE(g_pm_led_state);
}

/**
 * @brief INIT: DEFAULT FLAG + POWER DOWN → WRITES POWER_DOWN
 */
void test_init_writes_default_flag_power_down() {
  T114FlagStore::g_flag = T114_FLAG_DEFAULT;
  g_pm_power_ok         = false;  // FORCE LINE DOWN BEFORE FIRST RUNONCE
  T114OptoFlagBridgeModule m;
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_DOWN, T114FlagStore::get());
  TEST_ASSERT_FALSE(g_pm_led_state);
}

/**
 * @brief INIT: EXISTING FLAG → PRESERVE
 */
void test_init_keeps_existing_flag() {
  T114FlagStore::g_hasBegun       = true;
  T114FlagStore::g_reset_on_begin = false;
  T114FlagStore::g_flag           = 0xDEADBEEFUL;

  T114OptoFlagBridgeModule m;
  m.runOnce();

  TEST_ASSERT_EQUAL_UINT32(0xDEADBEEFUL, T114FlagStore::get());
}

/**
 * @brief EDGE WRITE + GUARD BEHAVIOR
 * - DOWN EDGE WRITES DOWN
 * - WITHIN GUARD: IGNORE
 * - AFTER GUARD: NEW EDGE WRITES AGAIN
 *
 */
void test_edge_write_and_guard() {
  T114FlagStore::g_flag = T114_FLAG_OPTO_POWER_OK;
  T114OptoFlagBridgeModule m;
  m.runOnce();  // STARTS IN OK

  PM_forcePowerOk(false);  // OK → DOWN
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_DOWN, T114FlagStore::get());

  PM_forcePowerOk(true);  // BACK TO OK, STILL WITHIN GUARD → NO WRITE
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_DOWN, T114FlagStore::get());

  fast_forward(T114_OPTO_MIN_WRITE_MS + 1);

  PM_forcePowerOk(false);  // NEW DOWN EDGE AFTER GUARD
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_DOWN, T114FlagStore::get());

  fast_forward(T114_OPTO_MIN_WRITE_MS + 1);

  PM_forcePowerOk(true);  // DOWN → OK AFTER GUARD → WRITES OK
  m.runOnce();
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_OK, T114FlagStore::get());
}

/**
 * @brief PERIODIC LOG MUST NOT ALTER FLAG
 */
void test_periodic_log_no_extra_writes() {
  T114OptoFlagBridgeModule m;
  m.runOnce();  // INIT
  const uint32_t before = T114FlagStore::get();

  for (int i = 0; i < 5; ++i) {
    fast_forward(T114_OPTO_PM_PRINT_PERIOD_MS);
    m.runOnce();
  }
  TEST_ASSERT_EQUAL_UINT32(before, T114FlagStore::get());
}

/**
 * @brief PORT NUMBER IS PRIVATE APP
 */
void test_get_port_num_is_private() {
  T114OptoFlagBridgeModule m;
  TEST_ASSERT_EQUAL_INT((int) meshtastic_PortNum_PRIVATE_APP, (int) m.getPortNum());
}

/**
 * @brief REBOOT BRANCH (ONLY WHEN COMPILED WITH REBOOT ENABLED)
 */
void test_reboot_branch_if_enabled() {
#if T114_OPTO_REBOOT_ON_CHANGE
  T114OptoFlagBridgeModule m;
  m.runOnce();
  PM_forcePowerOk(false);
  fast_forward(T114_OPTO_MIN_WRITE_MS + 1);
  m.runOnce();
  TEST_ASSERT_TRUE_MESSAGE(g_reset_called, "NVIC_SystemReset WAS NOT INVOKED");
#else
  TEST_IGNORE_MESSAGE("BUILD WITH -D T114_OPTO_REBOOT_ON_CHANGE=1 TO COVER REBOOT BRANCH.");
#endif
}

/**
 * @brief HANDLE RECEIVED MUST RETURN CONTINUE
 */
void test_handle_received_returns_continue() {
  T114OptoFlagBridgeModule m;
  auto                     r = m.handleReceived(nullptr);
  TEST_ASSERT_EQUAL_INT((int) ProcessMessage::CONTINUE, (int) r);
}

/**
 * @brief HANDLE EDGE WRITE ERROR BRANCH: FORCE WRITE FAILURE
 */
void test_handle_edge_write_error_branch_else() {
  T114FlagStore::g_flag = T114_FLAG_OPTO_POWER_OK;
  T114OptoFlagBridgeModule m;
  m.runOnce();  // INIT

  // FORCE ERROR AND GENERATE DOWN EDGE AFTER GUARD
  T114FlagStore::g_force_write_fail = true;
  PM_forcePowerOk(false);
  fast_forward(T114_OPTO_MIN_WRITE_MS + 1);
  m.runOnce();

  // FLAG MUST REMAIN UNCHANGED (OK) → ELSE-BRANCH EXECUTED
  TEST_ASSERT_EQUAL_UINT32(T114_FLAG_OPTO_POWER_OK, T114FlagStore::get());

  // CLEANUP
  T114FlagStore::g_force_write_fail = false;
}

/**
 * @brief COVER DEBUG LOG FUNCTION
 */
void test_cover_debug_log_function() {
  T114_OPTOF_LOGD("COVER-DEBUG %d\n", 1);
}

/* =============================
 *  UNITY TEST RUNNER (MAIN)
 * ============================= */

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_init_writes_default_flag_power_ok);
  RUN_TEST(test_init_writes_default_flag_power_down);
  RUN_TEST(test_init_keeps_existing_flag);
  RUN_TEST(test_edge_write_and_guard);
  RUN_TEST(test_periodic_log_no_extra_writes);
  RUN_TEST(test_get_port_num_is_private);
  RUN_TEST(test_reboot_branch_if_enabled);
  RUN_TEST(test_handle_received_returns_continue);
  RUN_TEST(test_handle_edge_write_error_branch_else);
  RUN_TEST(test_cover_debug_log_function);
  return UNITY_END();
}