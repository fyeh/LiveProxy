// Implementation of "StreamClientState":
//#pragma once
#include "stdafx.h"
#include "StreamClientState.h"
#include "live.h"
#include "trace.h"
/**
Stream client constructor.
Clear out of public propertices
*/
StreamClientState::StreamClientState()
  : iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {
    TRACE_INFO(-1,"Constructor");
}

/**
Stream client destructor.
Clean up the env
*/
StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    TRACE_INFO(-1,"Destructor");
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}
