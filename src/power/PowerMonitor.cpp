/***************************************************************************************************************************************
 *   Project   : Blackout Traffic Light System                                     * Library   :
 * Power Monitor                       * Author    : Yeray Lois Sanchez
 *                                                                        * Email     :
 * yerayloissanchez@gmail.com                       * Available :
 * https://github.com/yeraylois/blackout-traffic-light-system/tree/main/firmware/common/libraries/PowerMonitor
 *            *
 ****************************************************************************************************************************************/

#include "PowerMonitor.h"

static uint8_t  _pinInput;
static uint8_t  _pinLed;
static bool     _invert          = false;
static uint16_t _debounce        = 0;
static bool     _lastStatus      = false;
static uint32_t _lastChangeMs    = 0;
static void (*_onChangeCb)(bool) = nullptr;

/**
 * INITIALIZE THE MONITOR: SETUP INPUT AND LED PINS
 */
void PM_init(uint8_t inputPin, uint8_t ledPin, bool inputPullup) {
  _pinInput = inputPin;
  _pinLed   = ledPin;
  pinMode(_pinInput, inputPullup ? INPUT_PULLUP : INPUT);
  pinMode(_pinLed, OUTPUT);
  digitalWrite(_pinLed, LOW);
}

/**
 * READ RAW DIGITAL VALUE FROM OPTOCOUPLER PIN
 */
bool PM_readRaw() {
  bool val = digitalRead(_pinInput) == HIGH;
  return _invert ? !val : val;
}

/**
 * RETURN TRUE IF POWER IS OK (RAW == HIGH, AFTER INVERSION AND DEBOUNCE)
 */
bool PM_isPowerOk() {
  bool     current = PM_readRaw();
  uint32_t now     = millis();

  if (_debounce > 0) {
    if (current != _lastStatus && (now - _lastChangeMs) < _debounce) {
      // STILL WITHIN DEBOUNCE WINDOW, IGNORE CHANGE
      return _lastStatus;
    }
    if (current != _lastStatus) {
      _lastStatus   = current;
      _lastChangeMs = now;
      if (_onChangeCb)
        _onChangeCb(current);
    }
  } else {
    if (current != _lastStatus) {
      _lastStatus = current;
      if (_onChangeCb)
        _onChangeCb(current);
    }
  }
  return _lastStatus;
}

/**
 * UPDATE LED INDICATOR ACCORDING TO POWER STATUS
 */
void PM_updateLED() {
  digitalWrite(_pinLed, PM_isPowerOk() ? LOW : HIGH);
}

/**
 * ATTACH A CALLBACK TO BE CALLED ON STATUS CHANGE
 */
void PM_onChange(void (*callback)(bool)) {
  _onChangeCb = callback;
}

/**
 * INVERT LOGIC (IF OPTO IS ACTIVE-LOW)
 */
void PM_invertLogic(bool invert) {
  _invert = invert;
}

/**
 * DEBOUNCE READINGS: SET DEBOUNCE TIME IN MILLISECONDS
 */
void PM_setDebounce(uint16_t ms) {
  _debounce = ms;
}
