/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Optoacoupler Check (Heltec Wireless Stick V3)  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#include "Ws3OptoPMModule.h"
#define LOG_TAG "opto_pm_ws3"
#include "power/PowerMonitor.h"  // PM_init/PM_* LIBRARY

#include "configuration.h"
#include "error.h"

Ws3OptoPMModule::Ws3OptoPMModule()
    : SinglePortModule("OptoPmModule_WS3", kPort), concurrency::OSThread("OptoPmModule_WS3") {
  WS3_OPTO_LOGI("CONSTRUCTOR_Ws3OptoPMModule\n");
}

void Ws3OptoPMModule::initOnce() {
  WS3_OPTO_LOGI("SETUP (OptoPM WS3)\n");

  PM_setDebounce(WS3_OPTO_PM_DEBOUNCE_MS);
  PM_invertLogic(false);
  PM_init(WS3_OPTO_PM_PIN, WS3_OPTO_PM_LED, WS3_OPTO_PM_PULLUP ? true : false);

  WS3_OPTO_LOGI("▶ Monitoring power via PC817 (pin=%d, led=%d, pullup=%u, deb=%u)\n",
                (int) WS3_OPTO_PM_PIN,
                (int) WS3_OPTO_PM_LED,
                (unsigned) WS3_OPTO_PM_PULLUP,
                (unsigned) WS3_OPTO_PM_DEBOUNCE_MS);

  tNextPrint_ = millis();
  ready_      = true;
}

int32_t Ws3OptoPMModule::runOnce() {
  if (!ready_)
    initOnce();

  PM_updateLED();
  bool powerOk = PM_isPowerOk();

  const uint32_t now = millis();
  if ((int32_t) (now - tNextPrint_) >= 0) {
    if (powerOk) {
      WS3_OPTO_LOGI("POWER OK\n");
    } else {
      WS3_OPTO_LOGI("POWER DOWN! RUNNING ON BATTERY\n");
    }
    tNextPrint_ = now + WS3_OPTO_PM_PRINT_PERIOD_MS;
  }

  return 25;
}