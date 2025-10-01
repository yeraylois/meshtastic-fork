/****************************************************************
 *   Project : Blackout Traffic Light System                    *
 *   Module  : Traffic Light Mesh Coordinator (Leader+Follower) *
 *   Author  : Yeray Lois Sanchez                               *
 *   Email   : yerayloissanchez@gmail.com                       *
 ****************************************************************/
#pragma once

#include <Arduino.h>

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

/* =======================[ DEFAULT CONFIG (OVERRIDE VIA -D ...) ]=======================
 *  ROLE:
 *    -DROLE_LEADER                      -> START AS LEADER; OTHERWISE FOLLOWER
 *
 *  NODE ID:
 *    -DSEM_MESH_NODE_ID=0|1|2          -> UNIQUE ID PER NODE
 *
 *  TOPOLOGY (ASSUMED 3 NODES):
 *    -DSEM_MESH_TOPOLOGY=3
 *
 *  OPTIONAL STATUS LEDS (-1 DISABLES):
 *    -DSEM_MESH_LED_RED_PIN=-1
 *    -DSEM_MESH_LED_AMBER_PIN=-1
 *    -DSEM_MESH_LED_GREEN_PIN=-1
 *
 *  KNOWN NODES & PRIORITY (LOWER INDEX = HIGHER PRIORITY):
 *    -DSEM_MESH_NUM_KNOWN_NODES=3
 *    -DSEM_MESH_PRIO0=0
 *    -DSEM_MESH_PRIO1=1
 *    -DSEM_MESH_PRIO2=2
 *
 *  TIMINGS (MS):
 *    -DSEM_MESH_CASE_INTERVAL_MS=25000
 *    -DSEM_MESH_AMBER_INTERVAL_MS=5000
 *    -DSEM_MESH_ALL_RED_MS=700
 *    -DSEM_MESH_AMBER_BLINK_MS=500
 *    -DSEM_MESH_BEACON_PERIOD_MS=2000
 *    -DSEM_MESH_LOSS_TIMEOUT_MS=8000
 *    -DSEM_MESH_LEASE_MS=15000
 *    -DSEM_MESH_RENEW_BEFORE_MS=5000
 *    -DSEM_MESH_STARTUP_WAIT_LOWER_MS=4000
 *
 *  ELECTION / CLAIM:
 *    -DSEM_MESH_ELECT_BACKOFF_BASE_MS=800
 *    -DSEM_MESH_ELECT_BACKOFF_STEP_MS=600
 *    -DSEM_MESH_ELECT_JITTER_MS=400
 *    -DSEM_MESH_CLAIM_WINDOW_MS=1200
 *
 *  VEHICLE HEADS (EACH HAS R/A/G; -1 DISABLES):
 *    -DSEM_MESH_V_S2N_R_PIN / _A_PIN / _G_PIN
 *    -DSEM_MESH_V_S2W_R_PIN / _A_PIN / _G_PIN
 *    -DSEM_MESH_V_N2S_R_PIN / _A_PIN / _G_PIN
 *    -DSEM_MESH_V_N2W_R_PIN / _A_PIN / _G_PIN
 *    -DSEM_MESH_V_W2N_R_PIN / _A_PIN / _G_PIN
 *    -DSEM_MESH_V_W2S_R_PIN / _A_PIN / _G_PIN
 *
 *  PEDESTRIAN HEADS (EACH HAS R/G; -1 DISABLES):
 *    -DSEM_MESH_P_N1_R_PIN / _G_PIN
 *    -DSEM_MESH_P_S1_R_PIN / _G_PIN
 *    -DSEM_MESH_P_W2_R_PIN / _G_PIN
 *    -DSEM_MESH_P_S2_R_PIN / _G_PIN
 *    -DSEM_MESH_P_N2_R_PIN / _G_PIN
 *    -DSEM_MESH_P_W1_R_PIN / _G_PIN
 * =====================================================================================*/

/* --- NODE/TOPOLOGY --- */
#ifndef SEM_MESH_NODE_ID
  #define SEM_MESH_NODE_ID 0
#endif
#ifndef SEM_MESH_TOPOLOGY
  #define SEM_MESH_TOPOLOGY 3
#endif

/* --- STATUS LEDS (OPTIONAL, -1 DISABLES) --- */
#ifndef SEM_MESH_LED_RED_PIN
  #define SEM_MESH_LED_RED_PIN -1
#endif
#ifndef SEM_MESH_LED_AMBER_PIN
  #define SEM_MESH_LED_AMBER_PIN -1
#endif
#ifndef SEM_MESH_LED_GREEN_PIN
  #define SEM_MESH_LED_GREEN_PIN -1
