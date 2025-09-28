/*************************************************************
 *   Project : Blackout Traffic Light System                  *
 *   Module  : Traffic RS485 Coordinator (Leader+Follower)    *
 *   Author  : Yeray Lois Sanchez                             *
 *   Email   : yerayloissanchez@gmail.com                     *
 **************************************************************/

#include "TrafficRs485CoordinatorModule.h"

#include "configuration.h"

#if defined(ARDUINO_ARCH_ESP32)
  #include <HardwareSerial.h>
#endif

/* =====================================[ LOG TAG ]===================================== */
#ifndef LOG_TAG
  #define LOG_TAG "TrafficRS485"
#endif

/* ===============================[ PRIORITY TABLE DEFINITION ]========================== */
const uint8_t TrafficRs485CoordinatorModule::kPrio_[RS485_NUM_KNOWN_NODES] = {RS485_PRIO0
#if (RS485_NUM_KNOWN_NODES > 1)
                                                                              ,
                                                                              RS485_PRIO1
#endif
#if (RS485_NUM_KNOWN_NODES > 2)
                                                                              ,
                                                                              RS485_PRIO2
#endif
};

/* ===================================[ CASE & TOPOLOGY ]================================ */
/**
 * NODE THAT IS 'green' IN EACH CASE:
 *  CASE 1 -> NODE 1
 *  CASE 2 -> NODE 0
 *  CASE 3 -> NODE 2
 */
inline uint8_t TrafficRs485CoordinatorModule::greenNode_(uint8_t c) {
  if (c == 1)
    return 1;
  if (c == 2)
    return 0;
  return 2;
}

/**
 * NEXT CASE IN TOPOLOGY (3 NODES: 1-2-3; 2 NODES: 1-2-1-2...)
 *  3-NODE: 1->2, 2->3, 3->1
 *  2-NODE: 1->2, 2->1
 * (HARMLESS IN 2-NODE SETUPS)
 */
inline uint8_t TrafficRs485CoordinatorModule::nextCaseForTopology_(uint8_t curr) {
#if (RS485_TOPOLOGY >= 3)
  if (curr == 2)
    return 3;
  if (curr == 3)
    return 1;
  return 2;
#else
  return (curr == 2) ? 3 : 2;
#endif
}

/* =====================================[ CTOR ]========================================= */
TrafficRs485CoordinatorModule::TrafficRs485CoordinatorModule()
    : SinglePortModule("traffic_rs485", kPort), concurrency::OSThread("TrafficRs485Coordinator") {}

/* ===================================[ UART / RS485 IO ]================================ */
void TrafficRs485CoordinatorModule::beginUart() {
#if defined(ARDUINO_ARCH_ESP32)
  Serial1.begin(RS485_BAUD, SERIAL_8N1, RS485_PIN_RX, RS485_PIN_TX);
#else
  Serial1.setPins(RS485_PIN_RX, RS485_PIN_TX);
  Serial1.begin(RS485_BAUD);
#endif
}
inline void TrafficRs485CoordinatorModule::setTx(bool en) {
  digitalWrite(RS485_PIN_DIR, en ? HIGH : LOW);
}

/* ===================================[ STATUS LEDS ]==================================== */
inline bool TrafficRs485CoordinatorModule::ledsPresent() {
  return (RS485_LED_RED_PIN >= 0 && RS485_LED_AMBER_PIN >= 0 && RS485_LED_GREEN_PIN >= 0);
}
inline void TrafficRs485CoordinatorModule::leds(bool r, bool a, bool g) {
  if (RS485_LED_RED_PIN >= 0)
    digitalWrite(RS485_LED_RED_PIN, r ? HIGH : LOW);
  if (RS485_LED_AMBER_PIN >= 0)
    digitalWrite(RS485_LED_AMBER_PIN, a ? HIGH : LOW);
  if (RS485_LED_GREEN_PIN >= 0)
    digitalWrite(RS485_LED_GREEN_PIN, g ? HIGH : LOW);
}

/* ===================================[ RS485 FRAMING ]================================== */
uint8_t TrafficRs485CoordinatorModule::computeXOR(const uint8_t* data, size_t len) {
  uint8_t cs = 0;
  while (len--)
    cs ^= *data++;
  return cs;
}
void TrafficRs485CoordinatorModule::sendFrame(const char* buf, size_t len) {
  setTx(true);
  delayMicroseconds(t_bit_us_ * 2);
  Serial1.write(reinterpret_cast<const uint8_t*>(buf), len);
  Serial1.flush();
  delayMicroseconds(t_char_us_);
  setTx(false);
  delayMicroseconds(t_bit_us_ * 2);
}

