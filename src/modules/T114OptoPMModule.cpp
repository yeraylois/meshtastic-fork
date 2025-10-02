/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Optoacoupler Check (Heltec Mesh Node T114)     *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#include "T114OptoPMModule.h"
#define LOG_TAG "T114_opto_pm"
#include "power/PowerMonitor.h"  // PM_init/PM_* LIBRARY

#include "configuration.h"
#include "error.h"

T114OptoPMModule::T114OptoPMModule()
    : SinglePortModule("opto_pm", kPort), concurrency::OSThread("opto_pm") {
  T114_OPTO_PM_LOGI("CONSTRUCTOR_T114OptoPMModule\n");
}

void T114OptoPMModule::initOnce() {
  T114_OPTO_PM_LOGI("SETUP (OptoPM, replica del sketch)\n");

  PM_setDebounce(T114_OPTO_PM_DEBOUNCE_MS);
  PM_invertLogic(false);
  PM_init(T114_OPTO_PM_PIN, T114_OPTO_PM_LED, T114_OPTO_PM_PULLUP ? true : false);

  T114_OPTO_PM_LOGI("▶ Monitoring power via PC817 (pin=%d, led=%d, pullup=%u, deb=%u)\n",
                    (int) T114_OPTO_PM_PIN,
                    (int) T114_OPTO_PM_LED,
                    (unsigned) T114_OPTO_PM_PULLUP,
                    (unsigned) T114_OPTO_PM_DEBOUNCE_MS);

  tNextPrint_ = millis();
  ready_      = true;
}

int32_t T114OptoPMModule::runOnce() {
  if (!ready_)
    initOnce();

  PM_updateLED();
  bool powerOk = PM_isPowerOk();

  uint32_t now = millis();
  if ((int32_t) (now - tNextPrint_) >= 0) {
    if (powerOk) {
      T114_OPTO_PM_LOGI("POWER OK\n");
    } else {
      T114_OPTO_PM_LOGI("POWER DOWN! RUNNING ON BATTERY\n");
    }
    tNextPrint_ = now + T114_OPTO_PM_PRINT_PERIOD_MS;
  }

  return 25;
}