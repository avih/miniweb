/////////////////////////////////////////////////////////////////////////////
//
// miniweb.c
//
// MiniWeb start-up code
//
/////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>
#include "httppil.h"
#include "httpapi.h"
#include "revision.h"

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

UrlHandler urlHandlerList[]={
	{"stats",uhStats,NULL},
#ifdef _7Z
	//{"7z",uh7Zip,NULL},
#endif
#ifdef _MPD
	{"mpd",uhMpd,ehMpd},
#endif
#ifdef _VOD
	{"vodstream",uhVodStream,NULL},
	{"vodlib",uhLib,0},
	{"vodplay",uhVod,ehVod},
	{"stream",uhStream,NULL},
#endif
	{NULL},
};

HttpParam *httpParam;
int nInst=0;

extern FILE *fpLog;


//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever it needs to substitute variables
//////////////////////////////////////////////////////////////////////////
int DefaultWebSubstCallback(SubstParam* sp)
{
	// the maximum length of variable value should never exceed the number
	// given by sp->iMaxValueBytes
	if (!strcmp(sp->pchParamName,"mykeyword")) {
		return sprintf(sp->pchParamValue, "%d", time(NULL));
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
int DefaultWebFileUploadCallback(HttpMultipart *pxMP, OCTET *poData, DWORD dwDataChunkSize)
{
  // Do nothing with the data
	int fd = (int)pxMP->pxCallBackData;
	if (!poData) {
		// to cleanup
		if (fd > 0) {
			close(fd);
			pxMP->pxCallBackData = NULL;
		}
		return 0;
	}
	if (!fd) {
		fd = open(pxMP->pchFilename, O_CREAT | O_TRUNC | O_RDWR | O_BINARY);
		pxMP->pxCallBackData = (void*)fd;
	}
	if (fd <= 0) return -1;
	write(fd, poData, dwDataChunkSize);
	if (pxMP->oFileuploadStatus & HTTPUPLOAD_LASTCHUNK) {
		close(fd);
		pxMP->pxCallBackData = NULL;
	}
	printf("Received %lu bytes for multipart upload file %s\n", dwDataChunkSize, pxMP->pchFilename);
	return 0;
}

void Shutdown()
{
	//shutdown server
	int i;
	for (i=0;i<nInst;i++) {
		printf("Shutting down instance %d\n",i);
		mwServerShutdown(&httpParam[i]);
	}
	fclose(fpLog);
	UninitSocket();
}

int MiniWebQuit(int arg) {
	static int quitting = 0;
	if (quitting) return 0;
	quitting = 1;
	if (arg) printf("\nCaught signal (%d). MiniWeb shutting down...\n",arg);
	Shutdown();
	return 0;
}

int main(int argc,char* argv[])
{
	printf("MiniWeb %d.%d.%d (C)2005-07 Written by Stanley Huang\n\n",VER_MAJOR,VER_MINOR,BUILD_NO);

#ifdef WIN32
	SetConsoleCtrlHandler( (PHANDLER_ROUTINE) MiniWebQuit, TRUE );
#else
	signal(SIGINT, (void *) MiniWebQuit);
	signal(SIGTERM, (void *) MiniWebQuit);
	signal(SIGPIPE, SIG_IGN);
#endif

#ifndef NOTHREAD
	//get the number of instances
	{
		int i;
		for (i=1;i<argc;i++) {
			if (!strcmp(argv[i],"-n")) nInst=atoi(argv[++i]);
		}
	}
#endif
	if (!nInst) nInst=1;
	//initialize HTTP parameter structure
	{
		int iParamBytes=nInst*sizeof(HttpParam);
		httpParam=malloc(iParamBytes);
		if (!httpParam) {
			printf("Out of memory\n");
			return -1;
		}
		memset(httpParam,0,iParamBytes);
	}
	//fill in default settings
	{
		int i;
		for (i=0;i<nInst;i++) {
			httpParam[i].maxClients=32;
			httpParam[i].maxReqPerConn=99;
			httpParam[i].pchWebPath="webroot";
			httpParam[i].pxUrlHandler=urlHandlerList;
			httpParam[i].flags=FLAG_DIR_LISTING;
#ifndef _NO_POST
			httpParam[i].pfnPost = DefaultWebPostCallback;
			httpParam[i].pfnFileUpload = DefaultWebFileUploadCallback;
#endif
		}
	}

	//parsing command line arguments
	{
		int inst=0;
		int i;
		for (i=1;i<argc;i++) {
			if (argv[i][0]=='-') {
				switch (argv[i][1]) {
				case 'i':
					if ((++i)<argc) {
						int n=atoi(argv[i]);
						if (n<nInst) inst=n;;
					}
					break;
				case 'p':
					if ((++i)<argc) (httpParam+inst)->httpPort=atoi(argv[i]);
					break;
				case 'r':
					if ((++i)<argc) (httpParam+inst)->pchWebPath=argv[i];
					break;
				case 'l':
					if ((++i)<argc) fpLog=freopen(argv[i],"w",stderr);
					break;
				case 'm':
					if ((++i)<argc) (httpParam+inst)->maxClients=atoi(argv[i]);
					break;
				case 'k':
					if ((++i)<argc) (httpParam+inst)->maxReqPerConn=atoi(argv[i]);
					break;
				case 'd':
					(httpParam+inst)->flags &= ~FLAG_DIR_LISTING;
					break;
				}
			}
		}
	}
	{
		int i;
		for (i = 0; urlHandlerList[i].pchUrlPrefix; i++) {
			if (urlHandlerList[i].pfnEventHandler)
				urlHandlerList[i].pfnEventHandler(MW_PARSE_ARGS, argc, argv);
		}
	}

	//adjust port setting
	{
		int i;
		short int port=80;
		for (i=0;i<nInst;i++) {
			if (httpParam[i].httpPort)
				port=httpParam[i].httpPort+1;
			else
				httpParam[i].httpPort=port++;
		}
	}

	InitSocket();

	if (nInst>1) printf("Number of instances: %d\n",nInst);
	{
		int i;
		int error=0;
		for (i=0;i<nInst;i++) {
			int n;
			if (nInst>1) printf("\nInstance %d\n",i);
			printf("Listening port: %d\n",httpParam[i].httpPort);
			printf("Web root: %s\n",httpParam[i].pchWebPath);
			printf("Max clients: %d\n",httpParam[i].maxClients);
			for (n=0;urlHandlerList[n].pchUrlPrefix;n++);
			printf("URL handlers: %d\n",n);
			if (httpParam[i].flags & FLAG_DIR_LISTING) printf("Dir listing: on\n");

			//register page variable substitution callback
			//httpParam[i].pfnSubst=DefaultWebSubstCallback;

			//start server
			if (mwServerStart(&httpParam[i])) {
				printf("Error starting instance #%d\n",i);
				error++;
			}
		}
	
		if (error<nInst) {
			#ifndef NOTHREAD
				ThreadWait(httpParam[0].tidHttpThread,NULL);
			#endif
		} else {
			printf("Failed to launch miniweb\n");
		}
	}

	Shutdown();
	return 0;
}
////////////////////////////// END OF FILE //////////////////////////////
