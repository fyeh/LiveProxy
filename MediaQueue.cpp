#include "stdafx.h"
#include "MediaQueue.h"
#include "trace.h"


/**
The queue of video frames.

Initalize the queue with the specified size
\param queueSize The size of the video queue
*/
CMediaQueue::CMediaQueue(int queueSize)
{
	count = 0;
	size=queueSize;
	head = tail = NULL;
	ptr = pstatic = (MediaQueue *)malloc(sizeof(MediaQueue)*queueSize);
  ZeroMemory(pstatic, sizeof(MediaQueue)*queueSize);
	head = ptr;
	readpos = writepos = head;
	for (int i = 1; i < queueSize; i++)
	{
		ptr->next = pstatic+i;
		ptr = ptr->next;
	}

	tail = ptr;
	tail->next =  head;
	ptr = head;
	//hFrameListLock = CreateMutex(NULL,FALSE,NULL);
	//hRecvEvent     = CreateEvent(NULL, TRUE, FALSE, NULL);
	hFrameListLock = new MyMutex();
	hRecvEvent     = new MyEvent();

}

/**
Cleanup the queuw
*/
CMediaQueue::~CMediaQueue()
{
	delete hRecvEvent;
	delete hFrameListLock;

	//empty the queue
	while (count >= 0)
	{
		//remove a frame so we can add one
		count--;
		if (readpos!=NULL && readpos->frame != NULL)
		{
			free(readpos->frame);
			readpos->frame = NULL;
		}
		readpos = readpos->next;
	}
	free(pstatic);

}


/**
Add a new video frame to the queue
*/
void CMediaQueue::put(FrameInfo* frame)
{
	if(ptr == NULL)
		return;

	hFrameListLock -> LockMutex();
	TRACE_DEBUG("new frame, have %d", count);

	if (count >= size)
	{
		TRACE_DEBUG("drop frame to make room");
		//remove a frame so we can add one
		count = size-1;
		if (readpos->frame)
		{
			free(readpos->frame);
			readpos->frame = NULL;
		}
		readpos = readpos->next;
	}

	//add the frame
	writepos->frame = frame;
	writepos =  writepos->next;
	count++;

	if (count <=1)
	{
		hRecvEvent -> SetEvent();
	}
	hFrameListLock -> ReleaseMutex();
}

/**
Remove a video frame from the queue
*/
FrameInfo* CMediaQueue::get()
{
	FrameInfo* frame = NULL;

	if (count < 1)
	{
		TRACE_DEBUG("No frames in queue, waiting");
		hRecvEvent -> WaitEvent(500);
	}

	hRecvEvent -> ResetEvent();

	hFrameListLock -> LockMutex();
	if(count > 0)
	{
		frame = readpos->frame;
		readpos->frame =  NULL;
		readpos = readpos->next;
		count--;
	}else{
		TRACE_ERROR("No frames in queue after wait");
	}
	hFrameListLock -> ReleaseMutex();

	return(frame);
}

/**
Empty the queue
*/
void CMediaQueue::reset()
{
	hFrameListLock -> LockMutex();
	ptr = readpos;
	while (ptr->frame)
	{
		free(ptr->frame);
		ptr->frame = NULL;
		ptr = ptr->next;
	}
	writepos = readpos;
	count  = 0;
	hFrameListLock -> ReleaseMutex();

}
