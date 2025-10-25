/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Traffic RS485 Coordinator (Leader+Follower)    *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#pragma once

#include <Arduino.h>

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

/* =======================[ DEFAULT CONFIG (OVERRIDE VIA -D ...) ]=======================
 *  ROLE:
 *    -DROLE_LEADER                  -> START AS LEADER; OTHERWISE FOLLOWER
 *
 *  NODE ID:
 *    -DRS485_NODE_ID=0|1|2         -> UNIQUE ID PER NODE
 *
 *  TOPOLOGY (ASSUMED 3 NODES):
 *    -DRS485_TOPOLOGY=3
 *
 *  UART / RS485 PINS:
 *    -DRS485_PIN_RX=...
 *    -DRS485_PIN_TX=...
 *    -DRS485_PIN_DIR=...           -> DE/RE (HIGH=TX, LOW=RX)
 *    -DRS485_BAUD=9600
 *
 *  OPTIONAL STATUS LEDS (-1 DISABLES):
 *    -DRS485_LED_RED_PIN=-1
 *    -DRS485_LED_AMBER_PIN=-1
 *    -DRS485_LED_GREEN_PIN=-1
 *
 *  KNOWN NODES & PRIORITY (LOWER INDEX = HIGHER PRIORITY):
 *    -DRS485_NUM_KNOWN_NODES=3
 *    -DRS485_PRIO0=0
 *    -DRS485_PRIO1=1
 *    -DRS485_PRIO2=2
 *
 *  TIMINGS (MS):
 *    -DRS485_CASE_INTERVAL_MS=25000
 *    -DRS485_AMBER_INTERVAL_MS=5000
 *    -DRS485_ALL_RED_MS=700
 *    -DRS485_AMBER_BLINK_MS=500
 *    -DRS485_BEACON_PERIOD_MS=2000
 *    -DRS485_LOSS_TIMEOUT_MS=8000
 *    -DRS485_LEASE_MS=15000
 *    -DRS485_RENEW_BEFORE_MS=5000
 *    -DRS485_STARTUP_WAIT_LOWER_MS=4000
 *
 *  ELECTION / CLAIM:
 *    -DRS485_ELECT_BACKOFF_BASE_MS=800
 *    -DRS485_ELECT_BACKOFF_STEP_MS=600
 *    -DRS485_ELECT_JITTER_MS=400
 *    -DRS485_CLAIM_WINDOW_MS=1200
 *
 *  VEHICLE HEADS (EACH HAS R/A/G; -1 DISABLES):
 *    -DRS485_V_S2N_R_PIN / _A_PIN / _G_PIN
 *    -DRS485_V_S2W_R_PIN / _A_PIN / _G_PIN
 *    -DRS485_V_N2S_R_PIN / _A_PIN / _G_PIN
 *    -DRS485_V_N2W_R_PIN / _A_PIN / _G_PIN
 *    -DRS485_V_W2N_R_PIN / _A_PIN / _G_PIN
 *    -DRS485_V_W2S_R_PIN / _A_PIN / _G_PIN
 *
 *  PEDESTRIAN HEADS (EACH HAS R/G; -1 DISABLES):
 *    -DRS485_P_N1_R_PIN / _G_PIN
 *    -DRS485_P_S1_R_PIN / _G_PIN
 *    -DRS485_P_W2_R_PIN / _G_PIN
 *    -DRS485_P_S2_R_PIN / _G_PIN
 *    -DRS485_P_N2_R_PIN / _G_PIN
 *    -DRS485_P_W1_R_PIN / _G_PIN
 * =====================================================================================*/

#ifndef RS485_NODE_ID
  #define RS485_NODE_ID 0
#endif
#ifndef RS485_TOPOLOGY
  #define RS485_TOPOLOGY 3
#endif

/* --- RS485 UART --- */
#ifndef RS485_PIN_RX
  #define RS485_PIN_RX -1
#endif
#ifndef RS485_PIN_TX
  #define RS485_PIN_TX -1
#endif
#ifndef RS485_PIN_DIR
  #define RS485_PIN_DIR -1 /* DE/RE (HIGH=TX, LOW=RX) */
#endif
#ifndef RS485_BAUD
  #define RS485_BAUD 9600
#endif

