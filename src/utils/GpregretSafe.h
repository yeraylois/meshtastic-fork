/**********************************************************
 *   Project   : Blackout Traffic Light System             *
 *   Util      : GpregretSafe                              *
 *   Author    : Yeray Lois Sanchez                        *
 *   Email     : yerayloissanchez@gmail.com                *
 ***********************************************************/
#pragma once
#include <Arduino.h>

#include <platform/nrf52/softdevice/nrf_sdm.h>  // FOR FUNCTION: sd_softdevice_is_enabled()
#include <platform/nrf52/softdevice/nrf_soc.h>  // FOR FUNCTION: sd_power_gpregret_{get,set,clr}()

// MMIO (FALLBACK WHEN 'SD' IS DISABLED)
#ifndef NRF_POWER_BASE
  #define NRF_POWER_BASE (0x40000000UL)
#endif
#define GPREGRET_ADDR                                                                              \
  (NRF_POWER_BASE + 0x0000051CUL)  // DONT USE THIS BUT COULD BE USEFUL IN FUTURE IMPROVEMENTS

#define GPREGRET2_ADDR (NRF_POWER_BASE + 0x00000520UL)  // THIS IS USED

/**
 * CHECK IF SOFTDEVICE IS ENABLED
 *
 * @return true IF SOFTDEVICE IS ENABLED, false OTHERWISE
 */
static inline bool sd_enabled() {
  uint8_t en = 0;
  (void) sd_softdevice_is_enabled(&en);
  return en != 0;
}

/**
 * READ GPREGRET2 (id = 1)
 *
 * @return value of GPREGRET2
 */
static inline uint8_t gp2_read() {
  if (sd_enabled()) {
    uint32_t v = 0;
    (void) sd_power_gpregret_get(1, &v);  // GPREGRET2 IS '1'
    return (uint8_t) (v & 0xFF);
  } else {
    return *(volatile uint8_t*) GPREGRET2_ADDR;
  }
}

/**
 * WRITE GPREGRET2 (id = 1)
 *
 * @param newv New value to write to GPREGRET2
 */
static inline void gp2_write(uint8_t newv) {
  if (sd_enabled()) {
    uint32_t cur = 0;
    (void) sd_power_gpregret_get(1, &cur);
    uint32_t to_set = (~cur) & newv;
    uint32_t to_clr = cur & (~newv);
    if (to_clr)
      (void) sd_power_gpregret_clr(1, to_clr);
    if (to_set)
      (void) sd_power_gpregret_set(1, to_set);
  } else {
    *(volatile uint8_t*) GPREGRET2_ADDR = newv;
  }
}