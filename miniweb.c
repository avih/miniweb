/////////////////////////////////////////////////////////////////////////////
//
// miniweb.c
//
// MiniWeb start-up code
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include "httppil.h"
#include "httpapi.h"
#include "revision.h"
#ifdef MEDIA_SERVER
#include "mediaserver.h"
#endif
#include "win32/win_compat.h"

#define APP_NAME "MiniWeb-avih"

int uhMpd(UrlHandlerParam* param);
int ehMpd(MW_EVENT msg, int argi, void* argp);
int uhStats(UrlHandlerParam* param);
int uhVod(UrlHandlerParam* param);
int uhLib(UrlHandlerParam* param);
int uhVodStream(UrlHandlerParam* param);
int uhStream(UrlHandlerParam* param);
int ehVod(MW_EVENT msg, int argi, void* argp);
int uhTest(UrlHandlerParam* param);
int uh7Zip(UrlHandlerParam* param);
int uhFileStream(UrlHandlerParam* param);
int uhAsyncDataTest(UrlHandlerParam* param);
int uhRTSP(UrlHandlerParam* param);
int uhSerial(UrlHandlerParam* param);

UrlHandler urlHandlerList[]={
	{"stats", uhStats, NULL},
#ifdef ENABLE_SERIAL
	{"serial", uhSerial, NULL},
#endif
#ifdef HAVE_THREAD
	{"async", uhAsyncDataTest, NULL},
#endif
#ifdef MEDIA_SERVER
	{"test.sdp", uhRTSP, NULL},
	{"MediaServer/VideoItems/", uhMediaItemsTranscode, ehMediaItemsEvent},
#endif
#ifdef _7Z
	{"7z", uh7Zip, NULL},
#endif
#ifdef _MPD
	{"mpd", uhMpd, ehMpd},
#endif
#ifdef _VOD
	{"vodstream", uhVodStream,NULL},
	{"vodlib", uhLib,0},
	{"vodplay", uhVod,ehVod},
	{"stream", uhStream,NULL},
#endif
	{NULL},
};

#ifndef DISABLE_BASIC_WWWAUTH
AuthHandler authHandlerList[]={
	{"stats", "user", "pass", "group=admin", ""},
	{NULL}
};
#endif

HttpParam httpParam;

extern FILE *fpLog;


//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever it needs to substitute variables
//////////////////////////////////////////////////////////////////////////
int DefaultWebSubstCallback(SubstParam* sp)
{
	// the maximum length of variable value should never exceed the number
	// given by sp->iMaxValueBytes
	if (!strcmp(sp->pchParamName,"mykeyword")) {
		return sprintf(sp->pchParamValue, "%d", 1234);
	}
	return -1;
}

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever it recevies posted data
//////////////////////////////////////////////////////////////////////////
int DefaultWebPostCallback(PostParam* pp)
{
  int iReturn=WEBPOST_OK;

  // by default redirect to config page
  //strcpy(pp->chFilename,"index.htm");

  return iReturn;
}

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever it receives a multipart
// upload file chunk
//////////////////////////////////////////////////////////////////////////
int DefaultWebFileUploadCallback(HttpMultipart *pxMP, OCTET *poData, size_t dwDataChunkSize)
{
  // Do nothing with the data
	int *fd = &pxMP->fd;
	if (!poData) {
		// to cleanup
		if (*fd > 0) {
			close(*fd);
			*fd = 0;
		}
		return 0;
	}
	if (!*fd) {
		char filename[256];
		snprintf(filename, sizeof(filename), "%s/%s", httpParam.pchWebPath, pxMP->pchFilename);
		*fd = open(filename, O_CREAT | O_TRUNC | O_RDWR | O_BINARY, 0);
	}
	if (*fd <= 0) return -1;
	if (write(pxMP->fd, poData, dwDataChunkSize) < 0) {
		close(*fd);
		*fd = -1;
		return -1;
	}
	if (pxMP->oFileuploadStatus & HTTPUPLOAD_LASTCHUNK) {
		close(*fd);
		*fd = 0;
	}
	printf("Received %u bytes for multipart upload file %s\n", (unsigned int)dwDataChunkSize, pxMP->pchFilename);
	return 0;
}

