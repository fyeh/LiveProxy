#include "stdafx.h"
#include "VideoDecoder.h"
#include "MediaQueue.h"
#include "trace.h"
//#pragma unmanaged

/** Create a FrameInfo object.
*  inline helper to create FrameInfo object
\param frame_size Default frame size
*/
inline FrameInfo* FrameNew(int frame_size = 4096)
{
	FrameInfo* frame = (FrameInfo*)malloc(sizeof(FrameInfo)+frame_size);
	if (frame == NULL)
		return(NULL);
	frame->pdata = (char *)frame + sizeof(FrameInfo);//new char[frame_size];
	frame->frameHead.FrameLen = frame_size;
	return(frame);
}

/** VideoDecoder Constructor.
* Allocates a AVFrame that will be reused by the decoder.  Always
* initializes the ffmpeg decoding library, to accept a H264 frame
*/
CVideoDecoder::CVideoDecoder(int engineId)
{
	m_EngineID = engineId;
	TRACE_INFO(m_EngineID,"Constructor");
	try
	{
		avcodec_register_all();

    av_log_set_level(AV_LOG_ERROR);

		//m_frame = avcodec_alloc_frame();
		m_frame = av_frame_alloc();

		TRACE_VERBOSE(m_EngineID,"Find codec");
		//m_codec = avcodec_find_decoder(CODEC_ID_H264);
		m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
		if ( m_codec != NULL)
		{
			TRACE_VERBOSE(m_EngineID,"Decoder found");
		}else{
			TRACE_ERROR(m_EngineID,"H264 Codec decoder not found");
		}

		TRACE_VERBOSE(m_EngineID,"Allocate codec context");
		m_codecContext = avcodec_alloc_context3(m_codec);
		if ( m_codecContext == NULL)
		{
			TRACE_ERROR(m_EngineID,"failed to allocate codec context");
		}

		m_codecContext->flags = 0;

//    if (m_codec->capabilities & AV_CODEC_CAP_TRUNCATED)
//        m_codecContext->flags |= AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

		TRACE_VERBOSE(m_EngineID,"open codec");
		int ret=avcodec_open2(m_codecContext, m_codec, NULL);
		if (ret < 0)
		{
			TRACE_ERROR(m_EngineID,"Error opening codec ret=%d",ret);
		}else{
			TRACE_VERBOSE(m_EngineID,"AV Codec found and opened");
		}

		//if ( m_codecContext->codec_id == CODEC_ID_H264 )
		if ( m_codecContext->codec_id == AV_CODEC_ID_H264 )
			m_codecContext->flags2 |= AV_CODEC_FLAG2_CHUNKS;
	}
	catch (...)
	{
		TRACE_WARN(m_EngineID,"Ignoring Exception");
	}
	TRACE_INFO(m_EngineID,"Constructor Done");
}

/** VideoDecoder descructor.
 Cleans up the ffmpeg environment and
 frees the AVFrame
*/

CVideoDecoder::~CVideoDecoder()
{
	TRACE_INFO(m_EngineID,"Destructor");
  if (m_frame!=NULL)
  {
      avcodec_close(m_codecContext);
      av_free(m_frame);
  }
  m_frame = NULL;

  if (m_codecContext!=NULL)
      av_free(m_codecContext);
	m_codecContext = NULL;
	TRACE_INFO(m_EngineID,"Destructor Done");
}

