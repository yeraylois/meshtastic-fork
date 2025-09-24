/**************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#include "TrafficRs485CoordinatorModule.h"

#if defined(ARDUINO_ARCH_ESP32)
#include <HardwareSerial.h>
#endif

/* ===============================[ PRIO TABLE DEFINITION ]=============================== */
const uint8_t TrafficRs485CoordinatorModule::kPrio_[RS485_NUM_KNOWN_NODES] = {
    RS485_PRIO0
#if (RS485_NUM_KNOWN_NODES > 1)
    ,
    RS485_PRIO1
#endif
#if (RS485_NUM_KNOWN_NODES > 2)
    ,
    RS485_PRIO2
#endif
};

/* ===================================[ LED HELPERS ]==================================== */
inline bool TrafficRs485CoordinatorModule::ledsPresent()
{
    return (LED_RED_PIN >= 0 && LED_AMBER_PIN >= 0 && LED_GREEN_PIN >= 0);
}
inline void TrafficRs485CoordinatorModule::leds(bool r, bool a, bool g)
{
    if (!ledsPresent())
        return;
    digitalWrite(LED_RED_PIN, r ? HIGH : LOW);
    digitalWrite(LED_AMBER_PIN, a ? HIGH : LOW);
    digitalWrite(LED_GREEN_PIN, g ? HIGH : LOW);
}

/* ===============================[ CASE & TOPOLOGY ]==================================== */
inline uint8_t TrafficRs485CoordinatorModule::greenNode(uint8_t c)
{
    if (c == 1)
        return 1;
    if (c == 2)
        return 0;
    return 2;
}
inline uint8_t TrafficRs485CoordinatorModule::nextCase(uint8_t curr)
{
#if (RS485_TOPOLOGY >= 3)
    /* 3-NODE: 2 -> 1 -> 3 -> 2 -> ... */
    return (curr == 2) ? 1 : ((curr == 1) ? 3 : 2);
#else
    /* 2-NODE: 2 <-> 1 */
    return (curr == 2) ? 1 : 2;
#endif
}

/* ===================================[ CTOR / INIT ]==================================== */
TrafficRs485CoordinatorModule::TrafficRs485CoordinatorModule()
    : SinglePortModule("traffic_rs485", kPort), concurrency::OSThread("TrafficRs485Coordinator")
{
}

void TrafficRs485CoordinatorModule::initOnce()
{
    if (ready_)
        return;

    pinMode(RS485_PIN_DIR, OUTPUT);
    setTx(false);

    if (ledsPresent())
    {
        pinMode(LED_RED_PIN, OUTPUT);
        pinMode(LED_AMBER_PIN, OUTPUT);
        pinMode(LED_GREEN_PIN, OUTPUT);
        leds(true, false, false); /* SAFE START -> RED */
    }

    beginUart();

    t_bit_us_ = (1000000UL / RS485_BAUD);
    t_char_us_ = t_bit_us_ * 10;

    /* INITIAL CASE */
    caseIndex_ = 2;
    inAmber_ = false;
    offNode_ = greenNode(caseIndex_);
    nextCase_ = nextCase(caseIndex_);
    tCaseStart_ = millis();
    tAmberStart_ = 0;

    /* ROLE TIMERS */
    if (isLeader_)
    {
        leaderId_ = myId_;
        leaseExpiryMs_ = millis() + RS_LEASE_MS;
        nextBeaconAt_ = millis();
        seq_ = 0;
    }
    else
    {
        leaderId_ = 0xFF;
        lastBeaconRxMs_ = 0;
        seenLeaseExpiryMs_ = 0;
        inSafety_ = false;
    }

    /* APPLY INITIAL LEDS */
    applyCaseLocal(caseIndex_);

    ready_ = true;

    LOG_INFO("init: id=%u role=%s topo=%u baud=%u\n",
             (unsigned)myId_, isLeader_ ? "LEADER" : "FOLLOWER",
             (unsigned)RS485_TOPOLOGY, (unsigned)RS485_BAUD);
}

