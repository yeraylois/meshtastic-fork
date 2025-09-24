/**************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/
#pragma once

#include <Arduino.h>
#include "configuration.h"
#include "mesh/SinglePortModule.h"
#include "concurrency/OSThread.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

/* =======================[ DEFAULT CONFIG (OVERRIDE VIA -D ...) ]=======================
 *  ROLE (CHOOSE EXACTLY ONE):
 *    -DRS485_ROLE_MASTER=1
 *    -DRS485_ROLE_SLAVE=1
 *
 *  NODE ID (UNIQUE PER NODE, 0..N):
 *    -DRS485_NODE_ID=0|1|2|...
 *
 *  UART / PINS / BAUD:
 *    -DRS485_PIN_RX=<GPIO>                 // RS-485 RX (UART1 BY DEFAULT)
 *    -DRS485_PIN_TX=<GPIO>                 // RS-485 TX
 *    -DRS485_PIN_DIR=<GPIO>                // RS-485 DE/RE (HIGH=TX, LOW=RX)
 *    -DRS485_BAUD=9600|19200|38400|...     // DEFAULT: 9600
 *
 *  TOPOLOGY / START:
 *    -DRS485_TOPOLOGY=2|3                  // 2 NODES (2<->1) OR 3 NODES (2->1->3)
 *    -DRS485_START_CASE=1|2|3              // INITIAL CASE (DEFAULT: 2)
 *
 *  TIMING (MS):
 *    -DRS485_CASE_INTERVAL_MS=25000        // STABLE GREEN DURATION
 *    -DRS485_AMBER_INTERVAL_MS=5000        // AMBER TRANSITION DURATION
 *    -DRS485_AMBER_BLINK_MS=500            // SAFETY BLINK PERIOD (~1 HZ)
 *    -DRS485_BEACON_PERIOD_MS=2000         // MASTER STATE BEACON PERIOD
 *    -DRS485_LOSS_TIMEOUT_MS=8000          // NO-BEACON -> SAFETY/ELECTION
 *
 *  ELECTION / HANDOVER:
 *    -DRS485_BACKOFF_MIN_MS=300            // RANDOM BACKOFF WINDOW (MIN)
 *    -DRS485_BACKOFF_MAX_MS=800            // RANDOM BACKOFF WINDOW (MAX)
 *    -DRS485_YIELD_GRACE_MS=700            // FOLLOWER YIELD GRACE FOR HANDOVER
 *
 *  PRIORITY ORDER (RANK 0 = HIGHEST; OMIT UNUSED):
 *    -DRS485_PRIO0=<NODE_ID>
 *    -DRS485_PRIO1=<NODE_ID>
 *    -DRS485_PRIO2=<NODE_ID>
 *    // EXAMPLE (LEADER(0) > F1(1) > F2(2)):
 *    // -DRS485_PRIO0=0 -DRS485_PRIO1=1 -DRS485_PRIO2=2
 *
 *  LOGGING / SAFETY (OPTIONAL):
 *    -DRS485_LOG_LEVEL=0|1|2               // 0=OFF, 1=INFO, 2=DEBUG
 *    -DRS485_ASSERTS=1                      // EXTRA RUNTIME CHECKS
 * =====================================================================================*/

#define LOG_TAG "TrafficRs485Coordinator"

/* ==========================[ DEFAULTS (OVERRIDE VIA -D ...) ]========================== */
/* PINS (DE/RE == DIR) */
#ifndef RS485_PIN_RX
#define RS485_PIN_RX 34
#endif
#ifndef RS485_PIN_TX
#define RS485_PIN_TX 33
#endif
#ifndef RS485_PIN_DIR
#define RS485_PIN_DIR 21
#endif
#ifndef RS485_BAUD
#define RS485_BAUD 9600
#endif

/* USER LEDS (-1 TO DISABLE) */
#ifndef LED_RED_PIN
#define LED_RED_PIN -1
#endif
#ifndef LED_AMBER_PIN
#define LED_AMBER_PIN -1
#endif
#ifndef LED_GREEN_PIN
#define LED_GREEN_PIN -1
#endif

/* NODE IDENTIFICATION */
#ifndef RS485_NODE_ID
#define RS485_NODE_ID 0 /* 0..N-1 */
#endif

/* START ROLE: IF DEFINED, NODE STARTS AS LEADER; OTHERWISE FOLLOWER */
#ifndef ROLE_LEADER
/* UNDEFINED => START AS FOLLOWER */
#endif

/* TOPOLOGY: 2 OR 3 (CASE ROTATION) */
#ifndef RS485_TOPOLOGY
#define RS485_TOPOLOGY 3
#endif

/* PRIORITY LIST (LEADER ELECTION ORDER). LOWER INDEX => HIGHER PRIORITY */
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

