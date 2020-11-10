#pragma once
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "UsageEnvironment.hh"
#include "liveMedia.hh"
//#include "MyRTSPClient.h"
#include "MediaQueue.h"
#include "trace.h"
#include "stdafx.h"

#ifdef _LP_FOR_LINUX_

#else

#    ifdef MY_DLL
#        define MY_DLL_EXPORTS __declspec(dllexport)
#    else
#        define MY_DLL_EXPORTS __declspec(dllimport)
#    endif // MY_DLL

#endif

// some type shortcuts
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned int fourcc_t;

//
#define FOURCC(a, b, c, d)                                                                         \
    (((uint32_t)a) | (((uint32_t)b) << 8) | (((uint32_t)c) << 16) | (((uint32_t)d) << 24))


// The different types of codecs
enum CodecType
{
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
    CODEC_TYPE_DATA,
    CODEC_TYPE_SUBTITLE,
    CODEC_TYPE_ATTACHMENT,
    CODEC_TYPE_NB
};

// The current state of the RTSP stream
enum RTSPClientState
{
    RTSP_STATE_IDLE, // Nothing happening yet
    RTSP_STATE_OPENING, // Thread started and RTSP commands in progress
    RTSP_STATE_OPENED, // Sessions being set up 
    RTSP_STATE_PLAYING, 
    RTSP_STATE_PAUSED,
    RTSP_STATE_CLOSING, // Started shutting down Sessions and Thread
    RTSP_STATE_CLOSED, // Sessions and Thread shutdown and cleaned up
    RTSP_STATE_SHUTDOWN, // CstreamMedia cleaned up
    RTSP_STATE_ERROR
};


// If we could get the video format from the VSM
// this is where would put it
typedef struct __VideoFormat
{
    int width;
    int height;
    int fps;
    int bitrate;
} VideoFormat;


typedef struct __MediaInfo
{
    enum CodecType codecType;
    fourcc_t i_format;
    char codecName[50];
    VideoFormat video;
    int duration;
    int b_packetized;
    char* extra;
    int extra_size;
} MediaInfo;


class CstreamMedia;
typedef struct __StreamTrack
{
    //  CstreamMedia   *	pstreammedia;
    int waiting;
    MediaInfo mediainfo;
    MediaSubsession* sub;
    char* p_buffer;
    unsigned int i_buffer;
} StreamTrack;


// our worker thread to receive frames on
// DWORD WINAPI rtspRecvDataThread( LPVOID lpParam );
void* rtspRecvDataThread(void* lpParam);

/**
 */
class MyRTSPClient;

class MY_DLL_EXPORTS CstreamMedia
{
private:
    bool m_recvThreadFlag;
    MediaSession* ms;
    TaskScheduler* scheduler;
    UsageEnvironment* env;
    MyRTSPClient* rtsp;
    int i_stream;
    StreamTrack** stream;
    int b_tcp_stream;   
    enum RTSPClientState m_state;
    char event;
    MyMutex* hFrameListLock;
    MyThread* hRecvDataThread;
    // MyEvent*				hDataThreadReady;
    MyEvent* hRecvEvent;
    int nostream;
    int m_frameQueueSize;
    int m_streamPort;
    int m_iTunnelOverHTTPPortNum;   // renamed for clarity
                                    // if non-zero, HTTP-Tunneling will be used regardless of m_bStreamUsingTCP

public:
    /*
     * !!!!!!!!!!!!IMPORTANT!!!!!!!!!!!!!!!!!!!!
     * We Must define the version here.
     *
     * Whenever LiveProxy is updated, new build must be tar'ed 
     * and pushed to artifactory from ../sse as follows:
     * - modify LIVEPROXY-VERSION in Makefile to match what is defined here
     * - make tar-liveproxy-build
     * - make push-liveproxy-build
     */
    static const std::string& VERSION(void)
    {
        static const std::string str = "3.0.0.11";

        return str;
    }
    CstreamMedia(int frameQueueSize, int engineId, int logLevel);
    ~CstreamMedia();

    // The rtsp thread
    int rtspClientPlayStream(const char* url);
    int rtspClientCloseStream(void);
	void rtspClientStopStream(void);

    // State management
    RTSPClientState GetRTSPState() { return m_state; }
    void SetRTSPState(RTSPClientState state);

    // Queue management
    bool GetFrame(BYTE* pData, int bufferSize);
    bool GetFramePtr(FrameInfo** ppframe);
    void DeleteFrame(FrameInfo* frame);
    int GetQueueSize();
    const char* get_Url() { return m_url.c_str(); }
    int GetVideoParms(int* piWidth, int* piHeight);
    void SetStreamPort(int streamPort) { m_streamPort = streamPort; }
    void SetHTTPTunnelPort(int tunnelPort) { m_iTunnelOverHTTPPortNum = tunnelPort; } // renamed for clarity
    void SetTCPStreamPort(int tunnelPort) { m_iTunnelOverHTTPPortNum = tunnelPort; } // backwards compatibility with IBM VA SSE
    void SetStreamUsingTCP(bool useTCP) { m_bStreamUsingTCP = useTCP; }

    const StreamTrack* GetTrack()
    {
        printf("cstreammed get track, num %d, ptr %p\n", i_stream, stream[0]);
        return stream[0];
    }
	std::string m_url;
    unsigned rtspClientCount;
	int m_LogLevel;
	int m_EngineID;
	char eventLoopWatchVariable = 0; // used to break out of event loop
	bool streamUp = false; // indicates if stream is up
	bool threadUp = false; // indicates if thread is up
    bool m_bStreamUsingTCP = false; // indicates whether to stream over TCP (interleaved, not HTTP-Tunneled)

private:
    // our data thread
    // friend DWORD WINAPI rtspRecvDataThread( LPVOID lpParam );
    friend void* rtspRecvDataThread(void* lpParam);
};
