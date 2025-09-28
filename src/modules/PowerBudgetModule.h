/**************************************************************
 *  Project : Blackout Traffic Light System                    *
 *  Module  : Power Budget                                     *
 *  Author  : Yeray Lois Sanchez                               *
 *  Email   : yerayloissanchez@gmail.com                       *
 ***************************************************************/
#pragma once

#include <Arduino.h>

#include "concurrency/OSThread.h"
#include "mesh/SinglePortModule.h"
#include "mesh/generated/meshtastic/portnums.pb.h"

class PowerBudgetModule final : public SinglePortModule, public concurrency::OSThread {
public:
  static constexpr meshtastic_PortNum kPort = meshtastic_PortNum_PRIVATE_APP;

  PowerBudgetModule();

  int32_t        runOnce() override;
  ProcessMessage handleReceived(const meshtastic_MeshPacket&) override {
    return ProcessMessage::CONTINUE;
  }
  meshtastic_PortNum getPortNum() const {
    return kPort;
  }

private:
  void initOnce();
  void printHeaderOnce();
  void printLive(float V, float I, float P);
  void printSummary();

  // TIMERS
  uint32_t t0_           = 0;
  uint32_t tPrev_        = 0;
  uint32_t tNextPrint_   = 0;
  uint32_t tNextSummary_ = 0;

  // ACCUMULATORS
  double mWh_ = 0.0;
  double mAh_ = 0.0;

  // MIN/MAX
  float Vmin_ = 1e9f, Vmax_ = -1e9f;
  float Imin_ = 1e9f, Imax_ = -1e9f;
  float Pmax_ = -1e9f;

  bool ready_ = false;
};