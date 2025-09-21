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

/* =================[ LED PIN DEFINITIONS (OVERRIDE VIA -D ...) ]================ */
#ifndef SEM_LED_RED_PIN
  #define SEM_LED_RED_PIN   -1
#endif
#ifndef SEM_LED_AMBER_PIN
  #define SEM_LED_AMBER_PIN -1
#endif
#ifndef SEM_LED_GREEN_PIN
  #define SEM_LED_GREEN_PIN -1
#endif
/* ============================================================================== */

/* ===========================[ TOPOLOGY / START CASE ]========================== */
#ifndef SEM_TOPOLOGY
  #define SEM_TOPOLOGY 3
#endif
#ifdef SEM_START_CASE
  #define SEM_START_CASE_DEFINED 1
#endif
/* ============================================================================== */

/* ==============================[ TIMING (ALL NODES) ]========================== */
#ifndef SEM_CASE_INTERVAL_MS
  #define SEM_CASE_INTERVAL_MS 25000U
#endif
#ifndef SEM_AMBER_INTERVAL_MS
  #define SEM_AMBER_INTERVAL_MS 5000U
#endif
#ifndef SEM_AMBER_BLINK_MS
  #define SEM_AMBER_BLINK_MS 500U
#endif
/* ============================================================================== */

/* =============================[ LEASE / BEACON FLAGS ]========================= */
#ifndef SEM_BEACON_PERIOD_MS
  #define SEM_BEACON_PERIOD_MS 2000U
#endif
#ifndef SEM_LOSS_TIMEOUT_MS
  #define SEM_LOSS_TIMEOUT_MS 8000U
#endif
#ifndef SEM_LEASE_MS
  #define SEM_LEASE_MS 15000U
#endif
#ifndef SEM_RENEW_BEFORE_MS
  #define SEM_RENEW_BEFORE_MS 5000U
#endif
/* ============================================================================== */

/* =============================[ ELECTION / HANDOVER ]========================== */
#ifndef FOLLOWER_YIELD_GRACE_MS
  #define FOLLOWER_YIELD_GRACE_MS 3000U
#endif
#ifndef ELECTION_BACKOFF_MIN_MS
  #define ELECTION_BACKOFF_MIN_MS 300U
#endif
#ifndef ELECTION_BACKOFF_MAX_MS
  #define ELECTION_BACKOFF_MAX_MS 800U
#endif
#ifndef HANDOVER_DELAY_MS
  #define HANDOVER_DELAY_MS 700U
#endif
/* ============================================================================== */

/* ===============================[ MESHTASTIC EXTERNS ]========================= */
extern MeshService *service;
extern Channels channels;
/* ============================================================================== */

/* =========[ COMPAT PLACEHOLDER (NOT USED, KEPT FOR LINK COMPATIBILITY) ]====== */
const uint16_t TrafficLightMeshModule::kPhaseDurationsMs_[4] =
    {25000, 5000, 25000, 5000};
/* ============================================================================== */

/* =============================[ PRIORITY TABLE (DEF) ]========================= */
/* REAL DEFINITION (AVOIDS “UNDEFINED REFERENCE” DURING LINKING)                 */
const uint8_t TrafficLightMeshModule::kPrio_[3] = {
  #ifdef SEM_PRIORITY_0
    (uint8_t)SEM_PRIORITY_0,
  #else
    0,
  #endif
  #ifdef SEM_PRIORITY_1
    (uint8_t)SEM_PRIORITY_1,
  #else
    1,
  #endif
  #ifdef SEM_PRIORITY_2
    (uint8_t)SEM_PRIORITY_2
  #else
    2
  #endif
};
/* ============================================================================== */

