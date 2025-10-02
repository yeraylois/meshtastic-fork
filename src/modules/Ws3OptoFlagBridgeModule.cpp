/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Optoacoupler & Reboot (Heltec Wireless Stick V3) *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/

#include "Ws3OptoFlagBridgeModule.h"

#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)

  #define LOG_TAG "opto_flag_ws3"
  #include <esp_system.h>  // FOR REBOOT (esp_restart)

  #include "power/PowerMonitor.h"

  #include "configuration.h"
  #include "flags/Ws3FlagStore.h"

Ws3OptoFlagBridgeModule::Ws3OptoFlagBridgeModule()
    : SinglePortModule("OptoFlagModule_WS3", kPort), concurrency::OSThread("OptoFlagModule_WS3") {
  WS3_OPTOF_LOGI("CONSTRUCTOR_Ws3OptoFlagBridgeModule\n");
}

void Ws3OptoFlagBridgeModule::initOnce() {
  WS3_OPTOF_LOGI("SETUP (Opto→NVS Bridge)\n");

  // POWER MONITOR CONFIG
  PM_setDebounce(WS3_OPTO_PM_DEBOUNCE_MS);
  PM_invertLogic(false);
  PM_init(WS3_OPTO_PM_PIN, WS3_OPTO_PM_LED, WS3_OPTO_PM_PULLUP ? true : false);

  // INIT FLAG STORE
  Ws3FlagStore::begin();

  // INITIAL STATUS
  lastPowerOk_ = PM_isPowerOk();
  printStatus(lastPowerOk_);

  // IF THE FLAG IS STILL DEFAULT, INITIALIZE IT BASED ON REAL STATUS
  const uint32_t currentFlag = Ws3FlagStore::get();
  if (currentFlag == WS3_FLAG_DEFAULT) {
    const uint32_t v = lastPowerOk_ ? WS3_FLAG_OPTO_POWER_OK : WS3_FLAG_OPTO_POWER_DOWN;
    if (Ws3FlagStore::write(v)) {
      WS3_OPTOF_LOGI("[Opto→NVS] Initialized flag=0x%08" PRIX32 " (%s)\n",
                     v,
                     lastPowerOk_ ? "POWER_OK" : "POWER_DOWN");
    }
  } else {
    WS3_OPTOF_LOGI("[Opto→NVS] Existing flag: ");
    Ws3FlagStore::print();
  }

  tNextPrint_  = millis();
  tWriteGuard_ = 0;
  ready_       = true;
}

void Ws3OptoFlagBridgeModule::printStatus(bool powerOk) {
  if (powerOk) {
    WS3_OPTOF_LOGI("POWER OK\n");
  } else {
    WS3_OPTOF_LOGI("POWER DOWN! RUNNING ON BATTERY\n");
  }
}

void Ws3OptoFlagBridgeModule::handleEdge(bool powerOk) {
  const uint32_t now = millis();
  if ((int32_t) (now - tWriteGuard_) < 0)
    return;  // RESPECT MINIMUM PERIOD

  const uint32_t v = powerOk ? WS3_FLAG_OPTO_POWER_OK : WS3_FLAG_OPTO_POWER_DOWN;
  if (Ws3FlagStore::write(v)) {
    WS3_OPTOF_LOGI(
        "[Opto→NVS] Change → flag=0x%08" PRIX32 " (%s)\n", v, powerOk ? "POWER_OK" : "POWER_DOWN");
    Ws3FlagStore::print();
  #if WS3_OPTO_REBOOT_ON_CHANGE
    WS3_OPTOF_LOGI("[Opto→NVS] Rebooting due to state change...\n");
    delay(120);
    esp_restart();
  #endif
  } else {
    WS3_OPTOF_LOGI("[Opto→NVS] ERROR writing flag\n");
  }
  tWriteGuard_ = now + WS3_OPTO_MIN_WRITE_MS;
}

int32_t Ws3OptoFlagBridgeModule::runOnce() {
  if (!ready_)
    initOnce();

  PM_updateLED();
  const bool powerOk = PM_isPowerOk();

  // EDGE → WRITE NVS ONCE
  static bool prev = powerOk;
  if (powerOk != prev) {
    handleEdge(powerOk);
    prev = powerOk;
  }

  // PERIODIC LOG
  const uint32_t now = millis();
  if ((int32_t) (now - tNextPrint_) >= 0) {
    printStatus(powerOk);
    tNextPrint_ = now + WS3_OPTO_PM_PRINT_PERIOD_MS;
  }

  return 25;
}

#endif