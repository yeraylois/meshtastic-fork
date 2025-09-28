/***************************************************************************************************************************************
 *   Project   : Blackout Traffic Light System                                     * Library   :
 * Power Monitor                       * Author    : Yeray Lois Sanchez
 *                                                                        * Email     :
 * yerayloissanchez@gmail.com                       * Available :
 * https://github.com/yeraylois/blackout-traffic-light-system/tree/main/firmware/common/libraries/PowerMonitor
 *            *
 ****************************************************************************************************************************************/

#ifndef POWERMONITOR_H
#define POWERMONITOR_H

#include <Arduino.h>

/**
 * INITIALIZE THE MONITOR: SETUP INPUT AND LED PINS
 *
 * @param inputPin      DIGITAL PIN CONNECTED TO OPTOCOUPLER OUTPUT
 * @param ledPin        DIGITAL PIN CONNECTED TO INDICATOR LED
 * @param inputPullup   ENABLE INTERNAL PULL-UP RESISTOR (DEFAULT TRUE)
 */
void PM_init(uint8_t inputPin, uint8_t ledPin, bool inputPullup = true);

/**
 * READ RAW DIGITAL VALUE FROM OPTOCOUPLER PIN
 *
 * @return TRUE IF PIN READS HIGH (BEFORE INVERSION)
 */
bool PM_readRaw();

/**
 * RETURN TRUE IF POWER IS OK (RAW == HIGH, AFTER INVERSION AND DEBOUNCE)
 *
 * @return CURRENT POWER STATUS
 */
bool PM_isPowerOk();

/**
 * UPDATE LED INDICATOR ACCORDING TO POWER STATUS
 *
 * LED IS OFF WHEN POWER IS OK, ON WHEN POWER DOWN.
 */
void PM_updateLED();

/**
 * ATTACH A CALLBACK TO BE CALLED ON STATUS CHANGE
 *
 * @param callback  FUNCTION TO CALL WITH NEW STATUS (TRUE=OK, FALSE=DOWN)
 */
void PM_onChange(void (*callback)(bool newStatus));

/**
 * INVERT LOGIC (IF OPTO IS ACTIVE-LOW)
 *
 * @param invert  SET TO TRUE TO INVERT INPUT LOGIC
 */
void PM_invertLogic(bool invert);

/**
 * DEBOUNCE READINGS: SET DEBOUNCE TIME IN MILLISECONDS
 *
 * @param ms  MINIMUM TIME BETWEEN STATUS CHANGES TO AVOID BOUNCE
 */
void PM_setDebounce(uint16_t ms);

#endif