/* =================================[ LED HELPERS ]============================== */
static inline bool sem_hw_present()
{
    return SEM_LED_RED_PIN >= 0 && SEM_LED_AMBER_PIN >= 0 && SEM_LED_GREEN_PIN >= 0;
}
static inline void sem_leds(bool r, bool a, bool g)
{
    if (!sem_hw_present()) return;
    digitalWrite(SEM_LED_RED_PIN,   r ? HIGH : LOW);
    digitalWrite(SEM_LED_AMBER_PIN, a ? HIGH : LOW);
    digitalWrite(SEM_LED_GREEN_PIN, g ? HIGH : LOW);
}
/* WHO IS GREEN FOR EACH CASE: 1→ID=1, 2→ID=0, 3→ID=2                                */
static inline uint8_t sem_green_node(uint8_t c)
{
    if (c == 1) return 1;
    if (c == 2) return 0;
    return 2; /* CASE 3 */
}
/* NEXT CASE BY TOPOLOGY: 3-NODE (2→1→3→2), ELSE 2-NODE (2↔1)                        */
static inline uint8_t sem_next_case(uint8_t curr)
{
  #if (SEM_TOPOLOGY == 3)
    return (curr == 2) ? 1 : ((curr == 1) ? 3 : 2);
  #else
    return (curr == 2) ? 1 : 2;
  #endif
}
/* APPLY LOCAL LEDS FOR CURRENT CASE                                                */
static inline void sem_apply_case(uint8_t c, uint8_t myId)
{
    if (!sem_hw_present()) return;
    const uint8_t g = sem_green_node(c);
    if (g == myId) sem_leds(false, false, true);  /* GREEN */
    else           sem_leds(true,  false, false); /* RED   */
}
/* APPLY AMBER ONLY ON THE NODE THAT TURNS OFF                                      */
static inline void sem_apply_amber_off(uint8_t offNode, uint8_t myId)
{
    if (!sem_hw_present()) return;
    if (offNode == myId) sem_leds(false, true,  false); /* AMBER */
    else                 sem_leds(true,  false, false); /* RED   */
}
/* SAFETY MODE: AMBER BLINK (≈ 1 HZ)                                                */
static inline void sem_apply_safety_blink()
{
    if (!sem_hw_present()) return;
    const bool on = ((millis() / SEM_AMBER_BLINK_MS) & 1) != 0;
    sem_leds(false, on, false);
}
/* VERY LIGHT PRNG FOR JITTER                                                       */
static uint32_t sem_rand32_()
{
    static uint32_t s = 0xA5A5F00Du ^ millis();
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s ^ millis();
}
/* ============================================================================== */

/* ================================== CONSTRUCTOR ================================== */
TrafficLightMeshModule::TrafficLightMeshModule()
    : SinglePortModule("traffic_semaphore", kPort)
    , concurrency::OSThread("Semaphore")
{
  #ifdef ROLE_LEADER
    leaderLabel_ = "WS3-LEADER";
  #else
    leaderLabel_ = "T114-FOLLOWER";
  #endif

    if (sem_hw_present())
    {
        pinMode(SEM_LED_RED_PIN,   OUTPUT);
        pinMode(SEM_LED_AMBER_PIN, OUTPUT);
        pinMode(SEM_LED_GREEN_PIN, OUTPUT);
        sem_leds(true, false, false); /* SAFE START → RED */
    }

  #ifdef SEM_START_CASE_DEFINED
    caseIndex_ = (uint8_t)SEM_START_CASE;
    if (caseIndex_ < 1 || caseIndex_ > 3) caseIndex_ = 2;
  #else
    caseIndex_ = 2; /* DEFAULT: MASTER GREEN */
  #endif

    inAmber_     = false;
    offNode_     = sem_green_node(caseIndex_);
    nextCase_    = sem_next_case(caseIndex_);
    tCaseStart_  = millis();
    tAmberStart_ = 0;

  #ifdef ROLE_LEADER
    isLeader_      = true;
    leaderId_      = myId_;
    leaseExpiryMs_ = millis() + SEM_LEASE_MS;
  #else
    isLeader_      = false;
    leaderId_      = 0; /* UNKNOWN UNTIL FIRST BEACON */
    leaseExpiryMs_ = 0;
  #endif

    nextBeaconAt_ = millis();
    sem_apply_case(caseIndex_, myId_);

    LOG_INFO("CONSTRUCTOR (role=%s) start_case=%u myId=%u\n",
             isLeader_ ? "LEADER" : "FOLLOWER",
             (unsigned)caseIndex_, (unsigned)myId_);
}
/* ============================================================================== */

/* =============================== COOPERATIVE LOOP ============================== */
int32_t TrafficLightMeshModule::runOnce()
{
    const uint32_t now = millis();

    /* DEFERRED HANDOVER (SMOOTH SWITCH) */
    if (handoverAt_ && (int32_t)(now - handoverAt_) >= 0)
    {
        const bool willLead = (leaderId_ == myId_);
        isLeader_ = willLead;

        if (willLead)
        {
            leaseExpiryMs_ = now + SEM_LEASE_MS;
            nextBeaconAt_  = 0;
            exitSafety_();
            LOG_INFO("handover: I AM THE LEADER NOW (id=%u)\n", (unsigned)myId_);
        }
        else
        {
            noSafetyUntil_        = now + FOLLOWER_YIELD_GRACE_MS;
            lastBeaconRxMs_       = now;
            electionBackoffUntil_ = 0;
            inSafety_             = false;
            LOG_INFO("handover: I YIELDED TO LEADER id=%u\n", (unsigned)leaderId_);
        }

        handoverAt_ = 0;
    }

    if (isLeader_) leaderTick_();
    else           followerTick_();

    return 50; /* MS */
}
/* ============================================================================== */