/** Decoder.
 The supplied buffer should contain an H264 video frame, then DecodeFrame
  will pass the buffer to the avcode_decode_video2 method. Once decoded we then
  use the get_picture command to convert the frame to RGB24.  The RGB24 buffer is
  then used to create a FrameInfo object which is placed on our video queue.

  \param pBuffer Memory buffer holding an H264 frame
  \param size Size of the buffer
*/
FrameInfo* CVideoDecoder::DecodeFrame(unsigned char *pBuffer, int size)
{
		FrameInfo	*p_block=NULL;
    uint8_t startCode4[] = {0x00, 0x00, 0x00, 0x01};
    //int got_frame = 0;
    AVPacket packet;

		//Initialize optional fields of a packet with default values.
		av_init_packet(&packet);

		//set the buffer and the size
    packet.data = pBuffer;
    packet.size = size;

    while (packet.size > sizeof(startCode4))
		{
			TRACE_DEBUG(m_EngineID,"sending decode of %d", size);
			#if 0
			//Decode the video frame of size avpkt->size from avpkt->data into picture.
			int len = avcodec_decode_video2(m_codecContext, m_frame, &got_frame, &packet);
			if(len<0)
			{
				TRACE_ERROR(m_EngineID,"Failed to decode video error rc=%d",len);
				break;
			}

			//sometime we dont get the whole frame, so move
			//forward and try again
      if ( !got_frame )
			{
				TRACE_DEBUG(m_EngineID,"did not get full frame, consumed %d, continue", len);
				packet.size -= len;
				packet.data += len;
				continue;
			}
			#else
			int  iRet = avcodec_send_packet( m_codecContext, &packet);
			if (iRet == 0)
			{
				packet.size -= size;
				packet.data += size;
				iRet = avcodec_receive_frame(m_codecContext, m_frame);
			}
			else
			{
				TRACE_DEBUG(m_EngineID,"got error ret %d from send packet\n", iRet);
				return NULL;
			}
			if( iRet != 0 )
			{
				TRACE_DEBUG(m_EngineID,"got error ret %d from receive frame\n", iRet);
				return NULL;
			}
			#endif

			//allocate a working frame to store our rgb image
      //AVFrame * rgb = avcodec_alloc_frame();
			AVFrame * rgb = av_frame_alloc();
			if(rgb==NULL)
			{
				TRACE_ERROR(m_EngineID,"Failed to allocate new av frame");
				return NULL;
			}

			//Allocate and return an SwsContext.
			TRACE_DEBUG(m_EngineID,"convert now w %d, h %d\n", m_codecContext->width,m_codecContext->height);
			struct SwsContext * scale_ctx = sws_getContext(m_codecContext->width,
				m_codecContext->height,
				m_codecContext->pix_fmt,
				m_codecContext->width,
				m_codecContext->height,
				//PIX_FMT_BGR24,
				AV_PIX_FMT_BGR24,
				//AV_PIX_FMT_RGB24,
				SWS_BICUBIC,
				NULL,
				NULL,
				NULL);
      if (scale_ctx == NULL)
			{
				TRACE_ERROR(m_EngineID,"Failed to get context");
				continue;
			}

			//Calculate the size in bytes that a picture of the given width and height would occupy if stored in the given picture format.
			//int numBytes = avpicture_get_size(PIX_FMT_RGB24,
			int numBytes = avpicture_get_size(AV_PIX_FMT_RGB24,
				m_codecContext->width,
				m_codecContext->height);

			try{
				//create one of our FrameInfo objects
				p_block = FrameNew(numBytes);
				if(p_block==NULL){

					//cleanup the working buffer
					av_free(rgb);
					sws_freeContext(scale_ctx);
					scale_ctx=NULL;
					return NULL;
				}

				//Fill our frame buffer with the rgb image
				avpicture_fill((AVPicture*)rgb,
					(uint8_t*)p_block->pdata,
					//PIX_FMT_RGB24,
					AV_PIX_FMT_RGB24,
					m_codecContext->width,
					m_codecContext->height);

				//Scale the image slice in srcSlice and put the resulting scaled slice in the image in dst.
				sws_scale(scale_ctx,
					m_frame->data,
					m_frame->linesize,
					0,
					m_codecContext->height,
					rgb->data,
					rgb->linesize);

				//set the frame header to indicate rgb24
				//p_block->frameHead.FrameType = (long)(PIX_FMT_RGB24);
				p_block->frameHead.FrameType = (long)(AV_PIX_FMT_RGB24);
				p_block->frameHead.TimeStamp = 0;
			}
			catch(...)
			{
				TRACE_ERROR(m_EngineID,"EXCEPTION: in afterGettingFrame1 ");
			}

			//cleanup the working buffer
      av_free(rgb);
			sws_freeContext(scale_ctx);

			//we got our frame now its time to move on
			break;
    }
		return p_block;
}


int CVideoDecoder::getDecoderImageParms( int * piWidth, int * piHeight)
{
  if (m_codecContext!=NULL)
	{
		*piWidth = m_codecContext->width;
		*piHeight = m_codecContext->height;

		return 1;
	}
	return 0;
}
