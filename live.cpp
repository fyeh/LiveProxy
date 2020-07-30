#include "live.h"
#include "MyRTSPClient.h"
#include "MyUsageEnvironment.h"
#include "stdafx.h"

#ifdef _LP_FOR_LINUX_

#define _strdup strdup
#define Sleep sleep

#endif

#define keepAliveTimer                                                         \
  55 * 1000000 // how many micro seconds between each keep alive  -- (JIH)
               // FixMe: read this in from the URL

// RTSP 'response handlers':
void continueAfterOPTIONS(RTSPClient *rtspClient, int resultCode,
                          char *resultString);
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode,
                           char *resultString);
void continueAfterSETUP(RTSPClient *rtspClient, int resultCode,
                        char *resultString);
void continueAfterPLAY(RTSPClient *rtspClient, int resultCode,
                       char *resultString);

// Other functions:
void subsessionAfterPlaying(
    void *clientData); // called when a stream's subsession (e.g., audio or
                       // video substream) ends
void subsessionByeHandler(
    void *clientData); // called when a RTCP "BYE" is received for a subsession
void streamTimerHandler(void *clientData);
void onScheduledDelayedTask(void *clientData);

// Used to iterate through each stream's 'subsessions', setting up each one:
void setupNextSubsession(RTSPClient *rtspClient);

// Used to shut down and close a stream (including its "RTSPClient" object):
// exit code 0 means do not try to restart, exit code 1 means try to restart
void shutdownStream(RTSPClient *rtspClient, int exitCode = 0);

