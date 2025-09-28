/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Optoacoupler & Reboot (Heltec Mesh Node T114)    *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/

#include "T114OptoFlagBridgeModule.h"

#if defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)

  #define LOG_TAG "opto_flag_t114"
  #include "power/PowerMonitor.h"

  #include "configuration.h"
  #include "flags/T114FlagStore.h"  // GPREGRET2

extern "C" void NVIC_SystemReset(void);

T114OptoFlagBridgeModule::T114OptoFlagBridgeModule()
    : SinglePortModule("OptoFlagModule_T114", kPort), concurrency::OSThread("OptoFlagModule_T114") {
  T114_OPTOF_LOGI("CONSTRUCTOR_T114OptoFlagBridgeModule\n");
}

void T114OptoFlagBridgeModule::initOnce() {
  T114_OPTOF_LOGI("SETUP (Opto→Flag Bridge T114)\n");

  // POWER MONITOR CONFIG
  PM_setDebounce(T114_OPTO_PM_DEBOUNCE_MS);
  PM_invertLogic(false);
  PM_init(T114_OPTO_PM_PIN, T114_OPTO_PM_LED, T114_OPTO_PM_PULLUP ? true : false);

  // FLAG STORE
  T114FlagStore::begin();

  // INITIAL STATE
  lastPowerOk_ = PM_isPowerOk();
  printStatus(lastPowerOk_);

  // IF FLAG IS STILL DEFAULT → INITIALIZE ACCORDING TO REAL STATE
  const uint32_t currentFlag = T114FlagStore::get();
  if (currentFlag == T114_FLAG_DEFAULT) {
    const uint32_t v = lastPowerOk_ ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
    if (T114FlagStore::write(v)) {
      T114_OPTOF_LOGI("[Opto→Flag] Initialized flag=0x%08" PRIX32 " (%s)\n",
                      v,
                      lastPowerOk_ ? "POWER_OK" : "POWER_DOWN");
    }
  } else {
    T114_OPTOF_LOGI("[Opto→Flag] Existing flag: ");
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
    return;  // RESPECT WRITE GUARD

  const uint32_t v = powerOk ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
  if (T114FlagStore::write(v)) {
    T114_OPTOF_LOGI(
        "[Opto→Flag] Change → flag=0x%08" PRIX32 " (%s)\n", v, powerOk ? "POWER_OK" : "POWER_DOWN");
    T114FlagStore::print();
  #if T114_OPTO_REBOOT_ON_CHANGE
    T114_OPTOF_LOGI("[Opto→Flag] Rebooting due to state change...\n");
    delay(120);
    NVIC_SystemReset();
  #endif
  } else {
    T114_OPTOF_LOGI("[Opto→Flag] ERROR writing flag\n");
  }
  tWriteGuard_ = now + T114_OPTO_MIN_WRITE_MS;
}

int32_t T114OptoFlagBridgeModule::runOnce() {
  if (!ready_)
    initOnce();

  PM_updateLED();
  const bool powerOk = PM_isPowerOk();

  // EDGE DETECTION → WRITE ONCE
  static bool prev = powerOk;
  if (powerOk != prev) {
    handleEdge(powerOk);
    prev = powerOk;
  }

  // PERIODIC LOG
  const uint32_t now = millis();
  if ((int32_t) (now - tNextPrint_) >= 0) {
    printStatus(powerOk);
    tNextPrint_ = now + T114_OPTO_PM_PRINT_PERIOD_MS;
  }

  return 25;
}

#endif  // BOARD_HELTEC_MESH_NODE_T114_V2_0