/* ===================================[ SIGNAL SETUP ]=================================== */
void TrafficRs485CoordinatorModule::setupSignals_() {
  /* VEHICLE HEAD PINMAPS */
  vR_[VM_S2N] = RS485_V_S2N_R_PIN;
  vA_[VM_S2N] = RS485_V_S2N_A_PIN;
  vG_[VM_S2N] = RS485_V_S2N_G_PIN;
  vR_[VM_S2W] = RS485_V_S2W_R_PIN;
  vA_[VM_S2W] = RS485_V_S2W_A_PIN;
  vG_[VM_S2W] = RS485_V_S2W_G_PIN;
  vR_[VM_N2S] = RS485_V_N2S_R_PIN;
  vA_[VM_N2S] = RS485_V_N2S_A_PIN;
  vG_[VM_N2S] = RS485_V_N2S_G_PIN;
  vR_[VM_N2W] = RS485_V_N2W_R_PIN;
  vA_[VM_N2W] = RS485_V_N2W_A_PIN;
  vG_[VM_N2W] = RS485_V_N2W_G_PIN;
  vR_[VM_W2N] = RS485_V_W2N_R_PIN;
  vA_[VM_W2N] = RS485_V_W2N_A_PIN;
  vG_[VM_W2N] = RS485_V_W2N_G_PIN;
  vR_[VM_W2S] = RS485_V_W2S_R_PIN;
  vA_[VM_W2S] = RS485_V_W2S_A_PIN;
  vG_[VM_W2S] = RS485_V_W2S_G_PIN;

  /* PEDESTRIAN PINMAPS */
  pR_[PX_N1] = RS485_P_N1_R_PIN;
  pG_[PX_N1] = RS485_P_N1_G_PIN;
  pR_[PX_S1] = RS485_P_S1_R_PIN;
  pG_[PX_S1] = RS485_P_S1_G_PIN;
  pR_[PX_W2] = RS485_P_W2_R_PIN;
  pG_[PX_W2] = RS485_P_W2_G_PIN;
  pR_[PX_S2] = RS485_P_S2_R_PIN;
  pG_[PX_S2] = RS485_P_S2_G_PIN;
  pR_[PX_N2] = RS485_P_N2_R_PIN;
  pG_[PX_N2] = RS485_P_N2_G_PIN;
  pR_[PX_W1] = RS485_P_W1_R_PIN;
  pG_[PX_W1] = RS485_P_W1_G_PIN;

  /* PINMODES + DEFAULTS */
  for (uint8_t i = 0; i < VM_COUNT; ++i) {
    if (vR_[i] >= 0)
      pinMode(vR_[i], OUTPUT);
    if (vA_[i] >= 0)
      pinMode(vA_[i], OUTPUT);
    if (vG_[i] >= 0)
      pinMode(vG_[i], OUTPUT);
    vState_[i] = L_RED;
  }
  for (uint8_t i = 0; i < PX_COUNT; ++i) {
    if (pR_[i] >= 0)
      pinMode(pR_[i], OUTPUT);
    if (pG_[i] >= 0)
      pinMode(pG_[i], OUTPUT);
    pGreen_[i] = false;
  }

  /* FORCE ALL-RED INIT */
  for (uint8_t i = 0; i < VM_COUNT; ++i)
    setVehPins_(i, true, false, false);
  for (uint8_t i = 0; i < PX_COUNT; ++i)
    setPedPins_(i, false);
}