#endif

/* --- TIMINGS (MS) --- */
#ifndef SEM_MESH_BEACON_PERIOD_MS
  #define SEM_MESH_BEACON_PERIOD_MS 2000U
#endif
#ifndef SEM_MESH_LOSS_TIMEOUT_MS
  #define SEM_MESH_LOSS_TIMEOUT_MS 8000U
#endif
#ifndef SEM_MESH_LEASE_MS
  #define SEM_MESH_LEASE_MS 15000U
#endif
#ifndef SEM_MESH_RENEW_BEFORE_MS
  #define SEM_MESH_RENEW_BEFORE_MS 5000U
#endif
#ifndef SEM_MESH_CASE_INTERVAL_MS
  #define SEM_MESH_CASE_INTERVAL_MS 25000U
#endif
#ifndef SEM_MESH_AMBER_INTERVAL_MS
  #define SEM_MESH_AMBER_INTERVAL_MS 5000U
#endif
#ifndef SEM_MESH_ALL_RED_MS
  #define SEM_MESH_ALL_RED_MS 700U
#endif
#ifndef SEM_MESH_AMBER_BLINK_MS
  #define SEM_MESH_AMBER_BLINK_MS 500U
#endif
#ifndef SEM_MESH_STARTUP_WAIT_LOWER_MS
  #define SEM_MESH_STARTUP_WAIT_LOWER_MS 4000U
#endif

/* --- ELECTION/BACKOFF --- */
#ifndef SEM_MESH_ELECT_BACKOFF_BASE_MS
  #define SEM_MESH_ELECT_BACKOFF_BASE_MS 800U
#endif
#ifndef SEM_MESH_ELECT_BACKOFF_STEP_MS
  #define SEM_MESH_ELECT_BACKOFF_STEP_MS 600U
#endif
#ifndef SEM_MESH_ELECT_JITTER_MS
  #define SEM_MESH_ELECT_JITTER_MS 400U
#endif
#ifndef SEM_MESH_CLAIM_WINDOW_MS
  #define SEM_MESH_CLAIM_WINDOW_MS 1200U
#endif

/* --- PRIORITY TABLE LENGTH & ENTRIES --- */
#ifndef SEM_MESH_NUM_KNOWN_NODES
  #define SEM_MESH_NUM_KNOWN_NODES 3
#endif
#ifndef SEM_MESH_PRIO0
  #define SEM_MESH_PRIO0 0
#endif
#ifndef SEM_MESH_PRIO1
  #define SEM_MESH_PRIO1 1
#endif
#ifndef SEM_MESH_PRIO2
  #define SEM_MESH_PRIO2 2
#endif

/* ==========================[ VEHICLE HEAD MOVEMENTS ]========================== */
enum : uint8_t { VM_S2N = 0, VM_S2W, VM_N2S, VM_N2W, VM_W2N, VM_W2S, VM_COUNT };

/* EACH MOVEMENT HAS R/A/G PINS. DEFAULTS TO -1 (DISABLED) */
#ifndef SEM_MESH_V_S2N_R_PIN
  #define SEM_MESH_V_S2N_R_PIN -1
#endif
#ifndef SEM_MESH_V_S2N_A_PIN
  #define SEM_MESH_V_S2N_A_PIN -1
#endif
#ifndef SEM_MESH_V_S2N_G_PIN
  #define SEM_MESH_V_S2N_G_PIN -1
#endif
#ifndef SEM_MESH_V_S2W_R_PIN
  #define SEM_MESH_V_S2W_R_PIN -1
#endif
#ifndef SEM_MESH_V_S2W_A_PIN
  #define SEM_MESH_V_S2W_A_PIN -1
#endif
#ifndef SEM_MESH_V_S2W_G_PIN
  #define SEM_MESH_V_S2W_G_PIN -1
#endif
#ifndef SEM_MESH_V_N2S_R_PIN
  #define SEM_MESH_V_N2S_R_PIN -1
#endif
#ifndef SEM_MESH_V_N2S_A_PIN
  #define SEM_MESH_V_N2S_A_PIN -1
#endif
#ifndef SEM_MESH_V_N2S_G_PIN
  #define SEM_MESH_V_N2S_G_PIN -1
#endif
#ifndef SEM_MESH_V_N2W_R_PIN
  #define SEM_MESH_V_N2W_R_PIN -1
#endif
#ifndef SEM_MESH_V_N2W_A_PIN
  #define SEM_MESH_V_N2W_A_PIN -1
#endif
#ifndef SEM_MESH_V_N2W_G_PIN
  #define SEM_MESH_V_N2W_G_PIN -1
