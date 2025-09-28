/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Power Budget                                     *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/

#include "PowerBudgetModule.h"
#define LOG_TAG "power_budget"
#include <Adafruit_INA219.h>
#include <Wire.h>

#include "configuration.h"

/**************************************************************
 *             BUILD-TIME CONFIGURATION (MACROS)              *
 *                                                            *
 *   - Board-specific I2C mapping and INA219 address.         *
 *   - Sampling/printing cadence and battery model.           *
 **************************************************************/

/**
 * I2C BUS SELECTION
 *
 * - ON T114 (nRF52), WE USE THE GLOBAL 'Wire' PROVIDED BY THE FIRMWARE.
 * - ON WS3 (ESP32-S3), WE USE A DEDICATED BUS (TwoWire(1)) ON THE
 *   PINS DEFINED BELOW.
 *
 * NOTE: KEEP SDA/SCL CONSISTENT WITH YOUR BOARD PINOUT.
 */

/**
 * I2C PINOUT (LOGICAL)
 *
 * HELTEC MESH NODE T114 V2.0:
 *   SDA: GPIO 13
 *   SCL: GPIO 16
 *
 * HELTEC WIRELESS STICK V3 (ESP32-S3):
 *   SDA: GPIO 33
 *   SCL: GPIO 34
 */
#ifndef PB_I2C_SDA
  #define PB_I2C_SDA 13
#endif
#ifndef PB_I2C_SCL
  #define PB_I2C_SCL 16
#endif

/**
 * INA219 I2C ADDRESS
 * DEFAULT IS 0x40 UNLESS YOUR BOARD IS STRAPPED DIFFERENTLY.
 */
#ifndef PB_INA_ADDR
  #define PB_INA_ADDR 0x40
#endif

/**
 * RUNTIME CADENCE (MILLISECONDS)
 * - PB_SAMPLE_MS : SENSOR SAMPLING PERIOD
 * - PB_PRINT_MS  : LIVE PRINT INTERVAL
 * - PB_SUMMARY_MS: SUMMARY PRINT INTERVAL
 */
#ifndef PB_SAMPLE_MS
  #define PB_SAMPLE_MS 100  // ~10 Hz
#endif
#ifndef PB_PRINT_MS
  #define PB_PRINT_MS 1000  // LIVE PRINT EVERY 1 S
#endif
#ifndef PB_SUMMARY_MS
  #define PB_SUMMARY_MS 60000  // SUMMARY EVERY 60 S
#endif

/**
 * BATTERY POWER MODEL
 *
 * USED TO ESTIMATE AUTONOMY FROM AVERAGE POWER DRAW.
 * - PB_BATT_CAP_MAH: NOMINAL CAPACITY (mAh)
 * - PB_VBAT_NOM_V  : NOMINAL VOLTAGE (V)
 * - PB_REG_EFF     : REGULATOR EFFICIENCY (0.85–0.95 TYPICAL)
 */
#ifndef PB_BATT_CAP_MAH
  #define PB_BATT_CAP_MAH 2000.0f
#endif
#ifndef PB_VBAT_NOM_V
  #define PB_VBAT_NOM_V 3.70f
#endif
#ifndef PB_REG_EFF
  #define PB_REG_EFF 0.90f
#endif

/**************************************************************
 *   LOGGING CONFIGURATION                                    *
 *   - PB_LOG_LEVEL: [0=OFF, 1=INFO, 2=DEBUG]                 *
 *   - LOG_TAG: "power_budget"                                *
 *                                                            *
 *   USE LOG_INFO() AND LOG_DEBUG() FOR LOGGING.              *
 **************************************************************/
#ifndef PB_LOG_LEVEL
  #define PB_LOG_LEVEL 1
#endif
#if PB_LOG_LEVEL >= 1
  #define PB_LOGI(...) LOG_INFO(__VA_ARGS__)
#else
  #define PB_LOGI(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif
#if PB_LOG_LEVEL >= 2
  #define PB_LOGD(...) LOG_DEBUG(__VA_ARGS__)
#else
  #define PB_LOGD(...)                                                                             \
    do {                                                                                           \
    } while (0)
#endif

static Adafruit_INA219 gIna(PB_INA_ADDR);
static bool            gPrintedHeader = false;

/**************************************************************
 *                 I2C BUS BINDING PER BOARD                  *
 *   - WS3: DEDICATED Wire1 ON USER PINS.                     *
 *   - T114: FIRMWARE GLOBAL Wire.                            *
 **************************************************************/
#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)
static TwoWire pbWire(1);
  #define PB_WIRE pbWire
#elif defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)
  #define PB_WIRE Wire
#else
  #error                                                                                           \
      "Unsupported board: define BOARD_HELTEC_WIRELESS_STICK_V3 or BOARD_HELTEC_MESH_NODE_T114_V2_0"
#endif

/**************************************************************
 *                    CLASS IMPLEMENTATION                    *
 **************************************************************/

PowerBudgetModule::PowerBudgetModule()
    : SinglePortModule("PowerBudget", kPort), concurrency::OSThread("PowerBudget") {
  PB_LOGI("CONSTRUCTOR_PowerBudgetModule\n");
}

/**
 * PRINT A SINGLE STARTUP BANNER WITH EFFECTIVE CONFIGURATION.
 */
