/*
#include "TestUtil.h"

#include "concurrency/OSThread.h"
#include "gps/RTC.h"

#include "SerialConsole.h"

void initializeTestEnvironment() {
  concurrency::hasBeenSetup = true;
  consoleInit();
#if ARCH_PORTDUINO
  struct timeval tv;
  tv.tv_sec  = time(NULL);
  tv.tv_usec = 0;
  perhapsSetRTC(RTCQualityNTP, &tv);
#endif
  concurrency::OSThread::setup();
}
*/