/**************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#include "TrafficLightMeshModule.h"
#include "configuration.h"
#include "mesh/MeshService.h"
#include "Channels.h"
#include <string.h>
#include <stdio.h>

#define LOG_TAG "Semaphore"

// =================[ LED PIN DEFINITIONS (OVERRIDE VIA -D ...) ]=================
// IF YOU PASS PINS VIA BUILD FLAGS (E.G. -DSEM_LED_RED_PIN=XX), THEY WILL TAKE PRECEDENCE.
// IF NOT DEFINED, FALL BACK TO -1 (DISABLE LOCAL LED HARDWARE).

#ifndef SEM_LED_RED_PIN
#define SEM_LED_RED_PIN -1
#endif
#ifndef SEM_LED_AMBER_PIN
#define SEM_LED_AMBER_PIN -1
#endif
#ifndef SEM_LED_GREEN_PIN
#define SEM_LED_GREEN_PIN -1
#endif

// =================================[ NODE IDS ]==================================
// 0 = MASTER (WS3), 1 = SLAVE1 (T114), 2 = SLAVE2 (T114 OR OTHER).
#ifndef SEM_NODE_ID
#ifdef ROLE_LEADER
#define SEM_NODE_ID 0
#else
#define SEM_NODE_ID 1
#endif
#endif

// ============================[ TOPOLOGY / START CASE ]==========================
// DEFAULTS SUIT A 3-NODE SETUP (LEADER + 2 FOLLOWERS). OVERRIDE VIA -D IF NEEDED.
#ifndef SEM_TOPOLOGY
#define SEM_TOPOLOGY 3
#endif
#ifdef SEM_START_CASE
#define SEM_START_CASE_DEFINED 1
#endif

// ============================[ TIMING (BOTH NODES) ]============================
// GREEN STABLE INTERVAL, AMBER TRANSITION, AND SAFETY BLINK (≈1 HZ).
#ifndef SEM_CASE_INTERVAL_MS
#define SEM_CASE_INTERVAL_MS 25000U
#endif
#ifndef SEM_AMBER_INTERVAL_MS
#define SEM_AMBER_INTERVAL_MS 5000U
#endif
#ifndef SEM_AMBER_BLINK_MS
#define SEM_AMBER_BLINK_MS 500U
#endif

// ============================[ MESHTASTIC EXTERNS ]=============================
extern MeshService *service;
extern Channels channels;

// ===========================[ LEASE / BEACON FLAGS ]============================
#ifndef SEMAPHORE_BEACON_PERIOD_MS
#define SEMAPHORE_BEACON_PERIOD_MS 2000U
#endif
#ifndef SEMAPHORE_LOSS_TIMEOUT_MS
#define SEMAPHORE_LOSS_TIMEOUT_MS 8000U
#endif
#ifndef SEMAPHORE_LEASE_MS
#define SEMAPHORE_LEASE_MS 15000U
#endif
#ifndef SEMAPHORE_RENEW_BEFORE_MS
#define SEMAPHORE_RENEW_BEFORE_MS 5000U
#endif

// ================================[ LED HELPERS ]================================
static inline bool sem_hw_present()
{
    return SEM_LED_RED_PIN >= 0 && SEM_LED_AMBER_PIN >= 0 && SEM_LED_GREEN_PIN >= 0;
}
static inline void sem_leds(bool r, bool a, bool g)
{
    if (!sem_hw_present())
        return;
    digitalWrite(SEM_LED_RED_PIN, r ? HIGH : LOW);
    digitalWrite(SEM_LED_AMBER_PIN, a ? HIGH : LOW);
    digitalWrite(SEM_LED_GREEN_PIN, g ? HIGH : LOW);
}

/**
 * GET GREEN NODE ID FOR A GIVEN CASE
 * CASE 1 -> ID=1 (SLAVE1),
 * CASE 2 -> ID=0 (MASTER),
 * CASE 3 -> ID=2 (SLAVE2).
 */
static inline uint8_t sem_green_node(uint8_t c)
{
    if (c == 1)
        return 1;
    if (c == 2)
        return 0;
    return 2; // CASE 3
}

/** TODO: TRY TO CREATE A GENERIC FUNCTION FOR THIS TO COVER ALL TOPOLOGIES */
// NEXT CASE GIVEN CURRENT CASE AND TOPOLOGY
static inline uint8_t sem_next_case(uint8_t curr)
{
#if (SEM_TOPOLOGY == 3)
    // 3-NODE ROTATION: 2 -> 1 -> 3 -> 2 -> ...
    return (curr == 2) ? 1 : ((curr == 1) ? 3 : 2);
#else
    // 2-NODE ROTATION: 2 <-> 1
    return (curr == 2) ? 1 : 2;
#endif
}