void PowerBudgetModule::printHeaderOnce() {
  if (gPrintedHeader)
    return;
  gPrintedHeader = true;

  PB_LOGI("\n================ POWER BUDGET (INA219) ================\n");
#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)
  PB_LOGI("WS3 (ESP32-S3): DEDICATED I2C BUS = Wire1  SDA=%d  SCL=%d  addr=0x%02X\n",
          (int) PB_I2C_SDA,
          (int) PB_I2C_SCL,
          (unsigned) PB_INA_ADDR);
#elif defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)
  PB_LOGI("T114 (nRF52): USING FIRMWARE GLOBAL Wire (BOARD DEFAULT PINS)  addr=0x%02X\n",
          (unsigned) PB_INA_ADDR);
#endif
  PB_LOGI("CADENCE: Sample=%u ms  Live=%u ms  Summary=%u ms\n",
          (unsigned) PB_SAMPLE_MS,
          (unsigned) PB_PRINT_MS,
          (unsigned) PB_SUMMARY_MS);
  PB_LOGI("BATTERY MODEL: C=%.0f mAh  V=%.2f V  Eff=%.0f%%\n",
          (double) PB_BATT_CAP_MAH,
          (double) PB_VBAT_NOM_V,
          (double) (PB_REG_EFF * 100.0f));
  PB_LOGI("\n=======================================================\n");
}

/**
 * INITIALIZE THE MODULE ONCE.
 *
 * - SET UP I2C BUS AND INA219 SENSOR.
 * - PRINT INITIAL HEADER.
 * - SET INITIAL TIMERS AND STATE VARIABLES.
 * - PRINT INITIAL STATUS
 */
void PowerBudgetModule::initOnce() {
#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)
  PB_WIRE.begin(PB_I2C_SDA, PB_I2C_SCL, 100000);
#elif defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)
  PB_WIRE.end();  // ENSURE START IN A FRESH STATE
  PB_WIRE.setPins(PB_I2C_SDA, PB_I2C_SCL);
  PB_WIRE.begin();
  PB_WIRE.setClock(100000);
#endif
  delay(5);

#if PB_LOG_LEVEL >= 2
  for (uint8_t a = 1; a < 127; ++a) {
    PB_WIRE.beginTransmission(a);
    if (PB_WIRE.endTransmission() == 0) {
      PB_LOGD("[I2C] Found device at 0x%02X\n", a);
    }
  }
#endif

  // INIT INA219 ON THE SELECTED BUS
  if (!gIna.begin(&PB_WIRE)) {
    PB_LOGI("[PowerBudget] ERROR: INA219 not detected at 0x%02X\n", (unsigned) PB_INA_ADDR);
  } else {
    gIna.setCalibration_32V_1A();
    PB_LOGI("[PowerBudget] INA219 OK\n");
  }

  t0_ = tPrev_  = millis();
  tNextPrint_   = t0_ + PB_PRINT_MS;
  tNextSummary_ = t0_ + PB_SUMMARY_MS;

  mWh_ = 0.0;
  mAh_ = 0.0;

  Vmin_ = 1e9f;
  Vmax_ = -1e9f;
  Imin_ = 1e9f;
  Imax_ = -1e9f;
  Pmax_ = -1e9f;

  printHeaderOnce();
  ready_ = true;
}

void PowerBudgetModule::printLive(float V, float I, float P) {
  PB_LOGI("[INST] V=%5.3f V  I=%6.1f mA  P=%6.1f mW\n", V, I, P);
}

void PowerBudgetModule::printSummary() {
  const double hours   = (millis() - t0_) / 3600000.0;
  const double Iavg_mA = (hours > 0.0) ? (mAh_ / hours) : 0.0;  // mA
  const double Pavg_mW = (hours > 0.0) ? (mWh_ / hours) : 0.0;  // mW

  const double E_sys_Wh  = (PB_BATT_CAP_MAH / 1000.0) * PB_VBAT_NOM_V * PB_REG_EFF;
  const double runtime_h = (Pavg_mW > 0.0) ? (E_sys_Wh / (Pavg_mW / 1000.0)) : 0.0;

  PB_LOGI("\n---------------- POWER SUMMARY ----------------\n");
  PB_LOGI("t=%.1f s  |  ΣE=%.3f mWh  ΣQ=%.3f mAh\n", hours * 3600.0, mWh_, mAh_);
  PB_LOGI("Iavg=%.2f mA  (≈ mAh/h)   Pavg=%.1f mW\n", Iavg_mA, Pavg_mW);
  PB_LOGI("Autonomy (power model, full battery): %.1f h\n", runtime_h);
  PB_LOGI("Vmin/Vmax=%5.3f/%5.3f V  Imin/Imax=%6.1f/%6.1f mA  Pmax=%6.1f mW\n",
          Vmin_,
          Vmax_,
          Imin_,
          Imax_,
          Pmax_);
  PB_LOGI("\n------------------------------------------------\n");
}

int32_t PowerBudgetModule::runOnce() {
  if (!ready_)
    initOnce();

  const uint32_t now = millis();
  const uint32_t dt  = now - tPrev_;
  if ((int32_t) dt < 0 || dt < PB_SAMPLE_MS)
    return 25;
  tPrev_ = now;

  // READINGS
  const float V = gIna.getBusVoltage_V();
  const float I = gIna.getCurrent_mA();
  const float P = gIna.getPower_mW();

  // MIN/MAX
  if (V < Vmin_)
    Vmin_ = V;
  if (V > Vmax_)
    Vmax_ = V;
  if (I < Imin_)
    Imin_ = I;
  if (I > Imax_)
    Imax_ = I;
  if (P > Pmax_)
    Pmax_ = P;

  // PROPER INTEGRATION (mWh = mW * s / 3600)
  const double dt_s = dt / 1000.0;
  mWh_ += (P * dt_s) / 3600.0;
  mAh_ += (I * dt_s) / 3600.0;

  if ((int32_t) (now - tNextPrint_) >= 0) {
    printLive(V, I, P);
    tNextPrint_ = now + PB_PRINT_MS;
  }

  if ((int32_t) (now - tNextSummary_) >= 0) {
    printSummary();
    tNextSummary_ = now + PB_SUMMARY_MS;
  }

  return 25;  // ms
}