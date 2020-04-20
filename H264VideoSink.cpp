#include "stdafx.h"
#include "H264VideoSink.h"
#include <iostream>
#ifndef _LP_FOR_LINUX_
#include <process.h>
#pragma unmanaged
#endif
#include <cmath>
#include "MediaQueue.h"

//static int frame_count;

/**
Static construtor to create our implemenation of a video sink
*/
H264VideoSink* H264VideoSink::createNew(UsageEnvironment& env, MediaSubsession& subsession, int frameQueueSize, char const* streamId, int engineId) {
  return new H264VideoSink(env, subsession, streamId, frameQueueSize, engineId);
}

/**
Singleton constructor
*/
H264VideoSink::H264VideoSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamId, int frameQueueSize, int engineId):
MediaSink(env), m_fSubsession(subsession) , m_ready(0)
{
	TRACE_INFO(engineId, "Constructor");
	m_fStreamId = strDup(streamId);
	m_EngineID = engineId;
	m_bufferSize=253440;//2*DUMMY_SINK_RECEIVE_BUFFER_SIZE;
  m_fPos = 0;
  uint8_t startCode[] = {0x00, 0x00,0x01};

	m_buffer = new unsigned char[m_bufferSize];
	m_frameQueue=new CMediaQueue(frameQueueSize,m_EngineID);
	m_decoder=new CVideoDecoder(m_EngineID);
	AddData(startCode, sizeof(startCode));

  #ifndef _LP_FOR_LINUX_
	InitializeCriticalSection(&m_criticalSection);
  #endif
	m_ready=1;
	TRACE_INFO(engineId, "Constructor done");
}

/**
Destructor
*/
H264VideoSink::~H264VideoSink()
{
	TRACE_INFO(m_EngineID,"Destructor");
	m_ready=0;
    if(m_buffer!=NULL)
        delete [] m_buffer;
    m_buffer = NULL;
    m_bufferSize = 0;

	if(m_fStreamId!=NULL)
        delete [] m_fStreamId;
	m_fStreamId = NULL;

	if (m_decoder != NULL)
		delete m_decoder;
	m_decoder = NULL;

	if(m_frameQueue!=NULL)
		delete m_frameQueue;
	m_frameQueue=NULL;
	TRACE_INFO(m_EngineID,"Destructor Done");
}

/**
Initialize the video buffer with the header information
*/
void H264VideoSink::AddData(uint8_t* aData, int aSize)
{
    memcpy(m_buffer + m_fPos, aData, aSize);
    m_fPos += aSize;
}

void H264VideoSink::setDecoderInitData( unsigned char * pData, int datalen)
{
  m_decoder->DecodeFrame(pData, datalen);
}

/**
Keep retrieving frames from the source
*/
Boolean H264VideoSink::continuePlaying()
{
	if (fSource == NULL)
	{
		TRACE_ERROR(-1,"no source for continue play");
		return False;
	}
	TRACE_DEBUG(-1,"BufferSize=%d",m_bufferSize);
	fSource->getNextFrame(m_buffer + m_fPos, m_bufferSize - m_fPos, afterGettingFrame, this, onSourceClosure, this);
	return True;
}

/**
Called by the live555 code once we have a frame
*/
void H264VideoSink::afterGettingFrame(void* clientData, unsigned frameSize, unsigned numTruncatedBytes,
				  struct timeval presentationTime, unsigned durationInMicroseconds)
{
	TRACE_DEBUG(-1,"FrameSize=%d",frameSize);
	H264VideoSink* sink = (H264VideoSink*)clientData;
	sink->afterGettingFrame1(frameSize, presentationTime);
	if (sink->continuePlaying() == false)
	{
		TRACE_ERROR(-1,"Continue play failed closing source");
		sink->onSourceClosure(clientData);
	}
}

/**
Prepare the video frame for decoding
*/
void H264VideoSink::afterGettingFrame1(unsigned frameSize, struct timeval presentationTime)
{
  //int got_frame = 0;
  unsigned int size = frameSize;
  unsigned char *pBuffer = m_buffer + m_fPos;
  uint8_t* data = (uint8_t*)pBuffer;
  uint8_t startCode4[] = {0x00, 0x00, 0x00, 0x01};
  uint8_t startCode3[] = {0x00, 0x00, 0x01};
	FrameInfo	*frame=NULL;

	if(size<4){
		return;
  }
  if(memcmp(startCode3, pBuffer, sizeof(startCode3)) == 0)
	{
		data += 3;
  }else if(memcmp(startCode4, pBuffer, sizeof(startCode4)) == 0){
		data += 4;
  }else{
		pBuffer -= 3;
		size += 3;
  }

	// send the frame out to the decoder
	TRACE_DEBUG(m_EngineID,"Decoding Frame");
	frame=m_decoder->DecodeFrame(pBuffer, size);
	if(frame!=NULL)
  {
	TRACE_DEBUG(m_EngineID,"++++++Decoded Frame");
		m_frameQueue->put(frame);
  }
}


int H264VideoSink::getDecoderImageParms( int * piWidth, int * piHeight)
{
  return m_decoder->getDecoderImageParms(piWidth, piHeight);
}
