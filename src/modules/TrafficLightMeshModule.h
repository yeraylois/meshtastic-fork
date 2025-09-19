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

// =========================[ DEFAULT CONFIG (OVERRIDE VIA -D ...) ]=========================
// NOTE: NO DEFAULT VALUES ARE DEFINED HERE TO AVOID ALTERING LOGIC.
//
// SUGGESTED PLATFORMIO.INI FLAGS (EXAMPLES):
//   -D ROLE_LEADER / -DROLE_FOLLOWER
//   -D SEM_NODE_ID=0|1|2              // FORCE LOCAL NODE ID IF NEEDED
//   -D SEM_CASE_INTERVAL_MS=25000     // STABLE GREEN DURATION (MS)
//   -D SEM_AMBER_INTERVAL_MS=5000     // AMBER TRANSITION DURATION (MS)
//   -D SEM_AMBER_BLINK_MS=500         // SAFETY BLINK PERIOD (MS, ≈1 HZ)
//   -D SEMAPHORE_BEACON_PERIOD_MS=2000
//   -D SEMAPHORE_LOSS_TIMEOUT_MS=8000
//   -D SEMAPHORE_LEASE_MS=15000
//   -D SEMAPHORE_RENEW_BEFORE_MS=5000
//   -D SEM_TOPOLOGY=2|3               // OPTIONAL: ROTATION 2↔1 OR 2→1→3
//   -D SEM_START_CASE=1|2|3           // OPTIONAL: INITIAL CASE
// =========================================================================================

class TrafficLightMeshModule final : public SinglePortModule, public concurrency::OSThread
{
public:
    // =================================[ PORT SELECTION ]==================================
    // USE 'PRIVATE_APP' IN THE PROTOTYPE (COMPACT JSON PAYLOAD)
    static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

    TrafficLightMeshModule();

    // ====================================[ OSTHREAD ]=====================================
    // COOPERATIVE LOOP (≈ ARDUINO LOOP)
    int32_t runOnce() override;

    // ===================================[ MESH-MODULE ]===================================
    // RECEIVE ON PORT 'kPort' (DO NOT CONSUME OTHER PORTS)
    ProcessMessage handleReceived(const meshtastic_MeshPacket &p) override;

    // REQUIRED BY SinglePortModule
    meshtastic_PortNum getPortNum() const { return kPort; }

private:
    // ==============================[ ROLE / STATE HELPERS ]===============================
    void leaderTick_();   // LEADER LOGIC: CASE ROTATION, AMBER, BEACONS
    void followerTick_(); // FOLLOWER LOGIC: APPLY CASE/AMBER/SAFETY

    // ===================================[ SAFETY MODE ]===================================
    void enterSafety_();
    void exitSafety_();

    // ===================================[ TX BEACONS ]====================================
    void sendBeacon_(); // EMIT JSON WITH {C, AM, OFF, PE, LT}

    // ============================[ JSON PARSER (LIGHTWEIGHT) ]============================
    // EXPECTS: {"t":"B","id":"...","seq":N,"c":X,"am":Y,"off":Z,"pe":Y,"lt":Z}
    bool parseLeaderBeaconJson_(const char *s,
                                uint8_t &outCase,
                                uint8_t &outAmber,
                                uint8_t &outOffNode,
                                uint32_t &outPe,
                                uint32_t &outLt,
                                uint32_t &outSeq,
                                char *outLeader,
                                size_t outLeaderSz);

    // PARSE HELPERS (NOT A FULL JSON VALIDATOR; EXTRACTS KEY:VALUE PAIRS)
    static bool findUInt_(const char *s, const char *key, uint32_t &out);
    static bool findU8_(const char *s, const char *key, uint8_t &out);
    static bool findStr_(const char *s, const char *key, char *out, size_t outSz);

private:
    // =========================[ STATE: TRAFFIC LIGHT BY “CASES” ]=========================
    // LABEL USED IN LOGS/BEACONS ("WS3-LEADER" / "T114-FOLLOWER")
    const char *leaderLabel_ = nullptr;

    /**
     * ACTIVE CASE AND TRANSITION :
     *   CASE 1 -> GREEN NODE ID=1
     *   CASE 2 -> GREEN NODE ID=0
     *   CASE 3 -> GREEN NODE ID=2 (RESERVED FOR THIRD NODE)
     */
    uint8_t caseIndex_ = 2; // START IN CASE 2 (MASTER GREEN)
    bool inAmber_ = false;
    uint8_t offNode_ = 0;  // NODE THAT TURNS OFF (WAS GREEN BEFORE CHANGING)
    uint8_t nextCase_ = 1; // NEXT CASE (2↔1 WITH 2 NODES)

    // TIMERS FOR STABLE SEGMENT AND AMBER TRANSITION
    uint32_t tCaseStart_ = 0;  // START OF “STABLE GREEN”
    uint32_t tAmberStart_ = 0; // START OF “AMBER TRANSITION”

    // =============================[ BEACONS / LEASE / SYNC ]==============================
    uint32_t lastBeaconRxMs_ = 0;    // FOLLOWER: LAST BEACON RX
    uint32_t seenLeaseExpiryMs_ = 0; // FOLLOWER: LEADER LEASE EXPIRY (SEEN)
    uint32_t leaseExpiryMs_ = 0;     // LEADER: LOCAL LEASE EXPIRY
    uint32_t nextBeaconAt_ = 0;      // LEADER: NEXT BEACON SCHEDULE
    uint32_t seq_ = 0;               // LEADER: BEACON SEQUENCE

    // ===================================[ SAFETY FLAG ]===================================
    bool inSafety_ = false;
};