// A function that outputs a string that identifies each stream (for debugging
// output).  Modify this if you wish:
UsageEnvironment &operator<<(UsageEnvironment &env,
                             const RTSPClient &rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for
// debugging output).  Modify this if you wish:
UsageEnvironment &operator<<(UsageEnvironment &env,
                             const MediaSubsession &subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}

void *rtspRecvDataThread(void *lpParam) {
  CstreamMedia *mediaClient = (CstreamMedia *)lpParam;
  TRACE_INFO(mediaClient->m_EngineID, "Starting receive data thread");
  TRACE_VERBOSE(mediaClient->m_EngineID, "CstreamMedia& = %p", mediaClient);

  // char*  event = &(rtspClient->event);

  // Begin by setting up our usage environment:
  TaskScheduler *scheduler = BasicTaskScheduler::createNew();
  TRACE_VERBOSE(mediaClient->m_EngineID, "&scheduler = %p", scheduler);
  UsageEnvironment *env = CMyUsageEnvironment::createNew(*scheduler);
  TRACE_VERBOSE(mediaClient->m_EngineID, "&env = %p", env);

  // TaskScheduler* scheduler = new BasicTaskScheduler();
  // UsageEnvironment* env = new CMyUsageEnvironment(*scheduler);

  // Create a "RTSPClient" object.  Note that there is a separate "RTSPClient"
  // object for each stream that we wish to receive (even if more than stream
  // uses the same "rtsp://" URL).
  MyRTSPClient *rtspClient = MyRTSPClient::createNew(
      *env, mediaClient->get_Url(), mediaClient->m_frameQueueSize,
      mediaClient->m_streamPort, mediaClient->m_streamOverTCPPort, mediaClient);
  TRACE_VERBOSE(mediaClient->m_EngineID, "&rtspClient = %p", rtspClient);

  if (rtspClient == NULL) {
    TRACE_ERROR(mediaClient->m_EngineID,
                "Failed to create a RTSP client for URL %s, Msg %s",
                mediaClient->get_Url(), env->getResultMsg());
    // return -1;
    return NULL;
  }

  // fixme - bpg - right now this all only works for 1 stream - hardcoded to
  // index 0
  TRACE_VERBOSE(mediaClient->m_EngineID, "set tk to rtsp client %p",
                mediaClient->stream[0]);

  rtspClient->set_StreamTrack(mediaClient->stream[0]);
  mediaClient->rtspClientCount++;

  mediaClient->scheduler = scheduler;
  mediaClient->env = env;
  mediaClient->rtsp = rtspClient;

  TRACE_VERBOSE(mediaClient->m_EngineID, "Sending options command");
  rtspClient->sendOptionsCommand(continueAfterOPTIONS);

  // All subsequent activity takes place within the event loop:
  TRACE_VERBOSE(mediaClient->m_EngineID, "Starting the event loop");
  mediaClient->eventLoopWatchVariable = 0;
  mediaClient->threadUp = true;
  // This function call does not return, unless, at some point in time,
  // "eventLoopWatchVariable" gets set to something non-zero.
  env->taskScheduler().doEventLoop(&mediaClient->eventLoopWatchVariable);

  // After Exiting Event Loop make sure everything is shut down.
  TRACE_INFO(mediaClient->m_EngineID, "Exited Event Loop");
  mediaClient->SetRTSPState(RTSP_STATE_CLOSING);

  // is stream is up shut it down
  if (mediaClient->streamUp) {
    shutdownStream(rtspClient, 0); // shutdown all sessions and rtspClient
  }

  env->reclaim();
  env = NULL;
  delete scheduler;
  scheduler = NULL;
  mediaClient->scheduler = NULL;
  mediaClient->env = NULL;
  mediaClient->rtsp = NULL;
  mediaClient->threadUp = false;

  TRACE_VERBOSE(mediaClient->m_EngineID, "Set state to RTSP_STATE_CLOSED");
  mediaClient->SetRTSPState(RTSP_STATE_CLOSED);
  mediaClient->rtspClientCloseStream();

  TRACE_INFO(mediaClient->m_EngineID, "Exiting");
  return NULL;
}

/*------------------------------------------------------------*/
// Implementation of the RTSP 'response handlers':
/*------------------------------------------------------------*/
/**
Response to the sendDescribeCommand
*/
void continueAfterOPTIONS(RTSPClient *rtspClient, int resultCode,
                          char *resultString) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID,
             "Code=%d Result (SDP Desc)=%s", resultCode, resultString);

  if (strstr(resultString, "GET_PARAMETER") != NULL) {
    ((MyRTSPClient *)rtspClient)->m_bKeepAliveUseOptions = false;
  }
  TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                "Sending describe command");
  rtspClient->sendDescribeCommand(continueAfterDESCRIBE);
}
/**
Response to the sendDescribeCommand
*/
void continueAfterDESCRIBE(RTSPClient *rtspClient, int resultCode,
                           char *resultString) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID,
             "Code=%d Result (SDP Desc)=%s", resultCode, resultString);

  UsageEnvironment &env = rtspClient->envir(); // alias

  StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias

  char *sdpDescription = resultString;

  if (resultCode != 0) {
    TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                "Failed to get a SDP description:");
    goto cleanup;
  }
  //
  // ***Create a media session object from this SDP description:***
  //
  scs.session = MediaSession::createNew(env, sdpDescription);

  delete[] sdpDescription; // because we don't need it anymore
  if (scs.session == NULL) {
    TRACE_ERROR(
        rtspClient->mediaClient->m_EngineID,
        "Failed to create a MediaSession object from the SDP description: {0}",
        env.getResultMsg());
    goto cleanup;
  } else if (!scs.session->hasSubsessions()) {
    TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                "This session has no media subsessions (i.e., no m=lines)");
    goto cleanup;
  }
  rtspClient->mediaClient->streamUp = true;

  // Then, create and set up our data source objects for the session.  We do
  // this by iterating over the session's 'subsessions', calling
  // "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on
  // each one. (Each 'subsession' will have its own data source.)
  scs.iter = new MediaSubsessionIterator(*scs.session);
  setupNextSubsession(rtspClient);
  return;

  // An unrecoverable error occurred with this stream.
cleanup:
  TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                "Set state to RTSP_STATE_ERROR");
  rtspClient->mediaClient->SetRTSPState(RTSP_STATE_ERROR);
  shutdownStream(rtspClient, 0);
}