/* ===================================[ APPLY HELPERS ]================================== */
inline void TrafficRs485CoordinatorModule::setVehPins_(uint8_t idx, bool r, bool a, bool g) {
  if (vR_[idx] >= 0)
    digitalWrite(vR_[idx], r ? HIGH : LOW);
  if (vA_[idx] >= 0)
    digitalWrite(vA_[idx], a ? HIGH : LOW);
  if (vG_[idx] >= 0)
    digitalWrite(vG_[idx], g ? HIGH : LOW);
}
inline void TrafficRs485CoordinatorModule::setPedPins_(uint8_t idx, bool g) {
  if (pG_[idx] >= 0)
    digitalWrite(pG_[idx], g ? HIGH : LOW);
  if (pR_[idx] >= 0)
    digitalWrite(pR_[idx], g ? LOW : HIGH);
}
void TrafficRs485CoordinatorModule::driveOutputs_() {
  const bool blink = ((millis() / RS485_AMBER_BLINK_MS) & 1) != 0;

  /* VEHICLE STATES */
  for (uint8_t i = 0; i < VM_COUNT; ++i) {
    switch (vState_[i]) {
      case L_RED:
        setVehPins_(i, true, false, false);
        break;
      case L_GREEN:
        setVehPins_(i, false, false, true);
        break;
      case L_AMBER_FIXED:
        setVehPins_(i, false, true, false);
        break;
      case L_AMBER_FLASH:
        setVehPins_(i, false, blink, false);
        break;
    }
  }
  /* PEDESTRIAN STATES */
  for (uint8_t i = 0; i < PX_COUNT; ++i)
    setPedPins_(i, pGreen_[i]);
}

/* ===============================[ INTERSECTION CASE TABLE ]============================= */
void TrafficRs485CoordinatorModule::applyIntersectionCase_(uint8_t c) {
  for (uint8_t i = 0; i < VM_COUNT; ++i)
    vState_[i] = L_RED;
  for (uint8_t i = 0; i < PX_COUNT; ++i)
    pGreen_[i] = false;

  switch (c) {
    case 1:
      /* SOUTH GOES */
      vState_[VM_S2N] = L_GREEN;
      vState_[VM_S2W] = L_GREEN;
      vState_[VM_N2S] = L_RED;
      vState_[VM_N2W] = L_AMBER_FLASH;
      vState_[VM_W2S] = L_AMBER_FLASH;
      vState_[VM_W2N] = L_RED;

      pGreen_[PX_N1] = true;
      pGreen_[PX_S1] = true;
      break;

    case 2:
      /* NORTH GOES */
      vState_[VM_S2N] = L_RED;
      vState_[VM_S2W] = L_RED;
      vState_[VM_N2S] = L_GREEN;
      vState_[VM_N2W] = L_GREEN;
      vState_[VM_W2N] = L_RED;
      vState_[VM_W2S] = L_AMBER_FLASH;

      pGreen_[PX_W2] = true;
      pGreen_[PX_S2] = true;
      pGreen_[PX_N2] = true;
      break;

    default:
      /* WEST GOES */
      vState_[VM_S2N] = L_RED;
      vState_[VM_S2W] = L_RED;
      vState_[VM_N2S] = L_RED;
      vState_[VM_N2W] = L_AMBER_FLASH;
      vState_[VM_W2N] = L_GREEN;
      vState_[VM_W2S] = L_GREEN;

      pGreen_[PX_W1] = true;
      pGreen_[PX_S2] = true;
      break;
  }

  driveOutputs_(); /* APPLY IMMEDIATELY */
}

/* ---------------------- AMBER TRANSITION & ALL-RED CLEARANCE ---------------------- */
void TrafficRs485CoordinatorModule::applyAllRed_() {
  for (uint8_t i = 0; i < VM_COUNT; ++i)
    vState_[i] = L_RED;
  for (uint8_t i = 0; i < PX_COUNT; ++i)
    pGreen_[i] = false;
  driveOutputs_();
}

void TrafficRs485CoordinatorModule::applyAmberTransitionForIntersection_() {
  for (uint8_t i = 0; i < VM_COUNT; ++i)
    vState_[i] = L_RED;
  for (uint8_t i = 0; i < PX_COUNT; ++i)
    pGreen_[i] = false;

  /* SET AMBER ON THE MOVEMENTS THAT WERE GREEN IN THE CURRENT CASE */
  switch (caseIndex_) {
    case 1:
      vState_[VM_S2N] = L_AMBER_FIXED;
      vState_[VM_S2W] = L_AMBER_FIXED;
      break;
    case 2:
      vState_[VM_N2S] = L_AMBER_FIXED;
      vState_[VM_N2W] = L_AMBER_FIXED;
      break;
    default:
      vState_[VM_W2N] = L_AMBER_FIXED;
      vState_[VM_W2S] = L_AMBER_FIXED;
      break;
  }
  driveOutputs_();
}