/* ===================================[ UART / IO ]====================================== */
void TrafficRs485CoordinatorModule::beginUart()
{
#if defined(ARDUINO_ARCH_ESP32)
    Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_PIN_RX, RS485_PIN_TX);
#else
    Serial1.setPins(RS485_PIN_RX, RS485_PIN_TX);
    Serial1.begin(RS485_BAUD);
#endif
}
inline void TrafficRs485CoordinatorModule::setTx(bool en)
{
    digitalWrite(RS485_PIN_DIR, en ? HIGH : LOW);
}
uint8_t TrafficRs485CoordinatorModule::computeXOR(const uint8_t *data, size_t len)
{
    uint8_t cs = 0;
    while (len--)
        cs ^= *data++;
    return cs;
}
void TrafficRs485CoordinatorModule::sendFrame(const char *buf, size_t len)
{
    setTx(true);
    delayMicroseconds(t_bit_us_ * 2);
    Serial1.write(reinterpret_cast<const uint8_t *>(buf), len);
    Serial1.flush();
    delayMicroseconds(t_char_us_);
    setTx(false);
    delayMicroseconds(t_bit_us_ * 2);
}

/* ===================================[ LED APPLY ]====================================== */
void TrafficRs485CoordinatorModule::applyCaseLocal(uint8_t c)
{
    if (!ledsPresent())
        return;
    const uint8_t g = greenNode(c);
    if (g == myId_)
        leds(false, false, true);
    else
        leds(true, false, false);
}
void TrafficRs485CoordinatorModule::applyAmberLocal(uint8_t offNode)
{
    if (!ledsPresent())
        return;
    if (offNode == myId_)
        leds(false, true, false);
    else
        leds(true, false, false);
}
void TrafficRs485CoordinatorModule::applySafetyBlink()
{
    if (!ledsPresent())
        return;
    const bool on = ((millis() / RS_AMBER_BLINK_MS) & 1) != 0;
    leds(false, on, false);
}

/* ===================================[ RX PUMP ]======================================== */
void TrafficRs485CoordinatorModule::pumpRx()
{
    while (Serial1.available())
    {
        char c = (char)Serial1.read();
        if (c == '\n' || c == '\r')
        {
            if (rxLen_ > 0)
            {
                rxBuf_[rxLen_] = '\0';
                /* TRIM TAIL */
                while (rxLen_ && (rxBuf_[rxLen_ - 1] == '\r' || rxBuf_[rxLen_ - 1] == ' ' || rxBuf_[rxLen_ - 1] == '\t'))
                    rxBuf_[--rxLen_] = '\0';
                if (rxLen_)
                    handleLine(rxBuf_);
                rxLen_ = 0;
            }
            continue;
        }
        if (rxLen_ < RX_MAX)
            rxBuf_[rxLen_++] = c;
        else
            rxLen_ = 0; /* OVERFLOW: DROP LINE */
    }
}

/* ===================================[ CSV PARSERS ]==================================== */
bool TrafficRs485CoordinatorModule::parseCSV_u8(const char *&p, uint8_t &out)
{
    char *end = nullptr;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p)
        return false;
    out = (uint8_t)v;
    p = (*end == ',') ? end + 1 : end;
    return true;
}
bool TrafficRs485CoordinatorModule::parseCSV_u16(const char *&p, uint16_t &out)
{
    char *end = nullptr;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p)
        return false;
    out = (uint16_t)v;
    p = (*end == ',') ? end + 1 : end;
    return true;
}
bool TrafficRs485CoordinatorModule::parseCSV_u32(const char *&p, uint32_t &out)
{
    char *end = nullptr;
    unsigned long v = strtoul(p, &end, 10);
    if (end == p)
        return false;
    out = (uint32_t)v;
    p = (*end == ',') ? end + 1 : end;
    return true;
}