/* TIMERS (MS) */
#ifndef RS_CASE_INTERVAL_MS
#define RS_CASE_INTERVAL_MS 25000U
#endif
#ifndef RS_AMBER_INTERVAL_MS
#define RS_AMBER_INTERVAL_MS 5000U
#endif
#ifndef RS_BEACON_PERIOD_MS
#define RS_BEACON_PERIOD_MS 2000U
#endif
#ifndef RS_LOSS_TIMEOUT_MS
#define RS_LOSS_TIMEOUT_MS 8000U
#endif
#ifndef RS_LEASE_MS
#define RS_LEASE_MS 15000U
#endif
#ifndef RS_RENEW_BEFORE_MS
#define RS_RENEW_BEFORE_MS 5000U
#endif
#ifndef RS_ELECT_BACKOFF_BASE_MS
#define RS_ELECT_BACKOFF_BASE_MS 800U
#endif
#ifndef RS_ELECT_BACKOFF_STEP_MS
#define RS_ELECT_BACKOFF_STEP_MS 600U
#endif
#ifndef RS_ELECT_JITTER_MS
#define RS_ELECT_JITTER_MS 400U
#endif
#ifndef RS_CLAIM_WINDOW_MS
#define RS_CLAIM_WINDOW_MS 1200U
#endif
#ifndef RS_AMBER_BLINK_MS
#define RS_AMBER_BLINK_MS 500U /* SAFETY BLINK ≈ 1 HZ */
#endif
/* ====================================================================================== */

class TrafficRs485CoordinatorModule final : public SinglePortModule, public concurrency::OSThread
{
public:
    static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

    TrafficRs485CoordinatorModule();

    /* ================================[ OSTHREAD ]==================================== */
    int32_t runOnce() override;

    /* ===============================[ MESH MODULE ]================================== */
    ProcessMessage handleReceived(const meshtastic_MeshPacket &) override
    {
        return ProcessMessage::CONTINUE;
    }
    meshtastic_PortNum getPortNum() const { return kPort; }

private:
    /* ================================[ LOW LEVEL IO ]================================ */
    void initOnce();
    void beginUart();
    inline void setTx(bool en);
    static uint8_t computeXOR(const uint8_t *data, size_t len);
    void sendFrame(const char *buf, size_t len);
    void pumpRx();

    /* ================================[ LED HELPERS ]================================= */
    static inline bool ledsPresent();
    static inline void leds(bool r, bool a, bool g);
    void applyCaseLocal(uint8_t c);
    void applyAmberLocal(uint8_t offNode);
    void applySafetyBlink();

    /* ===============================[ CASE & TOPOLOGY ]============================== */
    static inline uint8_t greenNode(uint8_t c);
    static inline uint8_t nextCase(uint8_t curr);

    /* ===============================[ PROTOCOL PARSE ]=============================== */
    void handleLine(const char *lineZ);
    bool parseCSV_u8(const char *&p, uint8_t &out);
    bool parseCSV_u16(const char *&p, uint16_t &out);
    bool parseCSV_u32(const char *&p, uint32_t &out);

    /* ===============================[ ELECTION LOGIC ]=============================== */
    uint8_t idxInPrioList_(uint8_t id) const;
    void scheduleElectionBackoff_();
    void startClaiming_();
    void stopClaiming_(bool won);
    void becomeLeaderFromHere_();
    void yieldTo_(uint8_t newLeader);

    /* ===============================[ LEADER LOGIC ]================================ */
    void leaderTick_();
    void sendBeacon_();

    /* ===============================[ FOLLOWER LOGIC ]============================== */
    void followerTick_();

private:
    /* ===============================[ CONSTANTS ]==================================== */
    static const uint8_t kPrio_[RS485_NUM_KNOWN_NODES];

    /* ===============================[ UART / BUFFER ]================================ */
    uint32_t t_bit_us_ = 0;
    uint32_t t_char_us_ = 0;
    static constexpr size_t RX_MAX = 192;
    char rxBuf_[RX_MAX + 1]{};
    size_t rxLen_ = 0;

    /* ===============================[ STATE - COMMON ]=============================== */
    const uint8_t myId_ = (uint8_t)RS485_NODE_ID;

    bool ready_ = false;
    bool isLeader_ =
#ifdef ROLE_LEADER
        true
#else
        false
#endif
        ;
    /**
     * TRAFFIC CASE STATE
     *  - caseIndex_: ACTIVE CASE (1..3)
     *  - inAmber_: TRANSITION FLAG
     *  - offNode_: NODE THAT TURNS OFF
     *  - nextCase_: NEXT CASE TO ACTIVATE
     */
    uint8_t caseIndex_ = 2;
    bool inAmber_ = false;
    uint8_t offNode_ = 0;
    uint8_t nextCase_ = 1;

    /* TIMERS */
    uint32_t tCaseStart_ = 0;
    uint32_t tAmberStart_ = 0;

    /* ===============================[ LEADER STATE ]================================= */
    uint32_t leaseExpiryMs_ = 0;
    uint32_t nextBeaconAt_ = 0;
    uint32_t seq_ = 0;

    /* ===============================[ FOLLOWER STATE ]=============================== */
    uint32_t lastBeaconRxMs_ = 0;
    uint32_t seenLeaseExpiryMs_ = 0;
    bool inSafety_ = false;

    /* ===============================[ ELECTION STATE ]=============================== */
    uint8_t leaderId_ =
#ifdef ROLE_LEADER
        (uint8_t)RS485_NODE_ID
#else
        0xFF
#endif
        ;
    uint32_t electBackoffUntilMs_ = 0;
    bool claiming_ = false;
    uint32_t claimUntilMs_ = 0;
    uint8_t observedLeaderRank_ = 0xFF; /* BEST (LOWEST) RANK SEEN DURING ELECTION */

    /* ===============================[ TX HELPERS ]=================================== */
    void tx_Beacon_(uint8_t leaderId, uint32_t seq, uint8_t c, uint8_t am, uint8_t off,
                    uint32_t leaseTtlMs, uint32_t elapsedMs);
    void tx_Claim_(uint8_t id, uint8_t rank);
    void tx_Yield_(uint8_t fromId, uint8_t toId);
};