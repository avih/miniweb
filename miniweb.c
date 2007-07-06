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
#ifdef _7Z
#include "7zDec/7zInc.h"
#endif
#include "httpxml.h"

int uhMpd(UrlHandlerParam* param);
int ehMpd(MW_EVENT msg, int argi, void* argp);
int uhStats(UrlHandlerParam* param);
int uhVod(UrlHandlerParam* param);
int uhLib(UrlHandlerParam* param);
int uhVodStream(UrlHandlerParam* param);
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
#endif
	{NULL},
};

HttpParam *httpParam;
int nInst=0;

extern FILE *fpLog;

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever a valid request comes in
//////////////////////////////////////////////////////////////////////////
int uhStats(UrlHandlerParam* param)
{
	char *p;
	char buf[128];
	HttpStats *stats=&((HttpParam*)param->hp)->stats;
	HttpRequest *req=&param->hs->request;
	IP ip = param->hs->ipAddr;
	HTTP_XML_NODE node;
	int bufsize = param->iDataBytes;
	int ret=FLAG_DATA_RAW;

	mwGetHttpDateTime(time(NULL),buf);

	if (stats->clientCount>4) {
		param->pucBuffer=(char*)malloc(stats->clientCount*256+1024);
		ret=FLAG_DATA_RAW | FLAG_TO_FREE;
	}
		
	p=param->pucBuffer;
	
	//generate XML
	mwWriteXmlHeader(&p, &bufsize, 10, 0, 0);

	mwWriteXmlString(&p, &bufsize, 0, "<ServerStats>");

	sprintf(buf, "%d.%d.%d.%d", ip.caddr[3], ip.caddr[2], ip.caddr[1], ip.caddr[0]);

	node.indent = 1;
	node.fmt = "%s";
	node.name = "ClientIP";
	node.value = buf;
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	node.fmt = "%d";
	node.name = "UpTime";
	node.value = (void*)(time(NULL)-stats->startTime);
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	node.fmt = "%d";
	node.name = "MaxClients";
	node.value = (void*)(stats->clientCountMax);
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	node.fmt = "%d";
	node.name = "Requests";
	node.value = (void*)(stats->reqCount);
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	node.fmt = "%d";
	node.name = "FileSent";
	node.value = (void*)(stats->fileSentCount);
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	node.fmt = "%d";
	node.name = "ByteSent";
	node.value = (void*)(stats->fileSentBytes);
	mwWriteXmlLine(&p, &bufsize, &node, 0);

	mwWriteXmlString(&p, &bufsize, 1, "<Clients>");

	{
		HttpSocket *phsSocketCur;
		time_t curtime=time(NULL);
		for (phsSocketCur=((HttpParam*)param->hp)->phsSocketHead; phsSocketCur; phsSocketCur=phsSocketCur->next) {
			ip=phsSocketCur->ipAddr;
			sprintf(buf,"<Client ip=\"%d.%d.%d.%d\" requests=\"%d\" expire=\"%d\"/>",
				ip.caddr[3],ip.caddr[2],ip.caddr[1],ip.caddr[0],phsSocketCur->iRequestCount,phsSocketCur->tmExpirationTime-curtime);
			mwWriteXmlString(&p, &bufsize, 2, buf);
			/*
			if (phsSocketCur->request.pucPath)
				p+=sprintf(p,"(%d/%d)",phsSocketCur->response.iSentBytes,phsSocketCur->response.iContentLength);
			else
				p+=sprintf(p,"(idle)");
			*/
		}
	}
	
	mwWriteXmlString(&p, &bufsize, 1, "</Clients>");
	mwWriteXmlString(&p, &bufsize, 0, "</ServerStats>");

	//return data to server
	param->iDataBytes=(int)p-(int)(param->pucBuffer);
	param->fileType=HTTPFILETYPE_XML;
	return ret;
}

#ifdef _7Z

int uh7Zip(UrlHandlerParam* param)
{
	HttpRequest *req=&param->hs->request;
	HttpParam *hp= (HttpParam*)param->hp;
	char *path;
	char *filename;
	void *content;
	int len;
	char *p = strchr(req->pucPath, '/');
	if (p) p = strchr(p + 1, '/');
	if (!p) return 0;
	filename = p + 1;
	*p = 0;
	path = (char*)malloc(strlen(req->pucPath) + strlen(hp->pchWebPath) + 5);
	sprintf(path, "%s/%s.7z", hp->pchWebPath, req->pucPath);
	*p = '/';
	
	if (!IsFileExist(path)) {
		free(path);
		return 0;
	}
	
	len = SzExtractContent(hp->szctx, path, filename, &content);
	free(path);
	if (len < 0) return 0;

	p = strrchr(filename, '.');
	param->fileType = p ? mwGetContentType(p + 1) : HTTPFILETYPE_OCTET;
	param->iDataBytes = len;
	param->pucBuffer = content;
	return FLAG_DATA_RAW;
}

#endif

int itoc(int num, char *pbuf, int type)
{
	static const char *chNum[]={"零","一","二","三","四","五","六","七","八","九"};
	static const char *chUnit[]={"亿","万","千","百","十","",NULL};
	char *p=pbuf;
	int c=1000000000,unit=4,d,last=0;
	if (num==0) return sprintf(pbuf,chNum[0]);
	if (num<0) {
		p+=sprintf(pbuf,"负");
		num=-num;
	}
	d=num;
	for (;;) {
		do {
			int tmp=d/c;
			if (tmp>0) {
				p+=sprintf(p,"%s%s",(unit==2 && tmp==1)?"":chNum[tmp],chUnit[unit]);
				d%=c;
			} else if (last!=0 && c>=10 && d>0) {
				p+=sprintf(p,chNum[0]);
			}
			last=tmp;
			c/=10;
		} while(chUnit[++unit]);
		if (c==0) break;
		if (c==1000 && num>=10000)
			p+=sprintf(p,chUnit[1]);
		else if (c==10000000 && num>=100000000)
			p+=sprintf(p,chUnit[0]);
		unit=2;
	}
	return (int)(p-pbuf);
}

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
  strcpy(pp->chFilename,"index.htm");

  return iReturn;
}

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever it receives a multipart 
// upload file chunk
//////////////////////////////////////////////////////////////////////////
int DefaultWebFileUploadCallback(char *pchFilename,
                                 OCTET oFileuploadStatus,
                                 OCTET *poData,
                                 DWORD dwDataChunkSize)
{
  // Do nothing with the data
  printf("Received %lu bytes for multipart upload file %s\n",
               dwDataChunkSize, pchFilename);
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
			(httpParam+i)->maxClients=32;
			(httpParam+i)->maxReqPerConn=99;
			(httpParam+i)->pchWebPath="webroot";
			(httpParam+i)->pxUrlHandler=urlHandlerList;
			(httpParam+i)->flags=FLAG_DIR_LISTING;
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
			if ((httpParam+i)->httpPort)
				port=(httpParam+i)->httpPort+1;
			else
				(httpParam+i)->httpPort=port++;
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
			printf("Listening port: %d\n",(httpParam+i)->httpPort);
			printf("Web root: %s\n",(httpParam+i)->pchWebPath);
			printf("Max clients: %d\n",(httpParam+i)->maxClients);
			for (n=0;urlHandlerList[n].pchUrlPrefix;n++);
			printf("URL handlers: %d\n",n);
			if ((httpParam+i)->flags & FLAG_DIR_LISTING) printf("Dir listing: on\n");

			//register page variable substitution callback
			//(httpParam+i)->pfnSubst=DefaultWebSubstCallback;

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