/* ===================================[ PROTOCOL RX ]==================================== */
/*
   FRAME FORMAT (ASCII + XOR CHECKSUM):
     "<TYPE>,<FIELDS>*<CS>\n"

   TYPES:
     B  : LEADER BEACON
          B,<leaderId>,<seq>,<case>,<am>,<off>,<leaseTtlMs>,<elapsedMs>*CS
     C  : CLAIM LEADERSHIP
          C,<id>,<rank>*CS
     Y  : YIELD
          Y,<fromId>,<toId>*CS
     A  : AMBER (OPTIONAL MANUAL CMD)
          A,<offNode>*CS
     S  : SET CASE (OPTIONAL MANUAL CMD)
          S,<case>*CS
*/
void TrafficRs485CoordinatorModule::handleLine(const char *lineZ)
{
    const char *star = strchr(lineZ, '*');
    if (!star)
        return;

    /* CHECKSUM */
    const size_t payLen = (size_t)(star - lineZ);
    uint8_t csRx = (uint8_t)strtoul(star + 1, nullptr, 16);
    if (computeXOR(reinterpret_cast<const uint8_t *>(lineZ), payLen) != csRx)
        return;

    /* TYPE */
    const char type = lineZ[0];
    if (lineZ[1] != ',')
        return;

    const char *p = lineZ + 2;

    if (type == 'B')
    {
        uint8_t lid = 0, c = 2, am = 0, off = 0;
        uint32_t seq = 0, lt = 0, pe = 0;
        if (!parseCSV_u8(p, lid))
            return;
        if (!parseCSV_u32(p, seq))
            return;
        if (!parseCSV_u8(p, c))
            return;
        if (!parseCSV_u8(p, am))
            return;
        if (!parseCSV_u8(p, off))
            return;
        if (!parseCSV_u32(p, lt))
            return;
        if (!parseCSV_u32(p, pe))
            return;

        const uint32_t now = millis();
        lastBeaconRxMs_ = now;
        seenLeaseExpiryMs_ = now + lt;
        leaderId_ = lid;

        /* ADOPT STATE */
        caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
        inAmber_ = (am != 0);
        offNode_ = off;

        /* IF I WAS CLAIMING AND BEACON FROM HIGHER RANK ARRIVES -> STOP CLAIM */
        const uint8_t rSeen = idxInPrioList_(lid);
        observedLeaderRank_ = rSeen;
        if (claiming_)
        {
            const uint8_t rMe = idxInPrioList_(myId_);
            if (rSeen < rMe)
                stopClaiming_(false);
        }

        /* IF I AM LEADER BUT HIGHER PRIORITY APPEARS -> YIELD */
        if (isLeader_ && lid != myId_)
        {
            const uint8_t rL = idxInPrioList_(lid);
            const uint8_t rM = idxInPrioList_(myId_);
            if (rL < rM)
            {
                yieldTo_(lid);
                /* APPLY REMOTE STATE IMMEDIATELY */
                if (inAmber_)
                    applyAmberLocal(offNode_);
                else
                    applyCaseLocal(caseIndex_);
            }
        }

        /* EXIT SAFETY */
        if (inSafety_)
        {
            inSafety_ = false;
            LOG_INFO("safety_exit (beacon)\n");
        }

        /* FOLLOWERS UPDATE LEDS */
        if (!isLeader_)
        {
            if (inAmber_)
                applyAmberLocal(offNode_);
            else
                applyCaseLocal(caseIndex_);
        }

        LOG_DEBUG("beacon_rx: L=%u seq=%lu c=%u am=%u off=%u lt=%lu pe=%lu\n",
                  (unsigned)lid, (unsigned long)seq, (unsigned)c, (unsigned)am,
                  (unsigned)off, (unsigned long)lt, (unsigned long)pe);
        return;
    }

    if (type == 'C')
    {
        uint8_t id = 0, rank = 0xFF;
        if (!parseCSV_u8(p, id))
            return;
        if (!parseCSV_u8(p, rank))
            return;

        /* DURING ELECTION: IF HIGHER PRIORITY CLAIM SEEN -> ABORT MY CLAIM */
        if (claiming_)
        {
            const uint8_t rMe = idxInPrioList_(myId_);
            if (rank < rMe)
                stopClaiming_(false);
        }

        /* IF I AM LEADER AND HIGHER PRIORITY CLAIM ARRIVES -> YIELD FAST */
        if (isLeader_ && id != myId_)
        {
            const uint8_t rL = rank;
            const uint8_t rM = idxInPrioList_(myId_);
            if (rL < rM)
            {
                yieldTo_(id);
            }
        }
        return;
    }

    if (type == 'Y')
    {
        /* YIELD NOTICE: Y,<from>,<to> */
        uint8_t from = 0, to = 0;
        if (!parseCSV_u8(p, from))
            return;
        if (!parseCSV_u8(p, to))
            return;
        /* NO SPECIAL ACTION REQUIRED (INFORMATIVE) */
        return;
    }

    if (type == 'A')
    {
        uint8_t off = 0;
        if (!parseCSV_u8(p, off))
            return;
        /* APPLY IF FOLLOWER */
        if (!isLeader_)
        {
            inAmber_ = true;
            offNode_ = off;
            applyAmberLocal(offNode_);
        }
        return;
    }

    if (type == 'S')
    {
        uint8_t c = 2;
        if (!parseCSV_u8(p, c))
            return;
        if (!isLeader_)
        {
            inAmber_ = false;
            caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
            applyCaseLocal(caseIndex_);
        }
        return;
    }
}

