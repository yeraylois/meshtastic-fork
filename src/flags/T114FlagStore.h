/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Util    : Flag Store (Heltec Mesh Node T114)               *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/
#pragma once
#if defined(BOARD_HELTEC_MESH_NODE_T114_V2_0)

  #include <Arduino.h>
  #include <inttypes.h>
  #define LOG_TAG "t114_flag_store"
  #include "configuration.h"

  /**
   * UTILITY FUNCTIONS FOR SoftDevice-safe ACCESS TO (GPREGRET2)
   *
   * gp2_read: Reads the GPREGRET2 register
   * gp2_write: Writes to the GPREGRET2 register
   */
  #include "utils/GpregretSafe.h"

  // ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
  #ifndef T114_FLAG_LOG_LEVEL
    #define T114_FLAG_LOG_LEVEL 1
  #endif
  #if T114_FLAG_LOG_LEVEL >= 1
    #define T114_FLAG_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define T114_FLAG_LOGI(...)                                                                    \
      do {                                                                                         \
      } while (0)
  #endif
  #if T114_FLAG_LOG_LEVEL >= 2
    #define T114_FLAG_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define T114_FLAG_LOGD(...)                                                                    \
      do {                                                                                         \
      } while (0)
  #endif

  // ======= FLAG DEFINITIONS =======
  #ifndef WS3_FLAG_DEFAULT
    #define WS3_FLAG_DEFAULT 0xCAFEBABEUL
  #endif
  #ifndef WS3_FLAG_OPTO_POWER_OK
    #define WS3_FLAG_OPTO_POWER_OK 0xAABBCC01UL
  #endif
  #ifndef WS3_FLAG_OPTO_POWER_DOWN
    #define WS3_FLAG_OPTO_POWER_DOWN 0xAABBCC00UL
  #endif

  // ======= ENCODING IN GPREGRET2 (ONE BYTE) =======
  /**
   * 0xFF -> DEFAULT
   * 0xA1 -> POWER_OK
   * 0xA0 -> POWER_DOWN
   *
   * (EVADE 0x57 FOR BOOTLOADER COMPATIBILITY (ALTHOUGH IS GPREGRET, NOT GPREGRET2))
   * (evitamos 0x57 por tradición bootloader, aunque es GPREGRET, no GPREGRET2)
   */
  #define T114_FLAG_CODE_DEFAULT 0xFFu
  #define T114_FLAG_CODE_POWER_OK 0xA1u
  #define T114_FLAG_CODE_POWER_DOWN 0xA0u

class T114FlagStore {
public:
  /**
   * INITIALIZE THE FLAG STORE
   *
   */
  static void begin() {
    /* NO-OP  (GPREGRET2 ALWAYS AVAILABLE) */
  }

  /**
   * GET THE CURRENT FLAG VALUE
   *
   * @return THE CURRENT FLAG VALUE
   */
  static uint32_t get() {
    uint8_t c = gp2_read();
    switch (c) {
      case T114_FLAG_CODE_POWER_OK:
        return WS3_FLAG_OPTO_POWER_OK;
      case T114_FLAG_CODE_POWER_DOWN:
        return WS3_FLAG_OPTO_POWER_DOWN;
      case T114_FLAG_CODE_DEFAULT:
      default:
        return WS3_FLAG_DEFAULT;
    }
  }

  /**
   * WRITE A NEW FLAG VALUE
   *
   * @param v THE NEW FLAG VALUE
   * @return true IF SUCCESSFUL, false OTHERWISE
   */
  static bool write(uint32_t v) {
    uint8_t code;
    if (v == WS3_FLAG_OPTO_POWER_OK)
      code = T114_FLAG_CODE_POWER_OK;
    else if (v == WS3_FLAG_OPTO_POWER_DOWN)
      code = T114_FLAG_CODE_POWER_DOWN;
    else
      code = T114_FLAG_CODE_DEFAULT;
    gp2_write(code);
    return true;
  }

  /**
   * WRITE THE DEFAULT FLAG VALUE
   *
   * @return true IF SUCCESSFUL, false OTHERWISE
   */
  static bool writeDefault() {
    gp2_write(T114_FLAG_CODE_DEFAULT);
    return true;
  }

  /**
   * PRINT THE CURRENT FLAG STATE
   *
   */
  static void print() {
    uint8_t  c      = gp2_read();
    uint32_t mapped = get();
    T114_FLAG_LOGI("[FlagStore] code=0x%02X mapped=0x%08" PRIX32 "\n", (unsigned) c, mapped);
  }
};

#endif  // BOARD_HELTEC_MESH_NODE_T114_V2_0