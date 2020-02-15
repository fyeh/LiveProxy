#pragma once

#ifdef _LP_FOR_LINUX_

#include <pthread.h>

#define __declspec(dllexport)
#define _declspec(dllexport)
#define MY_DLL_EXPORTS

#define FALSE	false
#define TRUE	true

#define WINAPI
#define DWORD  unsigned int
#define LPVOID  void *
#define BYTE  unsigned char

#define MY_WAIT_TIMEOUT  99999

#define ZeroMemory(ptr, sz) memset(ptr, 0, sz)

#else

#define MY_WAIT_TIMEOUT  WAIT_TIMEOUT

#endif


class MyMutex
{

public:
	MyMutex();
	virtual ~MyMutex();

	void ReleaseMutex();
	void LockMutex();

private:
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_t	m_mutex;
	#else
	HANDLE	m_mutex;
	#endif

};

class MyEvent
{

public:
	MyEvent();
	virtual ~MyEvent();

	void SetEvent();
  int WaitEvent( int iWaitTime);
	void ResetEvent();

private:
	#ifdef _LP_FOR_LINUX_
	bool m_bSignal;
	bool m_bAutoReset;
	pthread_mutex_t m_mutex;
	#else
	HANDLE m_hEvent;
	#endif

};

class MyThread
{

public:
	MyThread(void * (*pThreadProc)(void*), void* pArg);
	virtual ~MyThread();

  void TerminateThread( int rc);

private:

	#ifdef _LP_FOR_LINUX_
	pthread_t m_hWorkThread;
	#else
	HANDLE m_hWorkThread;
	#endif

};