/* ===================================[ ELECTION ]======================================= */
uint8_t TrafficRs485CoordinatorModule::idxInPrioList_(uint8_t id) const
{
    for (uint8_t i = 0; i < RS485_NUM_KNOWN_NODES; ++i)
        if (kPrio_[i] == id)
            return i;
    return 0xFE; /* NOT FOUND => VERY LOW PRIORITY */
}
void TrafficRs485CoordinatorModule::scheduleElectionBackoff_()
{
    const uint8_t r = idxInPrioList_(myId_);
    const uint32_t jitter = (uint32_t)random(RS_ELECT_JITTER_MS + 1);
    electBackoffUntilMs_ = millis() + RS_ELECT_BACKOFF_BASE_MS + (uint32_t)r * RS_ELECT_BACKOFF_STEP_MS + jitter;
    observedLeaderRank_ = 0xFF;
    LOG_INFO("election: backoff until %lu ms (rank=%u)\n",
             (unsigned long)electBackoffUntilMs_, (unsigned)r);
}
void TrafficRs485CoordinatorModule::startClaiming_()
{
    claiming_ = true;
    claimUntilMs_ = millis() + RS_CLAIM_WINDOW_MS;
    /* BROADCAST CLAIM IMMEDIATELY */
    tx_Claim_(myId_, idxInPrioList_(myId_));
    LOG_INFO("election: CLAIM start (id=%u)\n", (unsigned)myId_);
}
void TrafficRs485CoordinatorModule::stopClaiming_(bool won)
{
    if (!claiming_)
        return;
    claiming_ = false;
    if (won)
    {
        becomeLeaderFromHere_();
    }
    else
    {
        /* STAY FOLLOWER & WAIT FOR BEACON */
        LOG_INFO("election: CLAIM aborted (lost)\n");
    }
}
void TrafficRs485CoordinatorModule::becomeLeaderFromHere_()
{
    isLeader_ = true;
    leaderId_ = myId_;
    leaseExpiryMs_ = millis() + RS_LEASE_MS;
    nextBeaconAt_ = 0; /* SEND ASAP */
    seq_ = 0;

    /* KEEP CURRENT CASE TIMERS; DO NOT JUMP STATE */
    if (inSafety_)
        inSafety_ = false;

    LOG_INFO("election: I AM THE LEADER NOW (id=%u)\n", (unsigned)myId_);
}
void TrafficRs485CoordinatorModule::yieldTo_(uint8_t newLeader)
{
    if (!isLeader_)
        return;
    /* SEND POLITE NOTICE */
    tx_Yield_(myId_, newLeader);

    isLeader_ = false;
    leaderId_ = newLeader;
    claiming_ = false;
    inSafety_ = true; /* UNTIL WE SEE NEW BEACON */
    LOG_INFO("handover: I yielded to leader id=%u\n", (unsigned)newLeader);
}