int Shutdown(mwShutdownCallback cb, unsigned int timeout)
{
	//shutdown server
	int rv = mwServerShutdown(&httpParam, cb, timeout);
	fclose(fpLog);
	UninitSocket();
	return rv;
}

char* GetLocalAddrString()
{
	// get local ip address
	struct sockaddr_in sock;

	char hostname[128];
	struct hostent * lpHost;
	gethostname(hostname, 128);
	lpHost = gethostbyname(hostname);
	memcpy(&(sock.sin_addr), (void*)lpHost->h_addr_list[0], lpHost->h_length);
	return inet_ntoa(sock.sin_addr);
}

#ifdef WIN32  /* Windows - Console control handler on ctrl-c (new thread) */

BOOL MiniWebQuit(DWORD arg) {
	static int quitting = 0;
	if (quitting) return 1;  // shouldn't reenter on windows. regardless, already being handled
	quitting = 1;
	printf("\nCaught control signal (%d). Shutting down...\n", (int)arg);

	// Shutdown() runs in the handler thread, waits for the server (main
	// thread) to finish - up to timeout. Returns TRUE if stop succeeded.
	if (Shutdown(0, 5000)) {
	  printf("Cannot shut down the server, killing it instead...\n");
	  return 0;  // couldn't kill the server, let the system kill us.
	}
	// success, the program continues to finish main, don't let the system kill us.
	// possibly we never get here - if main finishes before Shutdown returns.
	return 1;
}

#else  /* *nix - signal handler - main thread */

void onShutdown()
{
  // Good thing we're lucky. Now main can finish.
}

void MiniWebQuit(int arg) {
	static int quitting = 0;
	if (quitting) return;
	quitting = 1;
	printf("\nCaught signal (%d), attempting to shut down...\n", arg);
	Shutdown(onShutdown, 0);  // tell the server to shutdown but don't wait for it.
	// since the server runs on the main thread, not much we can do in
	// terms of using a timeout since main thread is blocked as long as
	// we're here. If it doesn't exit, the user should kill it manually.
	// Empirically, it always seem to exit correctly and quickly.
	// This is bad design, but for now that's what we have.
}

#endif

void GetFullPath(char* buffer, char* argv0, char* path)
{
	char* p = strrchr(argv0, '/');
	if (!p) p = strrchr(argv0, '\\');
	if (!p) {
		strcpy(buffer, path);
	} else {
		int l = p - argv0 + 1;
		memcpy(buffer, argv0, l);
		strcpy(buffer + l, path);
	}
}

int main(int argc,char* argv[])
{
	int needs_argv_free = 0;
	argv = cc_get_argvutf8(argc, argv, &needs_argv_free);
	fprintf(stderr,"%s https://github.com/avih/miniweb (built on %s)\n"
	               "Originally: (C)2005-2013 Written by Stanley Huang <stanleyhuangyc@gmail.com>\n\n",
	               APP_NAME, __DATE__);

#ifdef WIN32
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) MiniWebQuit, TRUE );
#else
	signal(SIGINT, MiniWebQuit);
	signal(SIGTERM, MiniWebQuit);
	signal(SIGPIPE, SIG_IGN);
#endif

	//fill in default settings
	mwInitParam(&httpParam);
	httpParam.maxClients=32;
	httpParam.httpPort = 80;
	GetFullPath(httpParam.pchWebPath, argv[0], "htdocs");
#ifndef DISABLE_BASIC_WWWAUTH
	httpParam.pxAuthHandler = authHandlerList;
#endif
	httpParam.pxUrlHandler=urlHandlerList;
	httpParam.flags=FLAG_DIR_LISTING;
	httpParam.tmSocketExpireTime = 15;
	httpParam.pfnPost = DefaultWebPostCallback;
#ifdef MEDIA_SERVER
	httpParam.pfnFileUpload = TranscodeUploadCallback;
#else
	httpParam.pfnFileUpload = DefaultWebFileUploadCallback;