/**
Called after the DESCRIBE response
*/
void setupNextSubsession(RTSPClient *rtspClient) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Setup next session");
  UsageEnvironment &env = rtspClient->envir();                // alias
  StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias
  int streamPort = ((MyRTSPClient *)rtspClient)->get_streamPort();

  scs.subsession = scs.iter->next();

  if (scs.subsession != NULL) {

    // Only process subsessions that are of type video.  VA doesn't handle audio
    // at this time
    if (strcmp(scs.subsession->mediumName(), "video") != 0) {
      TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                    "Skipping subsession of type %s",
                    scs.subsession->mediumName());
      setupNextSubsession(rtspClient);
    } else {
      if (streamPort != 0 && scs.subsession->clientPortNum() == 0) {
        TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                      "set stream port to %d", streamPort);
        scs.subsession->setClientPortNum(streamPort);
      }
      if (!scs.subsession->initiate()) {
        TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                    "Failed to initiate the subsession: %s",
                    env.getResultMsg());
        setupNextSubsession(
            rtspClient); // give up on this subsession; go to the next one
      } else {
        TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                      "Initiated the subsession. Ports %d-%d",
                      scs.subsession->clientPortNum(),
                      scs.subsession->clientPortNum() + 1);
        // Continue setting up this subsession, by sending a RTSP "SETUP"
        // command:
        rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP);
      }
    }
    return;
  }

  // We've finished setting up all of the subsessions.  Now, send a RTSP "PLAY"
  // command to start the streaming:
  scs.duration = scs.session->playEndTime() - scs.session->playStartTime();

  TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                "Set state to RTSP_STATE_OPENED");
  rtspClient->mediaClient->SetRTSPState(RTSP_STATE_OPENED);
  TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID, "Sending Play Command");
  rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY,
                              0.00f); //  "now");
}