/* --- STATUS LEDS (OPTIONAL, -1 DISABLES) --- */
#ifndef RS485_LED_RED_PIN
  #define RS485_LED_RED_PIN -1
#endif
#ifndef RS485_LED_AMBER_PIN
  #define RS485_LED_AMBER_PIN -1
#endif
#ifndef RS485_LED_GREEN_PIN
  #define RS485_LED_GREEN_PIN -1
#endif

/* --- TIMINGS (MS) --- */
#ifndef RS485_BEACON_PERIOD_MS
  #define RS485_BEACON_PERIOD_MS 2000U
#endif
#ifndef RS485_LOSS_TIMEOUT_MS
  #define RS485_LOSS_TIMEOUT_MS 8000U
#endif
#ifndef RS485_LEASE_MS
  #define RS485_LEASE_MS 15000U
#endif
#ifndef RS485_RENEW_BEFORE_MS
  #define RS485_RENEW_BEFORE_MS 5000U
#endif
#ifndef RS485_CASE_INTERVAL_MS
  #define RS485_CASE_INTERVAL_MS 25000U
#endif
#ifndef RS485_AMBER_INTERVAL_MS
  #define RS485_AMBER_INTERVAL_MS 5000U
#endif
#ifndef RS485_ALL_RED_MS
  #define RS485_ALL_RED_MS 700U
#endif
#ifndef RS485_AMBER_BLINK_MS
  #define RS485_AMBER_BLINK_MS 500U
#endif
#ifndef RS485_STARTUP_WAIT_LOWER_MS
  #define RS485_STARTUP_WAIT_LOWER_MS 4000U
#endif

/* --- ELECTION/BACKOFF --- */
#ifndef RS485_ELECT_BACKOFF_BASE_MS
  #define RS485_ELECT_BACKOFF_BASE_MS 800U
#endif
#ifndef RS485_ELECT_BACKOFF_STEP_MS
  #define RS485_ELECT_BACKOFF_STEP_MS 600U
#endif
#ifndef RS485_ELECT_JITTER_MS
  #define RS485_ELECT_JITTER_MS 400U
#endif
#ifndef RS485_CLAIM_WINDOW_MS
  #define RS485_CLAIM_WINDOW_MS 1200U
#endif

/* --- PRIORITY TABLE LENGTH & ENTRIES --- */
#ifndef RS485_NUM_KNOWN_NODES
  #define RS485_NUM_KNOWN_NODES 3
#endif
#ifndef RS485_PRIO0
  #define RS485_PRIO0 0
#endif
#ifndef RS485_PRIO1
  #define RS485_PRIO1 1
#endif
#ifndef RS485_PRIO2
  #define RS485_PRIO2 2
#endif

/* ==========================[ VEHICLE HEAD MOVEMENTS ]========================== */
/* S2N: SOUTH->NORTH,
 * S2W: SOUTH->WEST,
 * N2S: NORTH->SOUTH,
 * N2W: NORTH->WEST,
 * W2N: WEST->NORTH,
 * W2S: WEST->SOUTH */
enum : uint8_t { VM_S2N = 0, VM_S2W, VM_N2S, VM_N2W, VM_W2N, VM_W2S, VM_COUNT };

/* EACH MOVEMENT HAS R/A/G PINS. DEFAULTS TO -1 (DISABLED) VIA -D IN .INI */
#ifndef RS485_V_S2N_R_PIN
  #define RS485_V_S2N_R_PIN -1
#endif
#ifndef RS485_V_S2N_A_PIN
  #define RS485_V_S2N_A_PIN -1
#endif
#ifndef RS485_V_S2N_G_PIN
  #define RS485_V_S2N_G_PIN -1
#endif
#ifndef RS485_V_S2W_R_PIN
  #define RS485_V_S2W_R_PIN -1
#endif
#ifndef RS485_V_S2W_A_PIN
  #define RS485_V_S2W_A_PIN -1
#endif
#ifndef RS485_V_S2W_G_PIN
  #define RS485_V_S2W_G_PIN -1
#endif
#ifndef RS485_V_N2S_R_PIN
  #define RS485_V_N2S_R_PIN -1
#endif
#ifndef RS485_V_N2S_A_PIN
  #define RS485_V_N2S_A_PIN -1
