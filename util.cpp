#include "util.h"


#ifdef _LP_FOR_LINUX_

#include <unistd.h>
#include <sys/time.h>

#define LP_WAIT_EVENT_FALSE			0
#define LP_WAIT_EVENT_TRUE			1
#define LP_WAIT_EVENT_TIME_OUT		2

#define LP_EVENT_SLEEP_TIME	(10 * 1000)

#endif


MyMutex::MyMutex()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_init(&m_mutex, NULL);
	#else
	m_mutex = CreateMutex(NULL,FALSE,NULL);
	#endif

}

MyMutex::~MyMutex()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_destroy(&m_mutex);
	#else
	CloseHandle( m_mutex);
	#endif
}

void MyMutex::ReleaseMutex()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_unlock(&m_mutex);
	#else
	ReleaseMutex(m_mutex);
	#endif
}

void MyMutex::LockMutex()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_lock(&m_mutex);
	#else
	WaitForSingleObject(m_mutex, INFINITE);
	#endif

}




MyEvent::MyEvent()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_init(&m_mutex, NULL);
	m_bSignal = false;   // bInitialState;
	m_bAutoReset = true;  // !bManualReset;
	#else
	m_hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	#endif

}

MyEvent::~MyEvent()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_destroy(&m_mutex);
	#else
	CloseHandle( m_hEvent);
	#endif
}

void MyEvent::SetEvent()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_lock(&m_mutex);
	m_bSignal = true;
	pthread_mutex_unlock(&m_mutex);
	#else
	SetEvent(m_hEvent);
	#endif

}

int MyEvent::WaitEvent( int iWaitTime)
{
	#ifdef _LP_FOR_LINUX_
	int liStartTime = 0;
	while (true)
	{
		bool bSignal = false;
		pthread_mutex_lock(&m_mutex);
		if (m_bSignal)
		{
			bSignal = m_bSignal;
			if (m_bAutoReset)
				m_bSignal = false;
		}
		pthread_mutex_unlock(&m_mutex);

		if (bSignal)
			return LP_WAIT_EVENT_TRUE;

		if (iWaitTime == 0)
			return LP_WAIT_EVENT_FALSE;

		if (iWaitTime == -1)
		{
			usleep(LP_EVENT_SLEEP_TIME);
			continue;
		}

		timeval tv;
		gettimeofday(&tv, NULL);
		int liTmp = tv.tv_sec;
		liTmp = liTmp * 1000 + tv.tv_usec / 1000;

		if (liStartTime != 0)
		{
			if ((liTmp - liStartTime) >= iWaitTime)
				return LP_WAIT_EVENT_TIME_OUT;
		}
		else
			liStartTime = liTmp;

		usleep(LP_EVENT_SLEEP_TIME);
	}
	#else
		WaitForSingleObject(m_hEvent, iWaitTime);
	#endif

	return 0;
}

void MyEvent::ResetEvent()
{
	#ifdef _LP_FOR_LINUX_
	pthread_mutex_lock(&m_mutex);
	m_bSignal = false;
	pthread_mutex_unlock(&m_mutex);
	#else
	ResetEvent(m_hEvent);
	#endif

}

MyThread::MyThread(void * (*pThreadProc)(void*), void * pArg)
{
	#ifdef _LP_FOR_LINUX_
	pthread_create(&m_hWorkThread, NULL, pThreadProc, pArg);
	#else
	m_hWorkThread = CreateThread(NULL, 0, pThreadProc, pArg, 0, NULL);
	#endif

}

MyThread::~MyThread()
{
	#ifdef _LP_FOR_LINUX_
	#else
	CloseHandle(m_hWorkThread)
	#endif
}

void MyThread::TerminateThread( int rc)
{
	#ifdef _LP_FOR_LINUX_
	pthread_cancel(m_hWorkThread);
	#else
	TerminateThread(m_hWorkThread, rc);
	#endif
}
