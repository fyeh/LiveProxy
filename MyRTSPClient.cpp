//#pragma once
#include "stdafx.h"
#include "MyRTSPClient.h"


/**
Static method to create an new instance of our version of a RTSPClient
*/
MyRTSPClient* MyRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL, int frameQueueSize, int streamPort, portNumBits tunnelOverHTTPPortNum)
{
	int level= g_logLevel;

	//return new MyRTSPClient(env, rtspURL, level, "LiveProxy", 0, frameQueueSize, streamPort);
	return new MyRTSPClient(env, rtspURL, level, "LiveProxy", tunnelOverHTTPPortNum, frameQueueSize, streamPort);
}

/**
Singleton constructor
*/
MyRTSPClient::MyRTSPClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum, int frameQueueSize, int streamPort)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1), m_sink(NULL)
{
	TRACE_INFO("Created MyRTSPClient, tunnel over TCP %d", tunnelOverHTTPPortNum);
	TRACE_INFO(rtspURL);

  m_tunnelOverHTTPPortNum = tunnelOverHTTPPortNum;
	m_sink=H264VideoSink::createNew(env, *scs.subsession, frameQueueSize, "DMH_STREAM");//rtspClient->url());
	m_streamPort = streamPort;
	tk = NULL;
}

/**
Destructor.
No work at this level
*/
MyRTSPClient::~MyRTSPClient()
{
	TRACE_INFO("Destroy RTSP client");
	// if stream was opened, the subsession takes control of the sink and will destroy it
	// thus - we do not destroy it ourselves
	// should revisit this, but it is that way for now
	//if(m_sink!=NULL)
		//delete m_sink;
	m_sink=NULL;

}