// APPLY LOCAL LEDS ACCORDING TO "CASE" (WINNER = GREEN, OTHERS = RED).
static inline void sem_apply_case(uint8_t c, uint8_t myId)
{
    if (!sem_hw_present())
        return;
    const uint8_t g = sem_green_node(c);
    if (g == myId)
        sem_leds(false, false, true); // GREEN
    else
        sem_leds(true, false, false); // RED
}

// APPLY FIXED AMBER ONLY TO THE NODE THAT TURNS OFF (OFFNODE). OTHERS: RED.
static inline void sem_apply_amber_off(uint8_t offNode, uint8_t myId)
{
    if (!sem_hw_present())
        return;
    if (offNode == myId)
        sem_leds(false, true, false); // AMBER
    else
        sem_leds(true, false, false); // RED
}

// SAFETY-MODE: AMBER BLINK (≈1 HZ).
static inline void sem_apply_safety_blink()
{
    if (!sem_hw_present())
        return;
    const bool on = ((millis() / SEM_AMBER_BLINK_MS) & 1) != 0;
    sem_leds(false, on, false);
}

/* ================================== CONSTRUCTOR ================================== */

TrafficLightMeshModule::TrafficLightMeshModule()
    : SinglePortModule("traffic_semaphore", kPort), concurrency::OSThread("Semaphore")
{
    // FRIENDLY LABEL FOR LOGS/BEACONS
#ifdef ROLE_LEADER
    leaderLabel_ = "WS3-LEADER";
#else
    leaderLabel_ = "T114-FOLLOWER";
#endif

    // GPIO LED INITIALIZATION
    if (sem_hw_present())
    {
        pinMode(SEM_LED_RED_PIN, OUTPUT);
        pinMode(SEM_LED_AMBER_PIN, OUTPUT);
        pinMode(SEM_LED_GREEN_PIN, OUTPUT);
        sem_leds(true, false, false); // SAFE START -> RED
    }

    // USER INITIAL ACTIVE CASE
#ifdef SEM_START_CASE_DEFINED
    caseIndex_ = (uint8_t)SEM_START_CASE; // HONOR USER-DEFINED START
    if (caseIndex_ < 1 || caseIndex_ > 3)
        caseIndex_ = 2;
#else
    caseIndex_ = 2; // DEFAULT: MASTER GREEN
#endif
    inAmber_ = false;
    offNode_ = sem_green_node(caseIndex_);
    nextCase_ = sem_next_case(caseIndex_);
    tCaseStart_ = millis();
    tAmberStart_ = 0;

#ifdef ROLE_LEADER
    leaseExpiryMs_ = millis() + SEMAPHORE_LEASE_MS;
#else
    leaseExpiryMs_ = 0;
#endif

    nextBeaconAt_ = millis(); // FIRST BEACON SOON

    // APPLY LOCAL LEDS FOR INITIAL CASE
    sem_apply_case(caseIndex_, SEM_NODE_ID);

    LOG_INFO("ctor (role=%s) start_case=%u\n",
#ifdef ROLE_LEADER
             "LEADER"
#else
             "FOLLOWER"
#endif
             ,
             (unsigned)caseIndex_);
}

/* =============================== COOPERATIVE LOOP ============================== */

int32_t TrafficLightMeshModule::runOnce()
{
#ifdef ROLE_LEADER
    leaderTick_();
#else
    followerTick_();
#endif
    return 50; // MS
}

/* ================================== LEADER SIDE ================================= */

void TrafficLightMeshModule::leaderTick_()
{
    const uint32_t now = millis();

    // LEASE RENEWAL WITH MARGIN
    if ((int32_t)(leaseExpiryMs_ - now) <= (int32_t)SEMAPHORE_RENEW_BEFORE_MS)
    {
        leaseExpiryMs_ = now + SEMAPHORE_LEASE_MS;
        LOG_INFO("lease_renew -> expires_in=%lu ms\n", (unsigned long)(leaseExpiryMs_ - now));
    }

    // SEQUENCE: [STABLE GREEN] -> [AMBER] -> APPLY NEXT CASE
    if (!inAmber_)
    {
        // STABLE GREEN IN PROGRESS
        if ((int32_t)(now - tCaseStart_) >= (int32_t)SEM_CASE_INTERVAL_MS)
        {
            // ENTER AMBER: NODE THAT WAS GREEN TURNS AMBER (OFFNODE = CURRENT WINNER)
            inAmber_ = true;
            tAmberStart_ = now;
            offNode_ = sem_green_node(caseIndex_);
            nextCase_ = sem_next_case(caseIndex_);

            sem_apply_amber_off(offNode_, SEM_NODE_ID);
            LOG_INFO("AMBER begin offNode=%u (from case=%u)\n", (unsigned)offNode_, (unsigned)caseIndex_);
        }
    }
    else
    {
        // AMBER IN PROGRESS
        if ((int32_t)(now - tAmberStart_) >= (int32_t)SEM_AMBER_INTERVAL_MS)
        {
            // END AMBER, APPLY NEW CASE
            inAmber_ = false;
            caseIndex_ = nextCase_;
            tCaseStart_ = now;

            sem_apply_case(caseIndex_, SEM_NODE_ID);
            LOG_INFO("CASE apply %u\n", (unsigned)caseIndex_);
        }
    }

    // PERIODIC BEACON
    if ((int32_t)(now - nextBeaconAt_) >= 0)
    {
        sendBeacon_();
        nextBeaconAt_ = now + SEMAPHORE_BEACON_PERIOD_MS;
    }
}

