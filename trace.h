#pragma once
#include "stdafx.h"
#include "util.h"

#define LOGLEVEL_OFF 0
#define LOGLEVEL_CRITICAL 1
#define LOGLEVEL_ERROR 2
#define LOGLEVEL_WARN 3
#define LOGLEVEL_INFO 4
#define LOGLEVEL_VERBOSE 5
#define LOGLEVEL_DEBUG 6

#define TRACE_CRITICAL(y,x,...) Log(LOGLEVEL_CRITICAL, __FUNCTION__, y, x, ##__VA_ARGS__)
#define TRACE_ERROR(y,x,...) Log(LOGLEVEL_ERROR, __FUNCTION__, y, x, ##__VA_ARGS__)
#define TRACE_WARN(y,x,...) Log(LOGLEVEL_WARN, __FUNCTION__, y, x, ##__VA_ARGS__)
#define TRACE_INFO(y,x,...) Log(LOGLEVEL_INFO, __FUNCTION__, y, x, ##__VA_ARGS__)
#define TRACE_VERBOSE(y,x,...) Log(LOGLEVEL_VERBOSE, __FUNCTION__, y, x, ##__VA_ARGS__)
#define TRACE_DEBUG(y,x,...) Log(LOGLEVEL_DEBUG, __FUNCTION__, y, x, ##__VA_ARGS__)

//extern int g_logLevel;

extern "C"
{
	extern int g_logLevel;

	typedef void (logHandler)(int level, const char* msg);
	_declspec(dllexport) void InitializeTraceHelper(int level, logHandler* callback);
	_declspec(dllexport) void Log(int level, const char * functionName, int engineId, const char * lpszFormat, ...);
}