/**
Response to the sendSetupCommand
*/
void continueAfterSETUP(RTSPClient *rtspClient, int resultCode,
                        char *resultString) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Code=%d Result=%s",
             resultCode, resultString);
  do {
    UsageEnvironment &env = rtspClient->envir();                // alias
    StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias

    if (resultCode != 0) {
      TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                  "Failed to set up the subsession: %s", env.getResultMsg());
      break;
    }

    TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                  "Set up the subsession (client ports %d - %d)",
                  scs.subsession->clientPortNum(),
                  scs.subsession->clientPortNum());

    // Having successfully setup the subsession, create a data sink for it, and
    // call "startPlaying()" on it. (This will prepare the data sink to receive
    // data; the actual flow of data from the client won't start happening until
    // later, after we've sent a RTSP "PLAY" command.)

    // note - VideoSink (and decoder) were set up in MyRTSPClient constructor -
    // just get them here

    MyRTSPClient *client = (MyRTSPClient *)rtspClient;
    MediaSubsession *sub = scs.subsession;

    if (!strcmp(sub->mediumName(), "video")) {

      scs.subsession->sink = client->get_sink();

      if (scs.subsession->sink == NULL) {
        TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                    "Failed to set up the subsession: %s", env.getResultMsg());
        break;
      }

      StreamTrack *tk = client->get_StreamTrack();
      TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                    "Found video session, port %d", sub->clientPortNum());

      tk->sub = sub;
      tk->waiting = 0;
      ZeroMemory(&tk->mediainfo, sizeof(MediaInfo));
      tk->mediainfo.b_packetized = 1;
      strncpy(tk->mediainfo.codecName, sub->codecName(),
              sizeof(tk->mediainfo.codecName));
      tk->mediainfo.duration = sub->playEndTime() - sub->playStartTime();

      tk->i_buffer = DUMMY_SINK_RECEIVE_BUFFER_SIZE;
      tk->p_buffer = new char[tk->i_buffer];
      tk->mediainfo.codecType = CODEC_TYPE_VIDEO;
      tk->mediainfo.i_format = FOURCC('u', 'n', 'd', 'f');
      tk->mediainfo.video.fps = sub->videoFPS();
      tk->mediainfo.video.height = sub->videoHeight();
      tk->mediainfo.video.width = sub->videoWidth();
      TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                    "codec = %s, h %d, w %d", sub->codecName(),
                    sub->videoHeight(), sub->videoWidth());
      if (!strcmp(sub->codecName(), "MPV")) {
        tk->mediainfo.i_format = FOURCC('m', 'p', 'g', 'v');
      } else if (!strcmp(sub->codecName(), "H263") ||
                 !strcmp(sub->codecName(), "H263-1998") ||
                 !strcmp(sub->codecName(), "H263-2000")) {
        tk->mediainfo.i_format = FOURCC('H', '2', '6', '3');
      } else if (!strcmp(sub->codecName(), "H261")) {
        tk->mediainfo.i_format = FOURCC('H', '2', '6', '1');
      } else if (!strcmp(sub->codecName(), "H264")) {
        unsigned int i_extra = 0;
        // char      *p_extra = NULL;

        tk->mediainfo.i_format = FOURCC('h', '2', '6', '4');
        tk->mediainfo.b_packetized = false;

        if (sub->fmtp_spropparametersets() != NULL) {
          // need these SProps to send to the decoder - save them away in NAL
          // format so they can be sent right to the decode engine

          unsigned numSPropRecords = 0;
          SPropRecord *pRecs = parseSPropParameterSets(
              sub->fmtp_spropparametersets(), numSPropRecords);
          TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID, "num SProp %d",
                        numSPropRecords);
          if (numSPropRecords > 0) {
            // count total length for allocate
            // include space for NAL header
            i_extra = 0;
            for (unsigned i = 0; i < numSPropRecords; i++) {
              i_extra += pRecs[i].sPropLength + 3;
            }
            TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                          "total SProp + NAL len %d", i_extra);
            tk->mediainfo.extra_size = i_extra;
            tk->mediainfo.extra = new char[i_extra];
            i_extra = 0;
            for (unsigned i = 0; i < numSPropRecords; i++) {
              tk->mediainfo.extra[i_extra++] = 0x00;
              tk->mediainfo.extra[i_extra++] = 0x00;
              tk->mediainfo.extra[i_extra++] = 0x01;
              memcpy(tk->mediainfo.extra + i_extra, pRecs[i].sPropBytes,
                     pRecs[i].sPropLength);
              i_extra += pRecs[i].sPropLength;
            }

            TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                          "Sending decoder init data");
            ((H264VideoSink *)(sub->sink))
                ->setDecoderInitData((unsigned char *)tk->mediainfo.extra,
                                     tk->mediainfo.extra_size);
          }

          delete[] pRecs;
        }
      } else if (!strcmp(sub->codecName(), "JPEG")) {
        tk->mediainfo.i_format = FOURCC('M', 'J', 'P', 'G');
      } else if (!strcmp(sub->codecName(), "MP4V-ES")) {
        unsigned int i_extra;
        char *p_extra;

        tk->mediainfo.i_format = FOURCC('m', 'p', '4', 'v');

        if ((p_extra =
                 (char *)parseGeneralConfigStr(sub->fmtp_config(), i_extra))) {
          tk->mediainfo.extra_size = i_extra;
          tk->mediainfo.extra = new char[i_extra];
          memcpy(tk->mediainfo.extra, p_extra, i_extra);
          delete[] p_extra;
        }
      } else if (!strcmp(sub->codecName(), "MP2P") ||
                 !strcmp(sub->codecName(), "MP1S")) {
        ;
      } else if (!strcmp(sub->codecName(), "X-ASF-PF")) {
        ;
      }
    }

    TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                  "Created a data sink for the subsession");
    scs.subsession->miscPtr =
        rtspClient; // a hack to let subsession handle functions get the
                    // "RTSPClient" from the subsession
    scs.subsession->sink->startPlaying(*(scs.subsession->readSource()),
                                       subsessionAfterPlaying, scs.subsession);
    // Also set a handler to be called if a RTCP "BYE" arrives for this
    // subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler,
                                                    scs.subsession);
    }
  } while (0);

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

