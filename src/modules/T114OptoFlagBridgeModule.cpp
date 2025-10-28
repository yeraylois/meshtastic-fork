/**************************************************************
 *  PROJECT : BLACKOUT TRAFFIC LIGHT SYSTEM                    *
 *  MODULE  : OPTOCOUPLER & FLAG BRIDGE (HELTEC MESH NODE T114)*
 *  AUTHOR  : YERAY LOIS SANCHEZ                               *
 *  EMAIL   : YERAYLOISSANCHEZ@GMAIL.COM                       *
 ***************************************************************/

#include "T114OptoFlagBridgeModule.h"

#if defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)

  #define LOG_TAG "OPTO_FLAG_T114"
  #include "configuration.h"
  #include "flags/T114FlagStore.h"

extern "C" void NVIC_SystemReset(void);

/* ===== CONSTRUCTOR: SET THREAD NAMES AND PORT ===== */
T114OptoFlagBridgeModule::T114OptoFlagBridgeModule()
    : SinglePortModule("OPTOFLAGMODULE_T114", kPort), concurrency::OSThread("OPTOFLAGMODULE_T114") {
  T114_OPTOF_LOGI("CONSTRUCTOR: T114 OPTO→FLAG BRIDGE\n");
}

/* ===== ACTIVE-LOW GPIO READER: LOW = CABLE PRESENT (POWER OK) ===== */
static inline bool readActiveLow(uint8_t pin) {
  return (digitalRead(pin) == LOW);  // LOW = CABLE PRESENT (POWER OK)
}

/* ===== RAW READ WRAPPER (KEEPS SYMBOLIC NAME) ===== */
bool T114OptoFlagBridgeModule::readRaw() {
  return readActiveLow(T114_OPTO_PM_PIN);
}

/* ===== ONE-TIME INITIALIZATION ===== */
void T114OptoFlagBridgeModule::initOnce() {
  T114_OPTOF_LOGI("SETUP: OPTO→FLAG BRIDGE (T114)\n");

  pinMode(T114_OPTO_PM_PIN, INPUT_PULLUP);

  // FLAG STORE (GPREGRET2) INIT
  T114FlagStore::begin();

  // INITIAL STATE (BEFORE BOOT-BLIND WINDOW)
  stablePowerOk_ = readRaw();
  printStatus(stablePowerOk_);

  // INITIALIZE FLAG IF CURRENT VALUE IS DEFAULT
  uint32_t v0 = T114FlagStore::get();
  if (v0 == T114_FLAG_DEFAULT) {
    const uint32_t v = stablePowerOk_ ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
    if (T114FlagStore::write(v)) {
      T114_OPTOF_LOGI("[OPTO→FLAG] INITIALIZED FLAG=0x%08" PRIX32 " (%s)\n",
                      v,
                      stablePowerOk_ ? "POWER_OK" : "POWER_DOWN");
    }
  } else {
    T114_OPTOF_LOGI("[OPTO→FLAG] EXISTING FLAG: ");
    T114FlagStore::print();
  }

  // TIMERS AND COUNTERS
  const uint32_t now = millis();
  tBootBlindEnd_     = now + T114_OPTO_BOOT_BLIND_MS;
  tNextSample_       = now + T114_OPTO_SAMPLE_MS;
  tNextPrint_        = now + T114_OPTO_DEBUG_PRINT_MS;
  tWriteGuard_       = 0;
  tPinModeReassert_  = now;  // REASSERT IMMEDIATELY IN FIRST LOOP
  cntLoss_ = cntGain_ = 0;

  ready_ = true;
}

/* ===== READABLE STATUS LOG ===== */
void T114OptoFlagBridgeModule::printStatus(bool powerOk) {
  if (powerOk)
    T114_OPTOF_LOGI("STATUS: POWER OK (CABLE PRESENT)\n");
  else
    T114_OPTOF_LOGI("STATUS: POWER DOWN (RUNNING ON BATTERY)\n");
}

/* ===== HANDLE STABLE EDGE: WRITE FLAG (+ OPTIONAL REBOOT) ===== */
void T114OptoFlagBridgeModule::handleEdge(bool powerOk) {
  const uint32_t now = millis();
  if ((int32_t) (now - tWriteGuard_) < 0)
    return;  // RESPECT WRITE-GUARD INTERVAL

  const uint32_t v = powerOk ? T114_FLAG_OPTO_POWER_OK : T114_FLAG_OPTO_POWER_DOWN;
  if (T114FlagStore::write(v)) {
    T114_OPTOF_LOGI(
        "[OPTO→FLAG] CHANGE → FLAG=0x%08" PRIX32 " (%s)\n", v, powerOk ? "POWER_OK" : "POWER_DOWN");
    T114FlagStore::print();
  #if T114_OPTO_REBOOT_ON_CHANGE
    T114_OPTOF_LOGI("[OPTO→FLAG] REBOOTING DUE TO STATE CHANGE (AFTER N SAMPLES)...\n");
    delay(120);
    NVIC_SystemReset();
  #endif
  } else {
    T114_OPTOF_LOGI("[OPTO→FLAG] ERROR: FLAG WRITE FAILED\n");
  }
  tWriteGuard_ = now + T114_OPTO_MIN_WRITE_MS;
}