/* =================================== TX BEACON ================================== */

void TrafficLightMeshModule::sendBeacon_()
{
    const uint32_t now = millis();

    const uint32_t elapsed = (!inAmber_) ? (now - tCaseStart_)   // TIME IN STABLE GREEN
                                         : (now - tAmberStart_); // TIME IN AMBER
    const uint32_t leaseTt = (leaseExpiryMs_ > now) ? (leaseExpiryMs_ - now) : 0;

    // COMPACT JSON: t=B, id=label, seq, c=case(1..3), am=0/1, off=offNode, pe=elapsed_ms, lt=lease_ttl_ms
    char json[180];
    snprintf(json, sizeof(json),
             "{\"t\":\"B\",\"id\":\"%s\",\"seq\":%lu,\"c\":%u,\"am\":%u,\"off\":%u,\"pe\":%lu,\"lt\":%lu}",
             leaderLabel_, (unsigned long)seq_, (unsigned)caseIndex_,
             (unsigned)(inAmber_ ? 1 : 0), (unsigned)offNode_,
             (unsigned long)elapsed, (unsigned long)leaseTt);
    seq_++;

    // IMPORTANT: PACKET MUST BE HEAP-ALLOCATED (MESHTASTIC FREES AFTER TX)
    meshtastic_MeshPacket *pkt = (meshtastic_MeshPacket *)calloc(1, sizeof(*pkt));
    if (!pkt)
    {
        LOG_ERROR("sendBeacon_: OOM\n");
        return;
    }

    pkt->to = NODENUM_BROADCAST;
    pkt->channel = channels.getPrimaryIndex();
    pkt->want_ack = false;
    pkt->hop_start = 0;
    pkt->hop_limit = 0; // LET ROUTER APPLY DEFAULT HOP_LIMIT

    pkt->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    pkt->decoded.portnum = kPort; // PRIVATE_APP (PROTOTYPE)
    pkt->decoded.want_response = false;

    const size_t len = strnlen(json, sizeof(pkt->decoded.payload.bytes));
    pkt->decoded.payload.size = len;
    memcpy(pkt->decoded.payload.bytes, json, len);

    service->sendToMesh(pkt);

    LOG_DEBUG("beacon_tx: %s\n", json);
}

/* ================================= FOLLOWER SIDE ================================ */

void TrafficLightMeshModule::followerTick_()
{
    const uint32_t now = millis();

    // LONG BEACON LOSS -> SAFETY BLINK
    if (!inSafety_ && lastBeaconRxMs_ != 0 &&
        (int32_t)(now - lastBeaconRxMs_) > (int32_t)SEMAPHORE_LOSS_TIMEOUT_MS)
    {
        enterSafety_();
    }

    // APPLY LOCAL LEDS BASED ON CURRENT STATE
    if (inSafety_)
    {
        sem_apply_safety_blink();
    }
    else
    {
        if (inAmber_)
        {
            sem_apply_amber_off(offNode_, SEM_NODE_ID); // ONLY OFFNODE SEES AMBER; OTHERS RED
        }
        else
        {
            sem_apply_case(caseIndex_, SEM_NODE_ID); // ACTIVE CASE -> GREEN/RED
        }
    }

    // LOCAL TELEMETRY (USEFUL TO SEE STATE)
    LOG_DEBUG("local lights: safety=%d, amber=%d, case=%u, offNode=%u\n",
              (int)inSafety_, (int)inAmber_, (unsigned)caseIndex_, (unsigned)offNode_);
}

void TrafficLightMeshModule::enterSafety_()
{
    inSafety_ = true;
    LOG_WARN("safety_enter (no beacon > %u ms)\n", (unsigned)SEMAPHORE_LOSS_TIMEOUT_MS);
}
void TrafficLightMeshModule::exitSafety_()
{
    if (inSafety_)
    {
        inSafety_ = false;
        LOG_INFO("safety_exit (valid beacon)\n");
    }
}