/**
Response to the sendPlayCommand
*/
void continueAfterPLAY(RTSPClient *rtspClient, int resultCode,
                       char *resultString) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Code=%d Result=%s",
             resultCode, resultString);

  UsageEnvironment &env = rtspClient->envir();                // alias
  StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias

  if (resultCode != 0) {
    TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                "Failed to start playing session: %s", resultString);
    TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                  "Set state to RTSP_STATE_ERROR");
    rtspClient->mediaClient->SetRTSPState(RTSP_STATE_ERROR);
    shutdownStream(rtspClient, 1);
    return;
  }

  // hDataThreadReady -> SetEvent();
  TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                "Set state to RTSP_STATE_PLAYING");
  rtspClient->mediaClient->SetRTSPState(RTSP_STATE_PLAYING);
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Started playing session");

  // checkForPacketArrival(rtspClient);

  // The standard only requires keep alives for RTP over RTSP, but some cameras
  // / encoders need it if RTCP is using multicast so add that check as well

  bool sendKeepAlive = ((MyRTSPClient *)rtspClient)->get_TCPstreamPort() != 0;
  const StreamTrack *track =
      ((MyRTSPClient *)rtspClient)->mediaClient->GetTrack();
  if (track == NULL) {
    TRACE_WARN(
        rtspClient->mediaClient->m_EngineID,
        "Could not get StreamTrack from rtspClient, RTSP keepalive is %s",
        (sendKeepAlive) ? "true" : "false");
  } else {
    const MediaSubsession *sub = track->sub;
    if (sub == NULL) {
      TRACE_WARN(rtspClient->mediaClient->m_EngineID,
                 "Could not get subsession from track.  RTSP keepalive is %s",
                 (sendKeepAlive) ? "true" : "false");
    } else {
      TRACE_INFO(rtspClient->mediaClient->m_EngineID,
                 "connection endpoint address is %x",
                 ntohl(sub->connectionEndpointAddress()));
      if ((ntohl(sub->connectionEndpointAddress()) & 0xF0000000) ==
          0xE0000000) {
        TRACE_INFO(rtspClient->mediaClient->m_EngineID,
                   "Session is multicast, setting keepalive to true\n");
        sendKeepAlive = true;
      } 
      /*
      else {
        TRACE_INFO(rtspClient->mediaClient->m_EngineID,
                   "Session is not multicast, keepalive stays at %s",
                   (sendKeepAlive) ? "true" : "false");
      }
      */
    }
  }

  // setup our keep alive using a delayed task
  // the scheduled task will be unscheduled by the scs destructor
  // We are trying sending keep alives in all cases but sendKeepAlive 
  // might get used again in the future.

  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Schedule keep alive task");
  scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
      keepAliveTimer, onScheduledDelayedTask, rtspClient);
}

/*------------------------------------------------------------*/
// Implementation of the other event handlers:
/*------------------------------------------------------------*/
void subsessionAfterPlaying(void *clientData) {
  MediaSubsession *subsession = (MediaSubsession *)clientData;
  RTSPClient *rtspClient = (RTSPClient *)(subsession->miscPtr);
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Entered");

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession &session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) {
      TRACE_INFO(rtspClient->mediaClient->m_EngineID,
                 ">>>>>>>>>>>subsession still active<<<<<<<<<");
      return; // this subsession is still active
    }
  }

  // All subsessions' streams have now been closed, so shutdown the Stream
  // Set the Retrying state so we will try to reopen the connection once.
  shutdownStream(rtspClient, 1);
}

/**
 */
void subsessionByeHandler(void *clientData) {
  MediaSubsession *subsession = (MediaSubsession *)clientData;
  RTSPClient *rtspClient = (RTSPClient *)subsession->miscPtr;
  // UsageEnvironment& env = rtspClient->envir(); // alias

  TRACE_WARN(rtspClient->mediaClient->m_EngineID,
             "!!!!!Received RTCP \"BYE\" on subsession!!!!!");
  subsessionAfterPlaying(subsession);
}

/**
The stream has timed out and needs to be shutdown
*/
void streamTimerHandler(void *clientData) {
  MyRTSPClient *rtspClient = (MyRTSPClient *)clientData;
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Stream timer handler");
  StreamClientState &scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient, 1);
}

