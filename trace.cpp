//#pragma once
#include "stdafx.h"
//#include <atlstr.h>
#include "trace.h"

#ifdef _LP_FOR_LINUX_

#include <unistd.h>
#include <pthread.h>

#define GetCurrentProcessId()	getpid()
//#define GetCurrentThreadId()	pthread_self()
#define GetCurrentThreadId() "0"

#endif

extern "C"
{
	int g_logLevel=LOGLEVEL_ERROR; //-1;
	logHandler* loggingCallback=NULL;
	/**
	Native interface to managed logger
	*/
	_declspec(dllexport) void Write(int level, const char * msg)
	{
		if(loggingCallback!=NULL)
		{
			loggingCallback(level, msg);
		}
		else
		{
			if( msg[strlen( msg) -1] == '\n')
		  	printf( "%s", msg);
			else
				printf( "%s\n", msg);
		}
	}

	_declspec(dllexport) void InitializeTraceHelper(int level,logHandler* callback){
		g_logLevel=level;
		loggingCallback=callback;
	}
	/**
	Creates the message
	*/
	_declspec(dllexport) void Log(int level, const char * functionName, const char * lpszFormat, ...)
	{
		if(level>g_logLevel)return;
		static char szMsg[2048];
		va_list argList;
		va_start(argList, lpszFormat);
		try
		{
			vsnprintf(szMsg,2000,lpszFormat, argList);
		}
		catch(...)
		{
			strcpy(szMsg ,"DebugHelper:Invalid string format!");
		}
		va_end(argList);
		//std::string logMsg=static_cast<std::ostringstream*>( &(std::ostringstream() << ::GetCurrentProcessId() << "," << ::GetCurrentThreadId() << "," << functionName << ", " <<  szMsg) )->str();
		std::ostringstream oss;
		oss << GetCurrentProcessId() << "," << GetCurrentThreadId() << "," << functionName << ", " <<  szMsg;
		std::string logMsg=oss.str();
		try{
			Write(level, logMsg.c_str());
		}
		catch(...)
		{
		}
	}

}