#endif

	const char *ifcarg = 0;

	//parsing command line arguments
	{
		int i;
		for (i=1;i<argc;i++) {
			if (argv[i][0]=='-') {
				switch (argv[i][1]) {
				case 'h':
					fprintf(stderr,"Usage: miniweb	-h	: display this help screen\n"
						       "		-v	: log status/error info\n"
						       "		-p	: specifiy http port [default 80]\n"
						       "		-i	: interface [default 0.0.0.0]\n"
						       "		-r	: specify http document directory [default htdocs]\n"
						       "		-l	: specify log file\n"
						       "		-m	: specifiy max clients [default 32]\n"
						       "		-M	: specifiy max clients per IP\n"
						       "		-s	: specifiy download speed limit in KB/s [default: none]\n"
						       "		-n	: disallow multi-part download [default: allow]\n"
						       "		-d	: disallow directory listing [default ON]\n\n"
						);
					fflush(stderr);
                                        exit(1);

				case 'p':
					if ((++i)<argc) httpParam.httpPort=atoi(argv[i]);
					break;
				case 'i':
					if ((++i)<argc) httpParam.hlBindIP = inet_addr(argv[i]);
					if (httpParam.hlBindIP) ifcarg = argv[i];
					break;
				case 'r':
					if ((++i)<argc) strncpy(httpParam.pchWebPath, argv[i], sizeof(httpParam.pchWebPath) - 1);
					break;
				case 'l':
					if ((++i)<argc) fpLog=freopen(argv[i],"w",stderr);
					break;
				case 'm':
					if ((++i)<argc) httpParam.maxClients=atoi(argv[i]);
					break;
				case 'M':
					if ((++i)<argc) httpParam.maxClientsPerIP=atoi(argv[i]);
					break;
				case 's':
					if ((++i)<argc) httpParam.maxDownloadSpeed=atoi(argv[i]);
					break;
				case 'n':
					httpParam.flags |= FLAG_DISABLE_RANGE;
					break;
				case 'd':
					httpParam.flags &= ~FLAG_DIR_LISTING;
					break;
				}
			}
		}
	}
	{
		int i;
		int error = 0;
		for (i = 0; urlHandlerList[i].pchUrlPrefix; i++) {
			if (urlHandlerList[i].pfnEventHandler) {
				if (urlHandlerList[i].pfnEventHandler(MW_PARSE_ARGS, urlHandlerList[i].pfnEventHandler, &httpParam))
					error++;
			}
		}
		if (error > 0) {
			printf("Error parsing command line options\n");
			if (needs_argv_free) cc_free_argvutf8(argc, argv);
			return -1;
		}
	}

	InitSocket();

	{
		int n;
		printf("Host: %s:%d\n", (ifcarg ? ifcarg : GetLocalAddrString()), httpParam.httpPort);
		printf("Web root: %s\n",httpParam.pchWebPath);
		printf("Max clients (per IP): %d (%d)\n",httpParam.maxClients, httpParam.maxClientsPerIP);
		for (n=0;urlHandlerList[n].pchUrlPrefix;n++);
		printf("URL handlers: %d\n",n);
		if (httpParam.flags & FLAG_DIR_LISTING) printf("Dir listing enabled\n");
		if (httpParam.flags & FLAG_DISABLE_RANGE) printf("Byte-range disabled\n");

		//register page variable substitution callback
		//httpParam[i].pfnSubst=DefaultWebSubstCallback;

		//start server
		if (mwServerStart(&httpParam)) {
			printf("Error starting HTTP server\n");
			Shutdown(0, 0);  // the server is not running but for the socket/log file.
		} else {
			mwHttpLoop(&httpParam);
			printf("Shutdown complete\n");
		}
	}

	// No need for Shutdown() here since it must have already happened:
	// the only way for mwHttpLoop to exit is if hp->bKillWebserver, and
	// it's set only from mwServerShutdown, and only Shutdown calls it.

	if (needs_argv_free) cc_free_argvutf8(argc, argv);
	return 0;
}
////////////////////////////// END OF FILE //////////////////////////////