/**
Cleanup and shutdown the stream sessions and rtspClient
*/
void shutdownStream(RTSPClient *rtspClient, int exitCode) {
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "ExitCode=%d", exitCode);
  if (exitCode == 1) {
    TRACE_VERBOSE(rtspClient->mediaClient->m_EngineID,
                  "Set state to RTSP_STATE_CLOSING");
    rtspClient->mediaClient->SetRTSPState(RTSP_STATE_CLOSING);
  }
  UsageEnvironment &env = rtspClient->envir();                // alias
  StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias

  // Cancel any pending tasks:
  env.taskScheduler().unscheduleDelayedTask(scs.streamTimerTask);

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) {
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession *subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
        Medium::close(subsession->sink);
        subsession->sink = NULL;
        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the
      // stream. Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }
  CstreamMedia *mediaClient =
      rtspClient->mediaClient; // we'll need this after we close the rtspClient
  TRACE_INFO(rtspClient->mediaClient->m_EngineID, "Closing the rtspClient");
  Medium::close(rtspClient);
  // Note that this will also cause this stream's "StreamClientState" structure
  // to get reclaimed.

  mediaClient->rtspClientCount--;
  mediaClient->streamUp = false;

  if (mediaClient->threadUp) {
    TRACE_INFO(mediaClient->m_EngineID, "Signal Thread to Stop");
    mediaClient->eventLoopWatchVariable = 1; // signal thread to close
  }

  TRACE_INFO(mediaClient->m_EngineID, "Done");
}

void onScheduledDelayedTask(void *clientData) {
  MyRTSPClient *rtspClient = (MyRTSPClient *)(clientData);
  try {
    TRACE_DEBUG(rtspClient->mediaClient->m_EngineID,
               "On delayed task (ie Keep Alive)");
    UsageEnvironment &env = rtspClient->envir();                // alias
    StreamClientState &scs = ((MyRTSPClient *)rtspClient)->scs; // alias
    MediaSession *session = scs.session;

    if (rtspClient->m_bKeepAliveUseOptions) {
        TRACE_DEBUG(rtspClient->mediaClient->m_EngineID, "Sending Options");
      rtspClient->sendOptionsCommand(NULL);
    } else {
      char *ret = NULL;
      // Get the session parameter to do a keep alive
      // rtspClient->getMediaSessionParameter(*session,NULL,ret);
      TRACE_DEBUG(rtspClient->mediaClient->m_EngineID, "Sending Get Parameters");
      rtspClient->sendGetParameterCommand(*session, NULL, ret);
      free(ret);
    }

    // setup our keep alive using a delayed task
    scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(
        keepAliveTimer, onScheduledDelayedTask, rtspClient);
  } catch (...) {
    TRACE_ERROR(rtspClient->mediaClient->m_EngineID,
                "Exception thrown during keep alive");
  }
}

/*------------------------------------------------------------*/
// CstreamMedia class implementation
/*------------------------------------------------------------*/

/**
Constructor
*/
CstreamMedia::CstreamMedia(int frameQueueSize, int engineId, int logLevel) {
  m_LogLevel = logLevel;
  m_EngineID = engineId;
  InitializeTraceHelper(m_LogLevel, NULL);
  // Print the version
  TRACE_CRITICAL(m_EngineID, "****************************************");
  TRACE_CRITICAL(m_EngineID, "*            LIVEPROXY                 *");
  TRACE_CRITICAL(m_EngineID, "*             %s                  *",
                 VERSION().c_str());
  TRACE_CRITICAL(m_EngineID, "****************************************");
  stream = NULL;
  ms = NULL;
  scheduler = NULL;
  env = NULL;
  rtsp = NULL;
  m_url = "";
  b_tcp_stream = 0; // normal udp
  i_stream = 0;
  event = 0;
  SetRTSPState(RTSP_STATE_IDLE);
  m_recvThreadFlag = TRUE;
  // hDataThreadReady=NULL;
  hRecvDataThread = NULL;
  hFrameListLock = NULL;
  m_frameQueueSize = frameQueueSize;
  m_streamOverTCPPort = 0;
  rtspClientCount = 0;
}