/* ================================== LEADER SIDE ================================= */
void TrafficLightMeshModule::leaderTick_()
{
    const uint32_t now = millis();

    /* LEASE RENEWAL WITH MARGIN */
    if ((int32_t)(leaseExpiryMs_ - now) <= (int32_t)SEM_RENEW_BEFORE_MS)
    {
        leaseExpiryMs_ = now + SEM_LEASE_MS;
        LOG_INFO("lease_renew → expires_in=%lu ms\n",
                 (unsigned long)(leaseExpiryMs_ - now));
    }

    /* STABLE GREEN → AMBER → NEXT CASE */
    if (!inAmber_)
    {
        if ((int32_t)(now - tCaseStart_) >= (int32_t)SEM_CASE_INTERVAL_MS)
        {
            inAmber_     = true;
            tAmberStart_ = now;
            offNode_     = sem_green_node(caseIndex_);
            nextCase_    = sem_next_case(caseIndex_);

            sem_apply_amber_off(offNode_, myId_);
            LOG_INFO("AMBER BEGIN offNode=%u (from case=%u)\n",
                     (unsigned)offNode_, (unsigned)caseIndex_);
        }
    }
    else
    {
        if ((int32_t)(now - tAmberStart_) >= (int32_t)SEM_AMBER_INTERVAL_MS)
        {
            inAmber_     = false;
            caseIndex_   = nextCase_;
            tCaseStart_  = now;

            sem_apply_case(caseIndex_, myId_);
            LOG_INFO("CASE APPLY %u\n", (unsigned)caseIndex_);
        }
    }

    /* PERIODIC BEACON */
    if ((int32_t)(now - nextBeaconAt_) >= 0)
    {
        sendBeacon_();
        nextBeaconAt_ = now + SEM_BEACON_PERIOD_MS;
    }
}
/* ============================================================================== */

/* =================================== TX BEACON ================================== */
void TrafficLightMeshModule::sendBeacon_()
{
    const uint32_t now = millis();

    const uint32_t elapsed = (!inAmber_) ? (now - tCaseStart_)
                                         : (now - tAmberStart_);
    const uint32_t leaseTt = (leaseExpiryMs_ > now) ? (leaseExpiryMs_ - now) : 0;

    /* COMPACT JSON WITH NUMERIC LEADER ID (lid) FOR PREEMPTION/ELECTION */
    char json[192];
    snprintf(json, sizeof(json),
             "{\"t\":\"B\",\"id\":\"%s\",\"lid\":%u,\"seq\":%lu,"
             "\"c\":%u,\"am\":%u,\"off\":%u,\"pe\":%lu,\"lt\":%lu}",
             leaderLabel_, (unsigned)myId_, (unsigned long)seq_,
             (unsigned)caseIndex_, (unsigned)(inAmber_ ? 1 : 0),
             (unsigned)offNode_, (unsigned long)elapsed, (unsigned long)leaseTt);
    seq_++;

    meshtastic_MeshPacket *pkt = (meshtastic_MeshPacket *)calloc(1, sizeof(*pkt));
    if (!pkt)
    {
        LOG_ERROR("sendBeacon_: OOM\n");
        return;
    }

    pkt->to         = NODENUM_BROADCAST;
    pkt->channel    = channels.getPrimaryIndex();
    pkt->want_ack   = false;
    pkt->hop_start  = 0;
    pkt->hop_limit  = 0;

    pkt->which_payload_variant = meshtastic_MeshPacket_decoded_tag;
    pkt->decoded.portnum       = kPort;
    pkt->decoded.want_response = false;

    const size_t len = strnlen(json, sizeof(pkt->decoded.payload.bytes));
    pkt->decoded.payload.size = len;
    memcpy(pkt->decoded.payload.bytes, json, len);

    service->sendToMesh(pkt);

    LOG_DEBUG("beacon_tx: %s\n", json);
}
/* ============================================================================== */

