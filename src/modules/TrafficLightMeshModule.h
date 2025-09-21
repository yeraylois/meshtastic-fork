/**************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

#include <Arduino.h>
#include "mesh/SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "mesh/generated/meshtastic/mesh.pb.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

/* =======================[ DEFAULT CONFIG (OVERRIDE VIA -D ...) ]=======================
 *  ROLE:
 *    -DROLE_LEADER / -DROLE_FOLLOWER
 *
 *  NODE ID (WILL DEFAULT TO 0 FOR LEADER, 1 FOR FOLLOWER IF NOT PROVIDED):
 *    -D SEM_NODE_ID=0|1|2
 *
 *  TOPOLOGY / START:
 *    -D SEM_TOPOLOGY=2|3
 *    -D SEM_START_CASE=1|2|3
 *
 *  TIMING:
 *    -D SEM_CASE_INTERVAL_MS=25000
 *    -D SEM_AMBER_INTERVAL_MS=5000
 *    -D SEM_AMBER_BLINK_MS=500
 *
 *  LEASE / BEACONS:
 *    -D SEM_BEACON_PERIOD_MS=2000
 *    -D SEM_LOSS_TIMEOUT_MS=8000
 *    -D SEM_LEASE_MS=15000
 *    -D SEM_RENEW_BEFORE_MS=5000
 *
 *  ELECTION / HANDOVER:
 *    -D FOLLOWER_YIELD_GRACE_MS=3000
 *    -D ELECTION_BACKOFF_MIN_MS=300
 *    -D ELECTION_BACKOFF_MAX_MS=800
 *    -D HANDOVER_DELAY_MS=700
 *
 *  PRIORITY ORDER (RANK 0 = HIGHEST):
 *    -D SEM_PRIORITY_0=0
 *    -D SEM_PRIORITY_1=1
 *    -D SEM_PRIORITY_2=2
 * =====================================================================================*/

/* =============================[ NODE ID DEFAULTS ]============================= */
#ifndef SEM_NODE_ID
  #ifdef ROLE_LEADER
    #define SEM_NODE_ID 0
  #else
    #define SEM_NODE_ID 1
  #endif
#endif
/* ============================================================================== */

class TrafficLightMeshModule final : public SinglePortModule, public concurrency::OSThread
{
public:
    /* =============================[ PORT SELECTION ]============================= */
    /* USE PRIVATE_APP SO DISPLAY DOES NOT WAKE ON TEXT APP TRAFFIC                */
    static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

    TrafficLightMeshModule();

    /* =================================[ THREAD ]================================ */
    /* COOPERATIVE LOOP (≈ ARDUINO LOOP)                                           */
    int32_t runOnce() override;

    /* ===============================[ RX HANDLER ]============================== */
    ProcessMessage handleReceived(const meshtastic_MeshPacket &p) override;

    meshtastic_PortNum getPortNum() const { return kPort; }

    /* =============[ COMPAT PLACEHOLDER (NOT USED, KEPT FOR ABI) ]============== */
    static const uint16_t kPhaseDurationsMs_[4];

private:
    /* ===========================[ ROLE / STATE TICKS ]========================== */
    void leaderTick_();
    void followerTick_();

    /* ===============================[ SAFETY MODE ]============================= */
    void enterSafety_();
    void exitSafety_();

    /* =================================[ TX SIDE ]=============================== */
    void sendBeacon_();

    /* ==========================[ ELECTION / PRIORITY ]========================== */
    uint8_t  idxInPrioList_(uint8_t id) const;
    bool     isHigherPriority_(uint8_t a, uint8_t b) const;
    uint32_t computeBackoffMs_(uint8_t rank) const;
    void     scheduleHandoverTo_(uint8_t newLeader);

    /* ===========================[ LIGHT JSON PARSER ]=========================== */
    /* EXPECTS: {"t":"B","id":"...","lid":N,"seq":N,"c":X,"am":Y,"off":Z,"pe":N,"lt":N} */
    bool parseLeaderBeaconJson_(const char *s,
                                uint8_t &outCase,
                                uint8_t &outAmber,
                                uint8_t &outOffNode,
                                uint32_t &outPe,
                                uint32_t &outLt,
                                uint32_t &outSeq,
                                uint8_t  &outLeaderIdNum,
                                char *outLeaderName,
                                size_t outLeaderNameSz);

    static bool findUInt_(const char *s, const char *key, uint32_t &out);
    static bool findU8_(const char *s, const char *key, uint8_t &out);
    static bool findStr_(const char *s, const char *key, char *out, size_t outSz);

private:
    /* =======================[ TRAFFIC LIGHT “CASE” STATE ]====================== */
    const char *leaderLabel_ = nullptr; /* HUMAN-FRIENDLY TAG FOR LOGS/BEACONS   */
    uint8_t     caseIndex_   = 2;       /* 1→ID=1, 2→ID=0, 3→ID=2                */
    bool        inAmber_     = false;
    uint8_t     offNode_     = 0;       /* NODE THAT TRANSITIONS TO AMBER         */
    uint8_t     nextCase_    = 1;

    /* =================================[ TIMERS ]================================ */
    uint32_t tCaseStart_  = 0;
    uint32_t tAmberStart_ = 0;

    /* ==============================[ ROLE / LEADER ]============================ */
    const uint8_t myId_   = (uint8_t)SEM_NODE_ID;
    uint8_t       leaderId_ = 0;
    bool          isLeader_ = false;

    /* =====================[ HANDOVER / ELECTION TIMERS ]======================= */
    uint32_t handoverAt_           = 0;
    uint32_t electionBackoffUntil_ = 0;

    /* ===========================[ BEACON / LEASE / SYNC ]====================== */
    uint32_t lastBeaconRxMs_    = 0;
    uint32_t noSafetyUntil_     = 0;
    uint32_t seenLeaseExpiryMs_ = 0;
    uint32_t leaseExpiryMs_     = 0;
    uint32_t nextBeaconAt_      = 0;
    uint32_t seq_               = 0;

    bool inSafety_ = false;

    /* ===============[ PRIORITY TABLE DECLARATION (DEFINED IN .CPP) ]=========== */
    static const uint8_t kPrio_[3];
};