/* ===================================[ LEADER ]========================================= */
void TrafficRs485CoordinatorModule::leaderTick_()
{
    const uint32_t now = millis();

    /* RENEW LEASE WITH MARGIN */
    if ((int32_t)(leaseExpiryMs_ - now) <= (int32_t)RS_RENEW_BEFORE_MS)
    {
        leaseExpiryMs_ = now + RS_LEASE_MS;
        LOG_INFO("lease_renew -> expires_in=%lu ms\n", (unsigned long)(leaseExpiryMs_ - now));
    }

    /* CASE SEQUENCE: STABLE GREEN -> AMBER -> NEXT CASE */
    if (!inAmber_)
    {
        if ((int32_t)(now - tCaseStart_) >= (int32_t)RS_CASE_INTERVAL_MS)
        {
            inAmber_ = true;
            tAmberStart_ = now;
            offNode_ = greenNode(caseIndex_);
            nextCase_ = nextCase(caseIndex_);

            applyAmberLocal(offNode_);
            LOG_INFO("AMBER begin offNode=%u (from case=%u)\n",
                     (unsigned)offNode_, (unsigned)caseIndex_);
        }
    }
    else
    {
        if ((int32_t)(now - tAmberStart_) >= (int32_t)RS_AMBER_INTERVAL_MS)
        {
            inAmber_ = false;
            caseIndex_ = nextCase_;
            tCaseStart_ = now;

            applyCaseLocal(caseIndex_);
            LOG_INFO("CASE apply %u\n", (unsigned)caseIndex_);
        }
    }

    /* PERIODIC BEACON */
    if ((int32_t)(now - nextBeaconAt_) >= 0)
    {
        sendBeacon_();
        nextBeaconAt_ = now + RS_BEACON_PERIOD_MS;
    }
}
void TrafficRs485CoordinatorModule::sendBeacon_()
{
    const uint32_t now = millis();
    const uint32_t elapsed = (!inAmber_) ? (now - tCaseStart_) : (now - tAmberStart_);
    const uint32_t leaseTt = (leaseExpiryMs_ > now) ? (leaseExpiryMs_ - now) : 0;

    tx_Beacon_(myId_, seq_, caseIndex_, (uint8_t)(inAmber_ ? 1 : 0), offNode_, leaseTt, elapsed);
    seq_++;
}
void TrafficRs485CoordinatorModule::tx_Beacon_(uint8_t leaderId, uint32_t seq, uint8_t c, uint8_t am, uint8_t off,
                                               uint32_t leaseTtlMs, uint32_t elapsedMs)
{
    char p[96];
    int L = snprintf(p, sizeof(p), "B,%u,%lu,%u,%u,%u,%lu,%lu",
                     (unsigned)leaderId, (unsigned long)seq,
                     (unsigned)c, (unsigned)am, (unsigned)off,
                     (unsigned long)leaseTtlMs, (unsigned long)elapsedMs);
    if (L < 0)
        return;
    uint8_t cs = computeXOR(reinterpret_cast<const uint8_t *>(p), (size_t)L);

    char f[112];
    int F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
    if (F > 0)
        sendFrame(f, (size_t)F);

    LOG_DEBUG("beacon_tx: %s\n", p);
}
void TrafficRs485CoordinatorModule::tx_Claim_(uint8_t id, uint8_t rank)
{
    char p[32];
    int L = snprintf(p, sizeof(p), "C,%u,%u", (unsigned)id, (unsigned)rank);
    if (L < 0)
        return;
    uint8_t cs = computeXOR(reinterpret_cast<const uint8_t *>(p), (size_t)L);

    char f[48];
    int F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
    if (F > 0)
        sendFrame(f, (size_t)F);
}
void TrafficRs485CoordinatorModule::tx_Yield_(uint8_t fromId, uint8_t toId)
{
    char p[32];
    int L = snprintf(p, sizeof(p), "Y,%u,%u", (unsigned)fromId, (unsigned)toId);
    if (L < 0)
        return;
    uint8_t cs = computeXOR(reinterpret_cast<const uint8_t *>(p), (size_t)L);

    char f[48];
    int F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
    if (F > 0)
        sendFrame(f, (size_t)F);
}

