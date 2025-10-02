/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Util    : Flag Store (Heltec Wireless Stick V3)            *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/

#pragma once

#if defined(BOARD_HELTEC_WIRELESS_STICK_V3)
  #include <Arduino.h>

  // ===================== CONFIGURATION =====================

  // <PARTITION/NAMESPACE> AND <KEY> FOR NVS
  #ifndef WS3_FLAG_NVS_NAMESPACE
    #define WS3_FLAG_NVS_NAMESPACE "ws3_nv"
  #endif
  #ifndef WS3_FLAG_NVS_KEY
    #define WS3_FLAG_NVS_KEY "phase_flag"
  #endif

  // DEFAULT VALUE (32-BIT) -> HEX RECOMMENDED
  #ifndef WS3_FLAG_DEFAULT
    #define WS3_FLAG_DEFAULT 0xCAFEBABEUL
  #endif

  // ======= LOG GATE (0=OFF, 1=INFO, 2=DEBUG) =======
  #ifndef WS3_FLAG_LOG_LEVEL
    #define WS3_FLAG_LOG_LEVEL 1
  #endif
  #if WS3_FLAG_LOG_LEVEL >= 1
    #define WS3_FLAG_LOGI(...) LOG_INFO(__VA_ARGS__)
  #else
    #define WS3_FLAG_LOGI(...)                                                                     \
      do {                                                                                         \
      } while (0)
  #endif
  #if WS3_FLAG_LOG_LEVEL >= 2
    #define WS3_FLAG_LOGD(...) LOG_DEBUG(__VA_ARGS__)
  #else
    #define WS3_FLAG_LOGD(...)                                                                     \
      do {                                                                                         \
      } while (0)
  #endif
// ===================== END OF CONFIGURATION =====================

/*
 * NOTE: NOT INCLUDED nvs.h HERE TO KEEP HEADER CLEAN.
 *
 * - THE HANDLE IS STORED AS AN OPAQUE POINTER.
 */
class Ws3FlagStore {
public:
  /**
   * INIT NVS
   *
   * @return true IF INIT WAS SUCCESSFUL, false OTHERWISE
   */
  static bool begin();

  /**
   * CHECK IF NVS IS READY
   *
   * @return true IF NVS IS READY, false OTHERWISE
   */
  static bool isReady();

  /**
   * READ FLAG FROM NVS
   *
   * @param out VARIABLE TO STORE THE READ VALUE
   * @return true IF THE FLAG WAS READ SUCCESSFULLY, false OTHERWISE
   */
  static bool read(uint32_t& out);

  /**
   * GET FLAG FROM NVS
   *
   * @return THE flag value IF IT EXISTS, THE default VALUE OTHERWISE (NOT WRITTEN)
   */
  static uint32_t get();

  /**
   * WRITE FLAG TO NVS
   *
   * @param value THE VALUE TO WRITE
   * @return true IF THE FLAG WAS WRITTEN SUCCESSFULLY, false OTHERWISE
   */
  static bool write(uint32_t value);

  /**
   * ERASE FLAG FROM NVS
   *
   * @return true IF THE FLAG WAS ERASED SUCCESSFULLY, false OTHERWISE
   * @note THIS DOES NOT FORMAT NVS; INTERNAL COMMIT REQUIRED
   */
  static bool erase();

  /**
   * WRITE DEFAULT FLAG TO NVS
   *
   * @return true IF THE DEFAULT FLAG WAS WRITTEN SUCCESSFULLY, false OTHERWISE
   */
  static bool writeDefault();

  /**
   * PRINT FLAG FROM NVS
   *
   * @return true IF THE FLAG WAS PRINTED SUCCESSFULLY, false OTHERWISE
   * @note IF THE flag DOES NOT EXIST, THE default VALUE WILL BE PRINTED
   */
  static void print();

  // STATIC INFO
  static constexpr const char* ns() {
    return WS3_FLAG_NVS_NAMESPACE;
  }
  static constexpr const char* key() {
    return WS3_FLAG_NVS_KEY;
  }
  static constexpr uint32_t def() {
    return (uint32_t) WS3_FLAG_DEFAULT;
  }

private:
  static void ensureNvsInit();

  // nvs_handle_t OPAQUE
  static void* handle_;

  static bool inited_;
  Ws3FlagStore() = delete;
};

#endif