/* ================================= FOLLOWER SIDE ================================ */
void TrafficLightMeshModule::followerTick_()
{
    const uint32_t now = millis();

    /* LONG BEACON LOSS → SAFETY (WITH YIELD GRACE) */
    if (!inSafety_ && lastBeaconRxMs_ != 0 &&
        (int32_t)(now - lastBeaconRxMs_) > (int32_t)SEM_LOSS_TIMEOUT_MS &&
        (int32_t)(now - noSafetyUntil_) >= 0)
    {
        enterSafety_();

        const uint8_t  rank = idxInPrioList_(myId_);
        const uint32_t wait = computeBackoffMs_(rank);
        electionBackoffUntil_ = now + wait;
        LOG_INFO("election: BACKOFF UNTIL %lu ms (rank=%u)\n",
                 (unsigned long)electionBackoffUntil_, (unsigned)rank);
    }

    /* BACKOFF EXPIRED AND STILL NO BEACONS → SELF-PROMOTE */
    if (!isLeader_ && inSafety_ && electionBackoffUntil_ &&
        (int32_t)(now - electionBackoffUntil_) >= 0 &&
        (int32_t)(now - lastBeaconRxMs_) > (int32_t)SEM_LOSS_TIMEOUT_MS)
    {
        leaderId_          = myId_;
        handoverAt_        = now + HANDOVER_DELAY_MS;
        electionBackoffUntil_ = 0;
        LOG_INFO("election: SELF-PROMOTE TO LEADER (id=%u)\n", (unsigned)myId_);
    }

    /* LOCAL LEDS */
    if (inSafety_) sem_apply_safety_blink();
    else           (inAmber_ ? sem_apply_amber_off(offNode_, myId_)
                             : sem_apply_case(caseIndex_,   myId_));

    LOG_DEBUG("local lights: safety=%d, amber=%d, case=%u, offNode=%u leader=%u isLeader=%d\n",
              (int)inSafety_, (int)inAmber_, (unsigned)caseIndex_, (unsigned)offNode_,
              (unsigned)leaderId_, (int)isLeader_);
}
/* ============================================================================== */

/* =================================== SAFETY MODE ================================== */
void TrafficLightMeshModule::enterSafety_()
{
    inSafety_ = true;
    sem_leds(false, false, false);
    LOG_WARN("safety_enter (NO BEACON > %u ms)\n", (unsigned)SEM_LOSS_TIMEOUT_MS);
}
void TrafficLightMeshModule::exitSafety_()
{
    if (inSafety_)
    {
        inSafety_ = false;
        LOG_INFO("safety_exit (VALID BEACON)\n");
    }
}
/* ============================================================================== */

/* =============================== RX ON PRIVATE_APP =============================== */
ProcessMessage TrafficLightMeshModule::handleReceived(const meshtastic_MeshPacket &p)
{
    if (p.which_payload_variant != meshtastic_MeshPacket_decoded_tag)
        return ProcessMessage::CONTINUE;
    if (p.decoded.portnum != kPort)
        return ProcessMessage::CONTINUE;

    const size_t n = p.decoded.payload.size;
    if (n == 0 || n >= sizeof(p.decoded.payload.bytes))
        return ProcessMessage::CONTINUE;

    char buf[256];
    const size_t m = (n < sizeof(buf) - 1) ? n : sizeof(buf) - 1;
    memcpy(buf, p.decoded.payload.bytes, m);
    buf[m] = '\0';

    /* EXPECTS: {"t":"B","id":"...","lid":N,"seq":N,"c":X,"am":Y,"off":Z,"pe":N,"lt":N} */
    uint8_t  c = 2, am = 0, off = 0, lidNum = 0xFF;
    uint32_t pe = 0, lt = 0, seq = 0;
    char     leaderName[32] = {0};

    if (!parseLeaderBeaconJson_(buf, c, am, off, pe, lt, seq, lidNum, leaderName, sizeof(leaderName)))
        return ProcessMessage::CONTINUE;

    const uint32_t now = millis();

    lastBeaconRxMs_       = now;
    seenLeaseExpiryMs_    = now + lt;
    noSafetyUntil_        = 0;
    electionBackoffUntil_ = 0;

    /* IF I AM LEADER AND A HIGHER-PRIORITY LEADER APPEARS → YIELD */
    if (isLeader_ && lidNum != myId_ && isHigherPriority_(lidNum, myId_))
    {
        leaderId_   = lidNum;
        handoverAt_ = now + HANDOVER_DELAY_MS;
        LOG_INFO("preempted: HIGHER-PRIORITY LEADER id=%u\n", (unsigned)lidNum);
        return ProcessMessage::CONTINUE;
    }

    /* IF I AM FOLLOWER, ADOPT THE BEST VISIBLE LEADER */
    if (!isLeader_)
    {
        if (leaderId_ == 0xFF || leaderId_ == 0 || isHigherPriority_(lidNum, leaderId_))
            leaderId_ = lidNum;

        exitSafety_();

        caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
        inAmber_   = (am != 0);
        offNode_   = off;

        if (inAmber_) sem_apply_amber_off(offNode_, myId_);
        else          sem_apply_case(caseIndex_,   myId_);

        LOG_INFO("beacon_rx id=%s lid=%u seq=%lu case=%u am=%u off=%u pe=%lu lt=%lu\n",
                 leaderName, (unsigned)lidNum, (unsigned long)seq,
                 (unsigned)caseIndex_, (unsigned)inAmber_, (unsigned)offNode_,
                 (unsigned long)pe, (unsigned long)lt);

        /* OPTIONAL PREEMPTION: IF I HAVE HIGHER PRIORITY THAN EMITTER, TAKE OVER */
        if (isHigherPriority_(myId_, lidNum))
        {
            leaderId_   = myId_;
            handoverAt_ = now + HANDOVER_DELAY_MS;
            LOG_INFO("preempt: SCHEDULING TAKEOVER (me=%u) OVER id=%u\n",
                     (unsigned)myId_, (unsigned)lidNum);
        }
    }

    return ProcessMessage::CONTINUE;
}
/* ============================================================================== */