#endif
#ifndef RS485_V_N2S_G_PIN
  #define RS485_V_N2S_G_PIN -1
#endif
#ifndef RS485_V_N2W_R_PIN
  #define RS485_V_N2W_R_PIN -1
#endif
#ifndef RS485_V_N2W_A_PIN
  #define RS485_V_N2W_A_PIN -1
#endif
#ifndef RS485_V_N2W_G_PIN
  #define RS485_V_N2W_G_PIN -1
#endif
#ifndef RS485_V_W2N_R_PIN
  #define RS485_V_W2N_R_PIN -1
#endif
#ifndef RS485_V_W2N_A_PIN
  #define RS485_V_W2N_A_PIN -1
#endif
#ifndef RS485_V_W2N_G_PIN
  #define RS485_V_W2N_G_PIN -1
#endif
#ifndef RS485_V_W2S_R_PIN
  #define RS485_V_W2S_R_PIN -1
#endif
#ifndef RS485_V_W2S_A_PIN
  #define RS485_V_W2S_A_PIN -1
#endif
#ifndef RS485_V_W2S_G_PIN
  #define RS485_V_W2S_G_PIN -1
#endif

/* ==========================[ PEDESTRIAN CROSSINGS ]============================ */
/* PX_N1,
 * PX_S1,
 * PX_W2,
 * PX_S2,
 * PX_N2,
 * PX_W1 */
enum : uint8_t { PX_N1 = 0, PX_S1, PX_W2, PX_S2, PX_N2, PX_W1, PX_COUNT };

/* EACH PEDESTRIAN HAS R/G PINS (ACTIVE HIGH ON GREEN, RED = !GREEN) */
#ifndef RS485_P_N1_R_PIN
  #define RS485_P_N1_R_PIN -1
#endif
#ifndef RS485_P_N1_G_PIN
  #define RS485_P_N1_G_PIN -1
#endif
#ifndef RS485_P_S1_R_PIN
  #define RS485_P_S1_R_PIN -1
#endif
#ifndef RS485_P_S1_G_PIN
  #define RS485_P_S1_G_PIN -1
#endif
#ifndef RS485_P_W2_R_PIN
  #define RS485_P_W2_R_PIN -1
#endif
#ifndef RS485_P_W2_G_PIN
  #define RS485_P_W2_G_PIN -1
#endif
#ifndef RS485_P_S2_R_PIN
  #define RS485_P_S2_R_PIN -1
#endif
#ifndef RS485_P_S2_G_PIN
  #define RS485_P_S2_G_PIN -1
#endif
#ifndef RS485_P_N2_R_PIN
  #define RS485_P_N2_R_PIN -1
#endif
#ifndef RS485_P_N2_G_PIN
  #define RS485_P_N2_G_PIN -1
#endif
#ifndef RS485_P_W1_R_PIN
  #define RS485_P_W1_R_PIN -1
#endif
#ifndef RS485_P_W1_G_PIN
  #define RS485_P_W1_G_PIN -1
#endif

/* ===========================[ LIGHT STATES (VEHICLE) ]========================= */
enum LightState : uint8_t { L_RED = 0, L_GREEN, L_AMBER_FIXED, L_AMBER_FLASH };