/**
Destructor
Cleanup the streams
*/
CstreamMedia::~CstreamMedia() {
  TRACE_INFO(m_EngineID, "Entered");
  StreamTrack *tr;

  rtspClientCloseStream();
  // wait for everything to close as thread functions need to
  // reference the media client variables
  sleep(1);

  SetRTSPState(RTSP_STATE_IDLE);
  m_recvThreadFlag = FALSE;
  if (stream != NULL) {
    for (int i = 0; i < i_stream; i++) {
      tr = (StreamTrack *)stream[i];
      if (tr)
        delete tr;
    }
    free(stream);
  }
  stream = NULL;
  TRACE_INFO(m_EngineID, "Done");
}

/**
Start streaming video from the open stream
*/
int CstreamMedia::rtspClientPlayStream(const char *url) {

  TRACE_INFO(m_EngineID, "URL=%s", url);
  this->m_url = url;
  event = 0;
  if (m_state == RTSP_STATE_OPENING || m_state == RTSP_STATE_OPENED ||
      m_state == RTSP_STATE_PLAYING) {
    return 0;
  }

  SetRTSPState(RTSP_STATE_OPENING);
  StreamTrack *tk = new StreamTrack;
  stream =
      (StreamTrack **)realloc(stream, sizeof(StreamTrack) * (i_stream + 1));
  stream[i_stream++] = tk;
  TRACE_VERBOSE(m_EngineID, "Allocates streamtrack %d, ptr %p", i_stream, tk);

  hRecvDataThread = new MyThread(rtspRecvDataThread, this);

  return 0;
}

/**
close the stream and clean things up
*/
int CstreamMedia::rtspClientCloseStream(void) {
  TRACE_INFO(m_EngineID, "Entered");
  StreamTrack *tr;

  if (m_state == RTSP_STATE_IDLE || m_state == RTSP_STATE_SHUTDOWN) {
    TRACE_VERBOSE(m_EngineID, "nothing to do");
    return (0);
  }

  if (m_state != RTSP_STATE_CLOSED && m_state != RTSP_STATE_CLOSING) {
    TRACE_INFO(m_EngineID, "Signalling Thread to stop");
    eventLoopWatchVariable = 1;
    return 0;
  }

  /*
          while (m_state == RTSP_STATE_OPENING || m_state == RTSP_STATE_OPENED)
          {
                  TRACE_VERBOSE(m_EngineID,"Waiting for Stream to start Playinf
     before shutting it down"); sleep(1);
          }

          if (m_state == RTSP_STATE_PLAYING)
          {
                  TRACE_VERBOSE(m_EngineID,"Terminate the receive thread");
                  eventLoopWatchVariable = 1;
                  while (m_state != RTSP_STATE_CLOSED)
                  {
                          TRACE_VERBOSE(m_EngineID,"Waiting for Thread & Stream
     to close"); sleep(1);
                  }
          }
  */
  if (stream) {
    for (int i = 0; i < i_stream; i++) {
      tr = (StreamTrack *)stream[i];
      if (tr) {
        delete tr;
      }
    }

    free(stream);
    stream = NULL;
    i_stream = 0;
  }

  if (hRecvDataThread != NULL)
    delete hRecvDataThread;
  hRecvDataThread = NULL;

  SetRTSPState(RTSP_STATE_SHUTDOWN);
  TRACE_INFO(m_EngineID, "Done ");
  return (0);
}

/**
Get a video frame from the sink's video queue
*/
bool CstreamMedia::GetFrame(BYTE *pData, int bufferSize) {
  TRACE_DEBUG(m_EngineID, "Entered");
  // FrameInfo* frame = NULL;
  if (rtsp == NULL) {
    TRACE_ERROR(m_EngineID, "No RTSPClient");
    // sleep(1);
    return false;
  }

  if (m_state != RTSP_STATE_PLAYING) {
    TRACE_VERBOSE(m_EngineID, "RTSP stream is not playing");
    // sleep(1);
    return false;
  }

  try {
    H264VideoSink *sink = rtsp->get_sink();
    if (sink != NULL && sink->get_FrameQueue() != NULL) {
      if (sink->get_FrameQueue()->get(pData, bufferSize)) {
        return true;
      } else {
        TRACE_VERBOSE(m_EngineID, "no frames in queue");
        return true;
      }

      /*
  frame = sink->get_FrameQueue()->get();
  if (frame != NULL && bufferSize >= frame->frameHead.FrameLen)
  {
      memcpy((char*)pData, frame->pdata, frame->frameHead.FrameLen);
      TRACE_VERBOSE(m_EngineID,"!!rtspClientReadFrame len = %d count = %d",
  frame->frameHead.FrameLen, sink->get_FrameQueue()->get_Count());
      DeleteFrame(frame);
      return true;
  }

  TRACE_WARN(m_EngineID,"no frames in queue");
  return true;
  */
    } else {
      TRACE_ERROR(m_EngineID, "Lost the video sink");
    }
  } catch (...) {
    TRACE_ERROR(m_EngineID, "Exception get frame");
  }
  return false;
}

