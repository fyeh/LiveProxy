//#pragma once
#include "stdafx.h"
#include "MyRTSPClient.h"

/**
Static method to create an new instance of our version of a RTSPClient
*/
MyRTSPClient* MyRTSPClient::createNew(UsageEnvironment& env, char const* rtspURL, int frameQueueSize, int streamPort, portNumBits tunnelOverHTTPPortNum, CstreamMedia* client)
{
	int level= g_logLevel;

	if (level < 5)
	{
		level = 0;
	}
	//return new MyRTSPClient(env, rtspURL, level, "LiveProxy", 0, frameQueueSize, streamPort);
	return new MyRTSPClient(env, rtspURL, level, "LiveProxy", tunnelOverHTTPPortNum, frameQueueSize, streamPort, client);
}

/**
Singleton constructor
*/
MyRTSPClient::MyRTSPClient(UsageEnvironment& env, char const* rtspURL,
			     int verbosityLevel, char const* applicationName, portNumBits tunnelOverHTTPPortNum, int frameQueueSize, int streamPort, CstreamMedia* client)
  : RTSPClient(env,rtspURL, verbosityLevel, applicationName, tunnelOverHTTPPortNum, -1, client), m_sink(NULL)
{
	TRACE_INFO(client->m_EngineID,"Constructor");
	TRACE_VERBOSE(client->m_EngineID,"Created MyRTSPClient, tunnel over TCP %d", tunnelOverHTTPPortNum);
	TRACE_VERBOSE(client->m_EngineID,rtspURL);

  	m_tunnelOverHTTPPortNum = tunnelOverHTTPPortNum;
  	mediaClient = client;
	m_sink=H264VideoSink::createNew(env, *scs.subsession, frameQueueSize, "DMH_STREAM");//rtspClient->url());
	m_streamPort = streamPort;
	m_bErrorState = false;
	tk = NULL;
	TRACE_INFO(client->m_EngineID,"Constructor Done");
}

/**
Destructor.
No work at this level
*/
MyRTSPClient::~MyRTSPClient()
{
	TRACE_INFO(mediaClient->m_EngineID,"Entered");
	// if stream was opened, the subsession takes control of the sink and will destroy it
	// thus - we do not destroy it ourselves
	// should revisit this, but it is that way for now
	//if(m_sink!=NULL)
		//delete m_sink;
	m_sink=NULL;
	TRACE_INFO(mediaClient->m_EngineID,"Done");

}