#endif
#ifndef SEM_MESH_V_W2N_R_PIN
  #define SEM_MESH_V_W2N_R_PIN -1
#endif
#ifndef SEM_MESH_V_W2N_A_PIN
  #define SEM_MESH_V_W2N_A_PIN -1
#endif
#ifndef SEM_MESH_V_W2N_G_PIN
  #define SEM_MESH_V_W2N_G_PIN -1
#endif
#ifndef SEM_MESH_V_W2S_R_PIN
  #define SEM_MESH_V_W2S_R_PIN -1
#endif
#ifndef SEM_MESH_V_W2S_A_PIN
  #define SEM_MESH_V_W2S_A_PIN -1
#endif
#ifndef SEM_MESH_V_W2S_G_PIN
  #define SEM_MESH_V_W2S_G_PIN -1
#endif

/* ==========================[ PEDESTRIAN CROSSINGS ]============================ */
enum : uint8_t { PX_N1 = 0, PX_S1, PX_W2, PX_S2, PX_N2, PX_W1, PX_COUNT };

#ifndef SEM_MESH_P_N1_R_PIN
  #define SEM_MESH_P_N1_R_PIN -1
#endif
#ifndef SEM_MESH_P_N1_G_PIN
  #define SEM_MESH_P_N1_G_PIN -1
#endif
#ifndef SEM_MESH_P_S1_R_PIN
  #define SEM_MESH_P_S1_R_PIN -1
#endif
#ifndef SEM_MESH_P_S1_G_PIN
  #define SEM_MESH_P_S1_G_PIN -1
#endif
#ifndef SEM_MESH_P_W2_R_PIN
  #define SEM_MESH_P_W2_R_PIN -1
#endif
#ifndef SEM_MESH_P_W2_G_PIN
  #define SEM_MESH_P_W2_G_PIN -1
#endif
#ifndef SEM_MESH_P_S2_R_PIN
  #define SEM_MESH_P_S2_R_PIN -1
#endif
#ifndef SEM_MESH_P_S2_G_PIN
  #define SEM_MESH_P_S2_G_PIN -1
#endif
#ifndef SEM_MESH_P_N2_R_PIN
  #define SEM_MESH_P_N2_R_PIN -1
#endif
#ifndef SEM_MESH_P_N2_G_PIN
  #define SEM_MESH_P_N2_G_PIN -1
#endif
#ifndef SEM_MESH_P_W1_R_PIN
  #define SEM_MESH_P_W1_R_PIN -1
#endif
#ifndef SEM_MESH_P_W1_G_PIN
  #define SEM_MESH_P_W1_G_PIN -1
#endif

/* ===========================[ LIGHT STATES (VEHICLE) ]========================= */
enum LightState : uint8_t { L_RED = 0, L_GREEN, L_AMBER_FIXED, L_AMBER_FLASH };

/* ===============================[ MAIN CLASS ]================================ */
class TrafficLightMeshModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  TrafficLightMeshModule();

  /* OSTHREAD */
  int32_t runOnce() override;

  /* RX ON MESH (CSV-in-payload) */
  ProcessMessage     handleReceived(const meshtastic_MeshPacket&) override;
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  /* ---------- INIT ---------- */
  void initOnce();

  /* ---------- MESH TX ---------- */
  static uint8_t computeXOR(const uint8_t* data, size_t len);
  void           sendPayload_(const char* buf, size_t len);
  void           sendBeacon_();
  void           tx_Beacon_(uint8_t  leaderId,
                            uint32_t seq,
                            uint8_t  c,
                            uint8_t  am,
                            uint8_t  off,
                            uint32_t leaseTtlMs,
                            uint32_t elapsedMs);
  void           tx_Claim_(uint8_t id, uint8_t rank);
  void           tx_Yield_(uint8_t fromId, uint8_t toId);

  /* ---------- CSV PARSERS / RX ---------- */
  bool parseCSV_u8(const char*& p, uint8_t& out);
  bool parseCSV_u16(const char*& p, uint16_t& out);
  bool parseCSV_u32(const char*& p, uint32_t& out);
  void handleLine_(const char* lineZ);

  /* ---------- ELECTION/YIELD ---------- */
  uint8_t     idxInPrioList_(uint8_t id) const;
  void        scheduleElectionBackoff_();
  void        startClaiming_();
  void        stopClaiming_(bool won);
  void        becomeLeaderFromHere_();
  void        yieldTo_(uint8_t newLeader);
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
  uint8_t myId_     = (uint8_t) SEM_MESH_NODE_ID;
  uint8_t leaderId_ = 0xFF;

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
  static const uint8_t kPrio_[SEM_MESH_NUM_KNOWN_NODES];
};