/* ===============================[ MAIN CLASS ]================================ */
class TrafficRs485CoordinatorModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  TrafficRs485CoordinatorModule();

  /* OSTHREAD */
  int32_t runOnce() override;

  /* NOT USING MESH PAYLOADS FOR NOW */
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  /* ---------- INIT ---------- */
  void        initOnce();
  void        beginUart();
  inline void setTx(bool en);

  /* ---------- RS485 TX/RX ---------- */
  static uint8_t          computeXOR(const uint8_t* data, size_t len);
  void                    sendFrame(const char* buf, size_t len);
  void                    pumpRx();
  static constexpr size_t RX_MAX = 192;
  char                    rxBuf_[RX_MAX + 1]{};
  size_t                  rxLen_ = 0;

  /* ---------- CSV PARSERS ---------- */
  bool parseCSV_u8(const char*& p, uint8_t& out);
  bool parseCSV_u16(const char*& p, uint16_t& out);
  bool parseCSV_u32(const char*& p, uint32_t& out);

  /* ---------- PROTOCOL ---------- */
  void handleLine(const char* lineZ);
  void sendBeacon_();
  void tx_Beacon_(uint8_t  leaderId,
                  uint32_t seq,
                  uint8_t  c,
                  uint8_t  am,
                  uint8_t  off,
                  uint32_t leaseTtlMs,
                  uint32_t elapsedMs);
  void tx_Claim_(uint8_t id, uint8_t rank);
  void tx_Yield_(uint8_t fromId, uint8_t toId);

  /* ---------- ELECTION/YIELD ---------- */
  uint8_t idxInPrioList_(uint8_t id) const;
  void    scheduleElectionBackoff_();
  void    startClaiming_();
  void    stopClaiming_(bool won);
  void    becomeLeaderFromHere_();
  void    yieldTo_(uint8_t newLeader);

  /* ---------- STARTUP LOWER-ID WATCH ---------- */
  inline void observeRemoteId_(uint8_t remoteId);

  /* ---------- CASE/TOPOLOGY ---------- */
  inline uint8_t greenNode_(uint8_t c);
  inline uint8_t nextCaseForTopology_(uint8_t curr);

  /* ---------- LEADER/FOLLOWER TICKS ---------- */
  void leaderTick_();
  void followerTick_();

  /* ---------- STATUS LEDS (OPTIONAL) ---------- */
  inline bool ledsPresent();
  inline void leds(bool r, bool a, bool g);
  void        applyCaseLocal(uint8_t c);
  void        applyAmberLocal(uint8_t offNode);
  void        applySafetyBlink();

  /* ---------- VEHICLE/PEDESTRIAN OUTPUTS ---------- */
  void        setupSignals_();
  inline void setVehPins_(uint8_t idx, bool r, bool a, bool g);
  inline void setPedPins_(uint8_t idx, bool g);
  void        driveOutputs_();
  void        applyIntersectionCase_(uint8_t c);
  void        applyAllRed_();
  void        applySafetyOutputs_();
  void        applyAmberTransitionForIntersection_();

  /* ---------- STATE ---------- */
  bool ready_ = false;

#ifdef ROLE_LEADER
  bool isLeader_ = true;
#else
  bool isLeader_ = false;
#endif
  uint8_t myId_     = (uint8_t) RS485_NODE_ID;
  uint8_t leaderId_ = 0xFF;

  /* UART TIMING (US) */
  uint32_t t_bit_us_  = 0;
  uint32_t t_char_us_ = 0;

  /* VEHICLE/PEDESTRIAN PINMAPS + STATES */
  int16_t    vR_[VM_COUNT]{}, vA_[VM_COUNT]{}, vG_[VM_COUNT]{};
  int16_t    pR_[PX_COUNT]{}, pG_[PX_COUNT]{};
  LightState vState_[VM_COUNT]{};
  bool       pGreen_[PX_COUNT]{};

  /* INTERSECTION CASE MANAGEMENT */
  uint8_t  caseIndex_    = 2;
  uint8_t  nextCase_     = 3;
  uint8_t  offNode_      = 0;
  bool     inAmber_      = false;
  bool     inAllRed_     = false;
  uint32_t tCaseStart_   = 0;
  uint32_t tAmberStart_  = 0;
  uint32_t tAllRedStart_ = 0;

  /* LEASE/BEACON TIMERS */
  uint32_t seq_               = 0;
  uint32_t leaseExpiryMs_     = 0;
  uint32_t lastBeaconRxMs_    = 0;
  uint32_t seenLeaseExpiryMs_ = 0;
  uint32_t nextBeaconAt_      = 0;

  /* ELECTION STATE */
  bool     inSafety_            = false;
  bool     claiming_            = false;
  uint32_t electBackoffUntilMs_ = 0;
  uint32_t claimUntilMs_        = 0;
  uint8_t  observedLeaderRank_  = 0xFF;

  /* STARTUP LOWER-ID WATCH */
  bool     seenLowerId_            = false;
  uint32_t startupLowerDeadlineMs_ = 0;

  /* PRIORITY ORDER (RANK 0 = HIGHEST) */
  static const uint8_t kPrio_[RS485_NUM_KNOWN_NODES];
};