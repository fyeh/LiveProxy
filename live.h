#pragma once
#include "UsageEnvironment.hh"
#include "BasicUsageEnvironment.hh"
#include "GroupsockHelper.hh"
#include "liveMedia.hh"
//#include "MyRTSPClient.h"
#include "MediaQueue.h"

#ifdef _LP_FOR_LINUX_

#else

#ifdef MY_DLL
    #define MY_DLL_EXPORTS  __declspec(dllexport)
#else
    #define MY_DLL_EXPORTS  __declspec(dllimport)
#endif //MY_DLL

#endif

//some type shortcuts
typedef unsigned char 	uint8_t;
typedef unsigned short 	uint16_t;
typedef unsigned int   	uint32_t;
typedef unsigned int  	fourcc_t;

#include "trace.h"

//
#define FOURCC( a, b, c, d ) ( ((uint32_t)a) | ( ((uint32_t)b) << 8 ) | ( ((uint32_t)c) << 16 ) | ( ((uint32_t)d) << 24 ) )


//The different types of codecs
enum CodecType {
    CODEC_TYPE_UNKNOWN = -1,
    CODEC_TYPE_VIDEO,
    CODEC_TYPE_AUDIO,
    CODEC_TYPE_DATA,
    CODEC_TYPE_SUBTITLE,
    CODEC_TYPE_ATTACHMENT,
    CODEC_TYPE_NB
};

//The current state of the RTSP stream
enum RTSPClientState {
  RTSP_STATE_IDLE,    /**< not initialized */
	RTSP_STATE_OPENED,
  RTSP_STATE_PLAYING, /**< initialized and receiving data */
  RTSP_STATE_PAUSED,  /**< initialized, but not receiving data */
	RTSP_STATE_LOADING,
	RTSP_STATE_ERROR
};


//If we could get the video format from the VSM
//this is where would put it
typedef struct __VideoFormat
{
	int	width;
	int	height;
	int	fps;
	int bitrate;
}VideoFormat;


typedef struct __MediaInfo
{
	enum CodecType	codecType;
	fourcc_t		i_format;
	char			codecName[50];
	VideoFormat		video;
	int				duration;
	int				b_packetized;
	char*			extra;
	int				extra_size;
}MediaInfo;


class CstreamMedia;
typedef struct __StreamTrack
{
//  CstreamMedia   *	pstreammedia;
	int 				waiting;
	MediaInfo   		mediainfo;
	MediaSubsession*	sub;
	char*				p_buffer;
	unsigned int		i_buffer;
} StreamTrack;


//our worker thread to receive frames on
//DWORD WINAPI rtspRecvDataThread( LPVOID lpParam );
void *  rtspRecvDataThread( void * lpParam );

/**
*/
class MyRTSPClient;

class MY_DLL_EXPORTS CstreamMedia
{

private:
	bool				m_recvThreadFlag;
  MediaSession		*ms;
  TaskScheduler		*scheduler;
  UsageEnvironment	*env ;
  MyRTSPClient			*rtsp;
	int					i_stream;
  StreamTrack			**stream;
	int					b_tcp_stream;
	std::string			m_url;
	enum RTSPClientState	m_state;
	char					event;
	MyMutex* 				hFrameListLock;
	MyThread*				hRecvDataThread;
	//MyEvent*				hDataThreadReady;
	MyEvent*  			hRecvEvent;
	int						nostream;
	int						m_frameQueueSize;
  int           m_streamPort;
  int           m_streamOverTCPPort;

 public:
	 CstreamMedia(int frameQueueSize);
	 ~CstreamMedia();

	//The rtsp thread
//	int rtspClientOpenStream(const char* filename);
	int rtspClientPlayStream(const char* url);
//	int rtspClientGetMediaInfo(enum CodecType codectype, MediaInfo& mediainfo);
	int rtspClientCloseStream(void);

	//Queue management
	bool GetFrame(BYTE * pData, int bufferSize);
  bool GetFramePtr(FrameInfo** ppframe);
	void DeleteFrame(FrameInfo* frame);
	int GetQueueSize();
	const char * get_Url(){return m_url.c_str();}
  int GetVideoParms( int * piWidth, int * piHeight);
  void SetStreamPort( int streamPort) {m_streamPort = streamPort;}
  void SetTCPStreamPort( int streamPort) {m_streamOverTCPPort = streamPort;}

  const StreamTrack *  GetTrack() {printf( "cstreammed get track, num %d, ptr %p\n", i_stream, stream[0]);return stream[0];}

private:
	//our data thread
	//friend DWORD WINAPI rtspRecvDataThread( LPVOID lpParam );
  friend void *  rtspRecvDataThread( void * lpParam );
};