/**
Get a video frame from the sink's video queue
NOTE we should not be using this as it may create a race condition.
use GetFrame() instead
bool CstreamMedia::GetFramePtr(FrameInfo** ppframe)
{
    TRACE_DEBUG("Entered.");
    FrameInfo* frame = NULL;
    if (rtsp == NULL)
    {
        TRACE_ERROR(m_EngineID,"No RTSP session");
        //sleep(1);
        return false;
    }

    if (m_state == RTSP_STATE_ERROR)
    {
        TRACE_ERROR(m_EngineID,"Error in RTSP Session.");
        //sleep(1);
        return false;
    }

    if (m_state == RTSP_STATE_IDLE)
    {
        TRACE_WARN(m_EngineID,"RTSP Session is idle.");
        //sleep(1);
        return false;
    }

    try
    {
        // H264VideoSink*sink = ((MyRTSPClient*)rtsp)->get_sink(); // alias
        H264VideoSink* sink = rtsp->get_sink(); // alias
        if (sink != NULL && sink->get_FrameQueue() != NULL)
        {
            frame = sink->get_FrameQueue()->get();
            if (frame != NULL)
            {
                *ppframe = frame;
                TRACE_DEBUG("len = %d frames left = %d, caller deletes",
frame->frameHead.FrameLen, sink->get_FrameQueue()->get_Count());

                // on purpose do not delete frame - it has been passed to caller
                // they MUST call DeleteFrame on it when they are done

                return (true);
                SetRTSPState(RTSP_STATE_PLAYING);
            }

            TRACE_WARN(m_EngineID,"no frames in queue");
            return false;
        }
        else
        {
            if (m_state == RTSP_STATE_OPENING)
            {
                return true;
            }
            // chances are we lost this because of an error
            // we will sleep here before returning to avoid a busy wait loop
            TRACE_ERROR(m_EngineID,"Lost the video sink\n");
            // Sleep( 1000);
        }
    }
    catch (...)
    {
        TRACE_ERROR(m_EngineID,"Exception get frame");
    }
    return false;
}
*/

/*
Sense we allocated the frame on our heap
we need to destory it
**/
void CstreamMedia::DeleteFrame(FrameInfo *frame) {
  try {
    if (frame != NULL)
      free(frame);
    frame = NULL;
  } catch (...) {
  }
}

void CstreamMedia::SetRTSPState(RTSPClientState newState) {
  TRACE_VERBOSE(m_EngineID, "New State = %d", newState);
  m_state = newState;
}

/**
Report back the number of frames left in our video queue
/sa MyRTSPClient
*/
int CstreamMedia::GetQueueSize() {
  if (rtsp == NULL) {
    return 0;
  }
  // H264VideoSink*sink = ((MyRTSPClient*)rtsp)->get_sink(); // alias
  H264VideoSink *sink = rtsp->get_sink(); // alias
  if (sink == NULL || sink->get_FrameQueue() == NULL) {
    return 0;
  }
  return sink->get_FrameQueue()->get_Count();
}

// this would be used when the metadata from an RTSP stream has no vide size
// info we'll pull the info from the decoded frames
int CstreamMedia::GetVideoParms(int *piWidth, int *piHeight) {
  if (rtsp == NULL) {
    return 0;
  }

  H264VideoSink *sink = rtsp->get_sink();
  if (sink == NULL) {
    return 0;
  }
  return sink->getDecoderImageParms(piWidth, piHeight);
}