/* ================================ FAST JSON PARSER =============================== */
bool TrafficLightMeshModule::parseLeaderBeaconJson_(
    const char *s, uint8_t &outCase, uint8_t &outAmber, uint8_t &outOffNode,
    uint32_t &outPe, uint32_t &outLt, uint32_t &outSeq, uint8_t &outLeaderIdNum,
    char *outLeader, size_t outLeaderSz)
{
    if (!strstr(s, "\"t\":\"B\"")) return false;

    bool ok = true;
    ok &= findU8_(s,  "\"c\"",   outCase);
    ok &= findU8_(s,  "\"am\"",  outAmber);
    ok &= findU8_(s,  "\"off\"", outOffNode);
    ok &= findUInt_(s,"\"pe\"",  outPe);
    ok &= findUInt_(s,"\"lt\"",  outLt);
    ok &= findUInt_(s,"\"seq\"", outSeq);

    uint32_t lidTmp = 0;
    ok &= findUInt_(s,"\"lid\"", lidTmp);
    outLeaderIdNum = (uint8_t)lidTmp;

    ok &= findStr_(s, "\"id\"", outLeader, outLeaderSz);
    return ok;
}

bool TrafficLightMeshModule::findUInt_(const char *s, const char *key, uint32_t &out)
{
    const char *p = strstr(s, key); if (!p) return false;
    p = strchr(p, ':');             if (!p) return false;
    p++;
    while (*p == ' ') p++;
    out = (uint32_t)strtoul(p, nullptr, 10);
    return true;
}
bool TrafficLightMeshModule::findU8_(const char *s, const char *key, uint8_t &out)
{
    uint32_t tmp = 0;
    if (!findUInt_(s, key, tmp)) return false;
    out = (uint8_t)tmp;
    return true;
}
bool TrafficLightMeshModule::findStr_(const char *s, const char *key, char *out, size_t outSz)
{
    const char *p = strstr(s, key); if (!p) return false;
    p = strchr(p, ':');             if (!p) return false;
    p++;
    while (*p == ' ') p++;
    if (*p != '\"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '\"' && i < outSz - 1) out[i++] = *p++;
    out[i] = '\0';
    return (*p == '\"');
}
/* ============================================================================== */

/* ===============================[ PRIORITY / ELECTION ]=============================== */
uint8_t TrafficLightMeshModule::idxInPrioList_(uint8_t id) const
{
    for (uint8_t i = 0; i < 3; ++i) if (kPrio_[i] == id) return i;
    return 3; /* NOT FOUND */
}
bool TrafficLightMeshModule::isHigherPriority_(uint8_t a, uint8_t b) const
{
    return idxInPrioList_(a) < idxInPrioList_(b);
}
uint32_t TrafficLightMeshModule::computeBackoffMs_(uint8_t rank) const
{
    const uint32_t base   = ELECTION_BACKOFF_MIN_MS + (rank * (ELECTION_BACKOFF_MIN_MS / 2));
    const uint32_t jitter = (sem_rand32_() % (ELECTION_BACKOFF_MAX_MS - ELECTION_BACKOFF_MIN_MS + 1));
    return base + jitter;
}
void TrafficLightMeshModule::scheduleHandoverTo_(uint8_t newLeader)
{
    leaderId_   = newLeader;
    handoverAt_ = millis() + HANDOVER_DELAY_MS;
}
/* ============================================================================== */