/* =============================== RX ON PRIVATE_APP =============================== */

ProcessMessage TrafficLightMeshModule::handleReceived(const meshtastic_MeshPacket &p)
{
    if (p.which_payload_variant != meshtastic_MeshPacket_decoded_tag)
    {
        return ProcessMessage::CONTINUE;
    }
    if (p.decoded.portnum != kPort)
    {
        return ProcessMessage::CONTINUE;
    }

    const size_t n = p.decoded.payload.size;
    if (n == 0 || n >= sizeof(p.decoded.payload.bytes))
    {
        return ProcessMessage::CONTINUE;
    }

#ifndef ROLE_LEADER
    char buf[256];
    const size_t m = (n < sizeof(buf) - 1) ? n : sizeof(buf) - 1;
    memcpy(buf, p.decoded.payload.bytes, m);
    buf[m] = '\0';

    // PARSE BEACON WITH "CASE/AMBER/OFF"
    uint8_t c = 2, am = 0, off = 0;
    uint32_t pe = 0, lt = 0, seq = 0;
    char leader[32] = {0};

    if (parseLeaderBeaconJson_(buf, c, am, off, pe, lt, seq, leader, sizeof(leader)))
    {
        const uint32_t now = millis();
        lastBeaconRxMs_ = now;
        seenLeaseExpiryMs_ = now + lt;

        // UPDATE REMOTE STATE
        caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
        inAmber_ = (am != 0);
        offNode_ = off;

        exitSafety_();

        // APPLY IMMEDIATELY FOR VISIBLE EFFECT
        if (inAmber_)
            sem_apply_amber_off(offNode_, SEM_NODE_ID);
        else
            sem_apply_case(caseIndex_, SEM_NODE_ID);

        LOG_INFO("beacon_rx id=%s seq=%lu case=%u am=%u off=%u pe=%lu lt=%lu\n",
                 leader, (unsigned long)seq, (unsigned)caseIndex_,
                 (unsigned)inAmber_, (unsigned)offNode_,
                 (unsigned long)pe, (unsigned long)lt);

        return ProcessMessage::CONTINUE;
    }
#endif

    return ProcessMessage::CONTINUE;
}

/* ================================== FAST JSON PARSE ================================== */
// EXPECTS: {"t":"B","id":"...","seq":N,"c":X,"am":Y,"off":Z,"pe":N,"lt":N}
bool TrafficLightMeshModule::parseLeaderBeaconJson_(
    const char *s, uint8_t &outCase, uint8_t &outAmber, uint8_t &outOffNode,
    uint32_t &outPe, uint32_t &outLt, uint32_t &outSeq, char *outLeader, size_t outLeaderSz)
{
    if (!strstr(s, "\"t\":\"B\""))
        return false;

    bool ok = true;
    ok &= findU8_(s, "\"c\"", outCase);
    ok &= findU8_(s, "\"am\"", outAmber);
    ok &= findU8_(s, "\"off\"", outOffNode);
    ok &= findUInt_(s, "\"pe\"", outPe);
    ok &= findUInt_(s, "\"lt\"", outLt);
    ok &= findUInt_(s, "\"seq\"", outSeq);
    ok &= findStr_(s, "\"id\"", outLeader, outLeaderSz);
    return ok;
}

// HELPERS: SEEK KEY AND READ NEXT NUMBER/STRING (NOT A FULL JSON VALIDATOR)
bool TrafficLightMeshModule::findUInt_(const char *s, const char *key, uint32_t &out)
{
    const char *p = strstr(s, key);
    if (!p)
        return false;
    p = strchr(p, ':');
    if (!p)
        return false;
    p++;
    while (*p == ' ')
        p++;
    out = (uint32_t)strtoul(p, nullptr, 10);
    return true;
}
bool TrafficLightMeshModule::findU8_(const char *s, const char *key, uint8_t &out)
{
    uint32_t tmp = 0;
    if (!findUInt_(s, key, tmp))
        return false;
    out = (uint8_t)tmp;
    return true;
}
bool TrafficLightMeshModule::findStr_(const char *s, const char *key, char *out, size_t outSz)
{
    const char *p = strstr(s, key);
    if (!p)
        return false;
    p = strchr(p, ':');
    if (!p)
        return false;
    p++;
    while (*p == ' ')
        p++;
    if (*p != '\"')
        return false;
    p++;
    size_t i = 0;
    while (*p && *p != '\"' && i < outSz - 1)
    {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return (*p == '\"');
}