/* ===================================[ FOLLOWER ]======================================= */
void TrafficRs485CoordinatorModule::followerTick_()
{
    const uint32_t now = millis();

    /* LONG BEACON LOSS => START/RESTART ELECTION */
    if (!inSafety_ && lastBeaconRxMs_ != 0 &&
        (int32_t)(now - lastBeaconRxMs_) > (int32_t)RS_LOSS_TIMEOUT_MS)
    {
        inSafety_ = true;
        scheduleElectionBackoff_();
        LOG_WARN("safety_enter (no beacon > %u ms)\n", (unsigned)RS_LOSS_TIMEOUT_MS);
    }

    /* SAFETY BLINK WHILE NO LEADER SEEN */
    if (inSafety_)
    {
        applySafetyBlink();

        /* IF IN BACKOFF WINDOW, DO NOTHING UNTIL TIMEOUT */
        if (electBackoffUntilMs_ && (int32_t)(now - electBackoffUntilMs_) < 0)
        {
            return;
        }

        /* IF BACKOFF PASSED AND STILL NO BEACON: CLAIM */
        if (!claiming_ && (electBackoffUntilMs_ != 0) &&
            (int32_t)(now - electBackoffUntilMs_) >= 0)
        {
            startClaiming_();
        }

        /* CLAIMING WINDOW: KEEP ADVERTISING AND RESOLVE */
        if (claiming_)
        {
            /* RE-BROADCAST CLAIM EVERY ~1/3 OF WINDOW TO IMPROVE RELIABILITY */
            static uint32_t tLastClaim = 0;
            if ((int32_t)(now - tLastClaim) > (int32_t)(RS_CLAIM_WINDOW_MS / 3))
            {
                tx_Claim_(myId_, idxInPrioList_(myId_));
                tLastClaim = now;
            }

            /* IF CLAIM WINDOW EXPIRES AND NO BETTER LEADER SEEN => WE WIN */
            if ((int32_t)(now - claimUntilMs_) >= 0)
            {
                const uint8_t rMe = idxInPrioList_(myId_);
                if (observedLeaderRank_ == 0xFF || rMe <= observedLeaderRank_)
                {
                    stopClaiming_(true);
                }
                else
                {
                    stopClaiming_(false);
                }
            }
        }

        return; /* WHILE IN SAFETY/ELECTION, DO NOT APPLY CASE/AMBER FROM OLD STATE */
    }

    /* NORMAL FOLLOW MODE: APPLY LEDS FROM LAST BEACON */
    if (inAmber_)
        applyAmberLocal(offNode_);
    else
        applyCaseLocal(caseIndex_);

    LOG_DEBUG("local lights: safety=%d, amber=%d, case=%u, offNode=%u leader=%u isLeader=%d\n",
              (int)inSafety_, (int)inAmber_, (unsigned)caseIndex_, (unsigned)offNode_,
              (unsigned)leaderId_, (int)isLeader_);
}

/* ===================================[ LOOP ]=========================================== */
int32_t TrafficRs485CoordinatorModule::runOnce()
{
    if (!ready_)
        initOnce();

    pumpRx();

    if (isLeader_)
        leaderTick_();
    else
        followerTick_();

    return 25; /* MS */
}