/* ===================================[ INIT ONCE ]====================================== */
void TrafficRs485CoordinatorModule::initOnce() {
  if (ready_)
    return;

  /* RS485 DIR PIN */
  pinMode(RS485_PIN_DIR, OUTPUT);
  setTx(false);

  /* OPTIONAL STATUS LEDS */
  if (ledsPresent()) {
    pinMode(RS485_LED_RED_PIN, OUTPUT);
    pinMode(RS485_LED_AMBER_PIN, OUTPUT);
    pinMode(RS485_LED_GREEN_PIN, OUTPUT);
    leds(true, false, false); /* SAFE START -> RED */
  }

  /* UART */
  beginUart();

  /* UART TIMINGS */
  t_bit_us_  = (1000000UL / RS485_BAUD);
  t_char_us_ = t_bit_us_ * 10;

  /* SIGNAL ARRAYS */
  setupSignals_();

  /* INITIAL PHASE */
  caseIndex_    = 2;
  nextCase_     = nextCaseForTopology_(caseIndex_);
  inAmber_      = false;
  inAllRed_     = false;
  offNode_      = greenNode_(caseIndex_);
  tCaseStart_   = millis();
  tAmberStart_  = 0;
  tAllRedStart_ = 0;

  /* ROLE TIMERS */
  if (isLeader_) {
    leaderId_      = myId_;
    leaseExpiryMs_ = millis() + RS485_LEASE_MS;
    nextBeaconAt_  = millis();
    seq_           = 0;
  } else {
    leaderId_          = 0xFF;
    lastBeaconRxMs_    = 0;
    seenLeaseExpiryMs_ = 0;

    /* START IN SAFETY */
    inSafety_ = true;
    scheduleElectionBackoff_();

    /** STARTUP LOWER-ID OBSERVER */
    seenLowerId_            = false;
    startupLowerDeadlineMs_ = millis() + RS485_STARTUP_WAIT_LOWER_MS;

    LOG_WARN("safety_enter (startup)\n");
  }

  /* APPLY INITIAL INTERSECTION */
  applyIntersectionCase_(caseIndex_);

  ready_ = true;

  LOG_INFO("INIT: id=%u role=%s topo=%u baud=%u\n",
           (unsigned) myId_,
           isLeader_ ? "LEADER" : "FOLLOWER",
           (unsigned) RS485_TOPOLOGY,
           (unsigned) RS485_BAUD);
}

