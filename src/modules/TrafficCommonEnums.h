#pragma once
#include <stdint.h>

/* =========================[ VEHICLE HEAD MOVEMENTS ]==========================
 *  S2N: SOUTH->NORTH,  S2W: SOUTH->WEST,  N2S: NORTH->SOUTH,
 *  N2W: NORTH->WEST,   W2N: WEST->NORTH,  W2S: WEST->SOUTH
 *  (SHARED BY RS485 AND MESH)
 * ============================================================================*/
enum : uint8_t { VM_S2N = 0, VM_S2W, VM_N2S, VM_N2W, VM_W2N, VM_W2S, VM_COUNT };

/* ===========================[ PEDESTRIAN CROSSINGS ]==========================
 *  PX_N1, PX_S1, PX_W2, PX_S2, PX_N2, PX_W1
 *  (SHARED BY RS485 AND MESH)
 * ============================================================================*/
enum : uint8_t { PX_N1 = 0, PX_S1, PX_W2, PX_S2, PX_N2, PX_W1, PX_COUNT };

/* ===========================[ LIGHT STATES (VEHICLE) ]======================= */
enum LightState : uint8_t { L_RED = 0, L_GREEN, L_AMBER_FIXED, L_AMBER_FLASH };