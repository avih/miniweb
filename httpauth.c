/////////////////////////////////////////////////////////////////////////////
//
// httpauth.c
//
// MiniWeb HTTP authentication implementation
// Copyright (c) 2005-2012 Stanley Huang <stanleyhuangyc@gmail.com>
//
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "httppil.h"
#include "httpapi.h"
#include "httpint.h"

#ifdef HTTPAUTH  

extern HttpParam g_httpParam;

////////////////////////////////////////////////////////////////////////////
// _mwCheckAuthentication
// Check if a connected peer is authenticated
////////////////////////////////////////////////////////////////////////////
BOOL _mwCheckAuthentication(HttpSocket* phsSocket)
{
	if (!ISFLAGSET(phsSocket,FLAG_AUTHENTICATION))
		return TRUE;
	if (g_httpParam.dwAuthenticatedNode!=phsSocket->ipAddr.laddr) {
		// Not authenticated
		g_httpParam.stats.authFailCount++;
		return FALSE;
	} 
    // Extend authentication period
    g_httpParam.tmAuthExpireTime = time(NULL) + HTTPAUTHTIMEOUT;
  return TRUE;
}

#endif