/* ===================================[ RX PUMP ]======================================== */
void TrafficRs485CoordinatorModule::pumpRx() {
  /* COMPACT, ROBUST LINE READER. */
  while (Serial1.available()) {
    char c = (char) Serial1.read();
    if (c == '\n' || c == '\r') {
      if (rxLen_ > 0) {
        rxBuf_[rxLen_] = '\0';
        while (rxLen_
               && (rxBuf_[rxLen_ - 1] == '\r' || rxBuf_[rxLen_ - 1] == ' '
                   || rxBuf_[rxLen_ - 1] == '\t'))
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
      rxLen_ = 0; /* OVERFLOW -> DROP LINE */
  }
}

/* ===================================[ CSV UTILS ]====================================== */
bool TrafficRs485CoordinatorModule::parseCSV_u8(const char*& p, uint8_t& out) {
  char*         end = nullptr;
  unsigned long v   = strtoul(p, &end, 10);
  if (end == p)
    return false;
  out = (uint8_t) v;
  p   = (*end == ',') ? end + 1 : end;
  return true;
}
bool TrafficRs485CoordinatorModule::parseCSV_u16(const char*& p, uint16_t& out) {
  char*         end = nullptr;
  unsigned long v   = strtoul(p, &end, 10);
  if (end == p)
    return false;
  out = (uint16_t) v;
  p   = (*end == ',') ? end + 1 : end;
  return true;
}
bool TrafficRs485CoordinatorModule::parseCSV_u32(const char*& p, uint32_t& out) {
  char*         end = nullptr;
  unsigned long v   = strtoul(p, &end, 10);
  if (end == p)
    return false;
  out = (uint32_t) v;
  p   = (*end == ',') ? end + 1 : end;
  return true;
}

/* =====================[ STARTUP LOWER-ID OBSERVER ]===================== */
inline void TrafficRs485CoordinatorModule::observeRemoteId_(uint8_t remoteId) {
  if (remoteId < myId_)
    seenLowerId_ = true;
}

/* ===================================[ PROTOCOL RX ]==================================== */
/*
 * FRAME: "<TYPE>,<FIELDS>*<CS>\n"  (ASCII + XOR CHECKSUM)
 *
 * TYPES:
 *   B : LEADER BEACON
 *       B,<leaderId>,<seq>,<case>,<am>,<off>,<leaseTtlMs>,<elapsedMs>*CS
 *         am: 0=STABLE, 1=AMBER, 2=ALL_RED
 *   C : CLAIM LEADERSHIP -> C,<id>,<rank>*CS
 *   Y : YIELD NOTICE     -> Y,<fromId>,<toId>*CS
 *   A : MANUAL AMBER     -> A,<offNode>*CS
 *   S : MANUAL CASE      -> S,<case>*CS
 */
void TrafficRs485CoordinatorModule::handleLine(const char* lineZ) {
  const char* star = strchr(lineZ, '*');
  if (!star)
    return;

  const size_t payLen = (size_t) (star - lineZ);
  uint8_t      csRx   = (uint8_t) strtoul(star + 1, nullptr, 16);
  if (computeXOR(reinterpret_cast<const uint8_t*>(lineZ), payLen) != csRx)
    return;

  if (lineZ[1] != ',')
    return;
  const char  type = lineZ[0];
  const char* p    = lineZ + 2;

  if (type == 'B') {
    uint8_t  lid = 0, c = 2, am = 0, off = 0;
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
    lastBeaconRxMs_    = now;
    seenLeaseExpiryMs_ = now + lt;
    leaderId_          = lid;

    observeRemoteId_(lid); /* LOWER-ID OBSERVER */

    /* ADOPT REMOTE PHASE */
    caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
    inAmber_   = (am == 1);
    inAllRed_  = (am == 2);
    offNode_   = off;

    /* ELECTION INTERACTION */
    const uint8_t rSeen = idxInPrioList_(lid);
    observedLeaderRank_ = rSeen;
    if (claiming_) {
      const uint8_t rMe = idxInPrioList_(myId_);
      if (rSeen < rMe)
        stopClaiming_(false);
    }

    /* YIELD IF SEE HIGHER PRIORITY */
    if (isLeader_ && lid != myId_) {
      const uint8_t rM = idxInPrioList_(myId_);
      if (rSeen < rM) {
        yieldTo_(lid);
      }
    }

    /* EXIT SAFETY */
    if (inSafety_) {
      inSafety_ = false;
      LOG_INFO("safety_exit (beacon)\n");
    }

    /* APPLY PHASE LOCALLY (FOLLOWER) */
    if (!isLeader_) {
      if (inAllRed_) {
        applyAllRed_();
      } else if (inAmber_) {
        applyAmberTransitionForIntersection_();
        applyAmberLocal(offNode_);
      } else {
        applyIntersectionCase_(caseIndex_);
      }
    }

    LOG_DEBUG("beacon_rx: L=%u seq=%lu c=%u am=%u off=%u lt=%lu pe=%lu\n",
              (unsigned) lid,
              (unsigned long) seq,
              (unsigned) c,
              (unsigned) am,
              (unsigned) off,
              (unsigned long) lt,
              (unsigned long) pe);
    return;
  }

  if (type == 'C') {
    uint8_t id = 0, rank = 0xFF;
    if (!parseCSV_u8(p, id))
      return;
    if (!parseCSV_u8(p, rank))
      return;

    observeRemoteId_(id); /* LOWER-ID OBSERVER */

    if (claiming_) {
      const uint8_t rMe = idxInPrioList_(myId_);
      if (rank < rMe)
        stopClaiming_(false);
    }

    if (isLeader_ && id != myId_) {
      const uint8_t rM = idxInPrioList_(myId_);
      if (rank < rM) {
        yieldTo_(id);
      }
    }
    return;
  }

  if (type == 'Y') {
    /* INFORMATIONAL ONLY */
    return;
  }

  if (type == 'A') {
    uint8_t off = 0;
    if (!parseCSV_u8(p, off))
      return;
    if (!isLeader_) {
      inAmber_  = true;
      inAllRed_ = false;
      offNode_  = off;
      applyAmberTransitionForIntersection_();
      applyAmberLocal(offNode_);
    }
    return;
  }

  if (type == 'S') {
    uint8_t c = 2;
    if (!parseCSV_u8(p, c))
      return;
    if (!isLeader_) {
      inAmber_   = false;
      inAllRed_  = false;
      caseIndex_ = (c >= 1 && c <= 3) ? c : 2;
      applyIntersectionCase_(caseIndex_);
    }
    return;
  }
}

/* ===================================[ ELECTION ]======================================= */
uint8_t TrafficRs485CoordinatorModule::idxInPrioList_(uint8_t id) const {
  for (uint8_t i = 0; i < RS485_NUM_KNOWN_NODES; ++i)
    if (kPrio_[i] == id)
      return i;
  return 0xFE; /* NOT FOUND => VERY LOW PRIORITY */
}
void TrafficRs485CoordinatorModule::scheduleElectionBackoff_() {
  const uint8_t  r      = idxInPrioList_(myId_);
  const uint32_t jitter = (uint32_t) random(RS485_ELECT_JITTER_MS + 1);
  electBackoffUntilMs_ =
      millis() + RS485_ELECT_BACKOFF_BASE_MS + (uint32_t) r * RS485_ELECT_BACKOFF_STEP_MS + jitter;
  observedLeaderRank_ = 0xFF;
  LOG_INFO("election: backoff until %lu ms (rank=%u)\n",
           (unsigned long) electBackoffUntilMs_,
           (unsigned) r);
}
void TrafficRs485CoordinatorModule::startClaiming_() {
  if (claiming_)
    return;
  claiming_     = true;
  claimUntilMs_ = millis() + RS485_CLAIM_WINDOW_MS;
  tx_Claim_(myId_, idxInPrioList_(myId_));
  LOG_INFO("election: CLAIM start (id=%u)\n", (unsigned) myId_);
}
void TrafficRs485CoordinatorModule::stopClaiming_(bool won) {
  if (!claiming_)
    return;
  claiming_ = false;
  if (won) {
    becomeLeaderFromHere_();
  } else {
    LOG_INFO("election: CLAIM aborted (lost)\n");
  }
}
void TrafficRs485CoordinatorModule::becomeLeaderFromHere_() {
  isLeader_      = true;
  leaderId_      = myId_;
  leaseExpiryMs_ = millis() + RS485_LEASE_MS;
  nextBeaconAt_  = 0; /* SEND ASAP */
  seq_           = 0;

  if (inSafety_)
    inSafety_ = false;

  LOG_INFO("election: I AM THE LEADER NOW (id=%u)\n", (unsigned) myId_);
}
void TrafficRs485CoordinatorModule::yieldTo_(uint8_t newLeader) {
  if (!isLeader_)
    return;

  tx_Yield_(myId_, newLeader);

  isLeader_ = false;
  leaderId_ = newLeader;
  claiming_ = false;
  inSafety_ = true; /* WAIT UNTIL NEW BEACON ARRIVES */
  LOG_INFO("handover: I yielded to leader id=%u\n", (unsigned) newLeader);
}

/* ===================================[ LEADER SIDE ]==================================== */
void TrafficRs485CoordinatorModule::leaderTick_() {
  const uint32_t now = millis();

  /* LEASE RENEWAL WITH MARGIN */
  if ((int32_t) (leaseExpiryMs_ - now) <= (int32_t) RS485_RENEW_BEFORE_MS) {
    leaseExpiryMs_ = now + RS485_LEASE_MS;
    LOG_INFO("lease_renew -> expires_in=%lu ms\n", (unsigned long) (leaseExpiryMs_ - now));
  }

  /* SEQUENCE: STABLE -> AMBER -> ALL_RED -> NEXT CASE (SYNC BEACONS AT EACH EDGE) */
  if (!inAmber_ && !inAllRed_) {
    if ((int32_t) (now - tCaseStart_) >= (int32_t) RS485_CASE_INTERVAL_MS) {
      inAmber_     = true;
      tAmberStart_ = now;
      offNode_     = greenNode_(caseIndex_);
      nextCase_    = nextCaseForTopology_(caseIndex_);

      applyAmberTransitionForIntersection_(); /* REAL AMBER ON HEADS */
      applyAmberLocal(offNode_);              /* STATUS LED */

      sendBeacon_(); /* am=1 */
      nextBeaconAt_ = now + RS485_BEACON_PERIOD_MS;

      LOG_INFO(
          "AMBER begin offNode=%u (from case=%u)\n", (unsigned) offNode_, (unsigned) caseIndex_);
    }
  } else if (inAmber_ && !inAllRed_) {
    if ((int32_t) (now - tAmberStart_) >= (int32_t) RS485_AMBER_INTERVAL_MS) {
      inAmber_      = false;
      inAllRed_     = true;
      tAllRedStart_ = now;

      applyAllRed_();

      sendBeacon_(); /* am=2 */
      nextBeaconAt_ = now + RS485_BEACON_PERIOD_MS;
    }
  } else { /* inAllRed_ */
    if ((int32_t) (now - tAllRedStart_) >= (int32_t) RS485_ALL_RED_MS) {
      inAllRed_   = false;
      caseIndex_  = nextCase_;
      tCaseStart_ = now;

      applyCaseLocal(caseIndex_);
      applyIntersectionCase_(caseIndex_);

      sendBeacon_(); /* am=0, c=new */
      nextBeaconAt_ = now + RS485_BEACON_PERIOD_MS;

      LOG_INFO("CASE apply %u\n", (unsigned) caseIndex_);
    }
  }

  /* PERIODIC BEACON (REDUNDANCY) */
  if ((int32_t) (now - nextBeaconAt_) >= 0) {
    sendBeacon_();
    nextBeaconAt_ = now + RS485_BEACON_PERIOD_MS;
  }
}
void TrafficRs485CoordinatorModule::sendBeacon_() {
  const uint32_t now     = millis();
  const uint32_t elapsed = (!inAmber_ && !inAllRed_)
                               ? (now - tCaseStart_)
                               : (inAmber_ ? (now - tAmberStart_) : (now - tAllRedStart_));
  const uint32_t leaseTt = (leaseExpiryMs_ > now) ? (leaseExpiryMs_ - now) : 0;
  const uint8_t  amField = inAllRed_ ? 2 : (inAmber_ ? 1 : 0);

  tx_Beacon_(myId_, seq_, caseIndex_, amField, offNode_, leaseTt, elapsed);
  seq_++;
}
void TrafficRs485CoordinatorModule::tx_Beacon_(uint8_t  leaderId,
                                               uint32_t seq,
                                               uint8_t  c,
                                               uint8_t  am,
                                               uint8_t  off,
                                               uint32_t leaseTtlMs,
                                               uint32_t elapsedMs) {
  char p[96];
  int  L = snprintf(p,
                   sizeof(p),
                   "B,%u,%lu,%u,%u,%u,%lu,%lu",
                   (unsigned) leaderId,
                   (unsigned long) seq,
                   (unsigned) c,
                   (unsigned) am,
                   (unsigned) off,
                   (unsigned long) leaseTtlMs,
                   (unsigned long) elapsedMs);
  if (L < 0)
    return;
  uint8_t cs = computeXOR(reinterpret_cast<const uint8_t*>(p), (size_t) L);

  char f[112];
  int  F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
  if (F > 0)
    sendFrame(f, (size_t) F);

  LOG_DEBUG("beacon_tx: %s\n", p);
}
void TrafficRs485CoordinatorModule::tx_Claim_(uint8_t id, uint8_t rank) {
  char p[32];
  int  L = snprintf(p, sizeof(p), "C,%u,%u", (unsigned) id, (unsigned) rank);
  if (L < 0)
    return;
  uint8_t cs = computeXOR(reinterpret_cast<const uint8_t*>(p), (size_t) L);

  char f[48];
  int  F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
  if (F > 0)
    sendFrame(f, (size_t) F);
}
void TrafficRs485CoordinatorModule::tx_Yield_(uint8_t fromId, uint8_t toId) {
  char p[32];
  int  L = snprintf(p, sizeof(p), "Y,%u,%u", (unsigned) fromId, (unsigned) toId);
  if (L < 0)
    return;
  uint8_t cs = computeXOR(reinterpret_cast<const uint8_t*>(p), (size_t) L);

  char f[48];
  int  F = snprintf(f, sizeof(f), "%s*%02X\n", p, cs);
  if (F > 0)
    sendFrame(f, (size_t) F);
}

/* ===================================[ FOLLOWER SIDE ]================================== */
void TrafficRs485CoordinatorModule::followerTick_() {
  const uint32_t now = millis();

  /* BEACON LOSS -> SAFETY + ELECTION */
  if (!inSafety_ && lastBeaconRxMs_ != 0
      && (int32_t) (now - lastBeaconRxMs_) > (int32_t) RS485_LOSS_TIMEOUT_MS) {
    inSafety_ = true;
    scheduleElectionBackoff_();
    LOG_WARN("safety_enter (no beacon > %u ms)\n", (unsigned) RS485_LOSS_TIMEOUT_MS);
  }

  if (inSafety_) {
    applySafetyBlink();

    /** STARTUP LOWER-ID POLICY:
     *
     * IF WE HAVE NOT HEARD ANY LOWER-ID NODE BY THE DEADLINE, WE START
     * CLAIMING IMMEDIATELY (ASSUME WE ARE HIGHEST AMONG PRESENT NODES).
     */
    if (!claiming_ && startupLowerDeadlineMs_ != 0
        && (int32_t) (now - startupLowerDeadlineMs_) >= 0) {
      if (!seenLowerId_) {
        LOG_INFO("startup_lower_id: no lower-ID heard -> start claiming now\n");
        startClaiming_();
        startupLowerDeadlineMs_ = 0;
      } else {
        startupLowerDeadlineMs_ = 0;
      }
    }

    /* NORMAL BACKOFF WAIT */
    if (electBackoffUntilMs_ && (int32_t) (now - electBackoffUntilMs_) < 0) {
      return;
    }

    /* START CLAIM IF STILL NO BEACON AND NOT TRIGGERED ABOVE */
    if (!claiming_ && (electBackoffUntilMs_ != 0) && (int32_t) (now - electBackoffUntilMs_) >= 0) {
      startClaiming_();
    }

    /* CLAIMING WINDOW: RE-ADVERTISE + RESOLVE */
    if (claiming_) {
      static uint32_t tLastClaim = 0;
      if ((int32_t) (now - tLastClaim) > (int32_t) (RS485_CLAIM_WINDOW_MS / 3)) {
        tx_Claim_(myId_, idxInPrioList_(myId_));
        tLastClaim = now;
      }
      if ((int32_t) (now - claimUntilMs_) >= 0) {
        const uint8_t rMe = idxInPrioList_(myId_);
        if (observedLeaderRank_ == 0xFF || rMe <= observedLeaderRank_)
          stopClaiming_(true);
        else
          stopClaiming_(false);
      }
    }
    return; /* DO NOT APPLY OLD CASE WHILE IN SAFETY/ELECTION */
  }

  /* NORMAL FOLLOWER: KEEP OUTPUTS REFRESHED (BLINKS) */
  driveOutputs_();

  LOG_DEBUG(
      "local lights: safety=%d, amber=%d, allred=%d, case=%u, offNode=%u leader=%u isLeader=%d\n",
      (int) inSafety_,
      (int) inAmber_,
      (int) inAllRed_,
      (unsigned) caseIndex_,
      (unsigned) offNode_,
      (unsigned) leaderId_,
      (int) isLeader_);
}

/* ===================================[ STATUS LED APPLY ]================================ */
void TrafficRs485CoordinatorModule::applyCaseLocal(uint8_t c) {
  if (!ledsPresent())
    return;
  const uint8_t g = greenNode_(c);
  if (g == myId_)
    leds(false, false, true);
  else
    leds(true, false, false);
}
void TrafficRs485CoordinatorModule::applyAmberLocal(uint8_t offNode) {
  if (!ledsPresent())
    return;
  if (offNode == myId_)
    leds(false, true, false);
  else
    leds(true, false, false);
}
void TrafficRs485CoordinatorModule::applySafetyBlink() {
  if (!ledsPresent())
    return;
  const bool on = ((millis() / RS485_AMBER_BLINK_MS) & 1) != 0;
  leds(false, on, false);
}

/* ===================================[ MAIN LOOP ]======================================= */
int32_t TrafficRs485CoordinatorModule::runOnce() {
  if (!ready_)
    initOnce();

  pumpRx();

  if (isLeader_)
    leaderTick_();
  else
    followerTick_();

  driveOutputs_();

  return 25; /* RUN AGAIN IN '25ms' */
}