/* ===== MAIN PERIODIC LOGIC ===== */
int32_t T114OptoFlagBridgeModule::runOnce() {
  if (!ready_)
    initOnce();

  const uint32_t now = millis();

  // REASSERT INPUT_PULLUP PERIODICALLY
  if ((int32_t) (now - tPinModeReassert_) >= 0) {
    pinMode(T114_OPTO_PM_PIN, INPUT_PULLUP);
    tPinModeReassert_ = now + T114_OPTO_FORCE_PINMODE_MS;
  }

  #if T114_OPTO_BYPASS_FILTER
  /* ===== BYPASS MODE: MIRROR RAW STATE IMMEDIATELY (NO FLAGS/REBOOT) ===== */
  bool        raw  = readRaw();  // LOW = CABLE
  static bool prev = raw;
  if (raw != prev) {
    prev = raw;
    T114_OPTOF_LOGI("[RAW] PIN=%s (MIRROR ONLY, NO FLAG/REBOOT)\n",
                    raw ? "LOW (CABLE)" : "HIGH (BATTERY)");
  }
  if ((int32_t) (now - tNextPrint_) >= 0) {
    T114_OPTOF_LOGI("[RAW] PIN=%s\n", raw ? "LOW (CABLE)" : "HIGH (BATTERY)");
    tNextPrint_ = now + T114_OPTO_DEBUG_PRINT_MS;
  }

  #else
  /* ===== FILTERED MODE: N-GAIN / N-LOSS PERSISTENCE WITH PROGRESS LOGS ===== */

  static bool     wishPowerOk = stablePowerOk_;
  static uint32_t wishStartMs = 0;

  if ((int32_t) (now - tNextSample_) >= 0) {
    bool raw = readRaw();  // LOW = CABLE

    if ((int32_t) (now - tBootBlindEnd_) >= 0) {
      // TRACK TARGET STATE (WHAT INPUT "WANTS" TO BECOME)
      if (raw != wishPowerOk) {
        wishPowerOk = raw;
        cntGain_    = 0;
        cntLoss_    = 0;
        wishStartMs = now;
      }

      if (wishPowerOk) {
        // TENDING TO CABLE (POWER_OK)
        cntGain_++;
        cntLoss_ = 0;
        if (!stablePowerOk_ && cntGain_ >= T114_OPTO_N_GAIN) {
          stablePowerOk_         = true;
          const uint32_t elapsed = now - wishStartMs;
          T114_OPTOF_LOGI("[FILTER] STABLE → CABLE AFTER %UMS (%U/%U SAMPLES)\n",
                          (unsigned) elapsed,
                          (unsigned) cntGain_,
                          (unsigned) T114_OPTO_N_GAIN);
          handleEdge(true);  // OPTIONAL REBOOT OCCURS ONLY AFTER N SAMPLES
        }
      } else {
        // TENDING TO BATTERY (POWER_DOWN)
        cntLoss_++;
        cntGain_ = 0;
        if (stablePowerOk_ && cntLoss_ >= T114_OPTO_N_LOSS) {
          stablePowerOk_         = false;
          const uint32_t elapsed = now - wishStartMs;
          T114_OPTOF_LOGI("[FILTER] STABLE → BATTERY AFTER %UMS (%U/%U SAMPLES)\n",
                          (unsigned) elapsed,
                          (unsigned) cntLoss_,
                          (unsigned) T114_OPTO_N_LOSS);
          handleEdge(false);  // OPTIONAL REBOOT OCCURS ONLY AFTER N SAMPLES
        }
      }
    }

    tNextSample_ = now + T114_OPTO_SAMPLE_MS;
  }

  // PERIODIC FILTER PROGRESS / STATUS
  if ((int32_t) (now - tNextPrint_) >= 0) {
    if (wishPowerOk != stablePowerOk_) {
      const uint32_t elapsed = now - wishStartMs;
      if (wishPowerOk) {
        T114_OPTOF_LOGI("[FILTER] SETTLING → CABLE: GAIN=%U/%U  (%UMS/%UMS)\n",
                        (unsigned) cntGain_,
                        (unsigned) T114_OPTO_N_GAIN,
                        (unsigned) elapsed,
                        (unsigned) (T114_OPTO_N_GAIN * T114_OPTO_SAMPLE_MS));
      } else {
        T114_OPTOF_LOGI("[FILTER] SETTLING → BATTERY: LOSS=%U/%U  (%UMS/%UMS)\n",
                        (unsigned) cntLoss_,
                        (unsigned) T114_OPTO_N_LOSS,
                        (unsigned) elapsed,
                        (unsigned) (T114_OPTO_N_LOSS * T114_OPTO_SAMPLE_MS));
      }
    } else {
      printStatus(stablePowerOk_);
    }
    tNextPrint_ = now + T114_OPTO_DEBUG_PRINT_MS;
  }
  #endif

  return 25;
}

#endif  // BOARD_HELTEC_MESH_NODE_T114_V2_0