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
int ehMpd(MW_EVENT msg, void* arg);
int uhStats(UrlHandlerParam* param);
int uhWebCounter(UrlHandlerParam* param);
int uhVod(UrlHandlerParam* param);
int uhVodStream(UrlHandlerParam* param);
int ehVod(MW_EVENT msg, void* arg);
int uhTest(UrlHandlerParam* param);

UrlHandler urlHandlerList[]={
	{"~stats",uhStats,NULL},
#ifdef _MPD
	{"mplayer/",uhMpd,ehMpd},
#endif
#ifdef _VOD
	{"vod/",uhVod,ehVod},
	{"vodstream.avi",uhVodStream,NULL},
#endif
	{NULL},
	{NULL},
};

HttpParam *httpParam;
int nInst=0;

const char *pageHead="<html><body class='body'><table border=1 cellpadding=0 cellspacing=0 width=280 class='body'>";
const char *pageCellBegin="<tr><td width=140>%s</td><td width=140>";
const char *pageCellEnd="</td></tr>";
const char *pageTail="</body></html>";

extern FILE *fpLog;

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever a valid request comes in
//////////////////////////////////////////////////////////////////////////
int uhStats(UrlHandlerParam* param)
{
	unsigned char *p;
	char buf[30];
	HttpStats *stats=&((HttpParam*)param->hp)->stats;
	HttpRequest *req=&param->hs->request;
	int ret=FLAG_DATA_RAW;

	mwGetHttpDateTime(time(NULL),buf);

	if (stats->clientCount>4) {
		param->pucBuffer=(char*)malloc(stats->clientCount*256+1024);
		ret=FLAG_DATA_RAW | FLAG_TO_FREE;
	}
		
	p=param->pucBuffer;
	//generate page
	p+=sprintf(p,pageHead);

	p+=sprintf(p,pageCellBegin,"Your IP address:");
	p+=sprintf(p,"%d.%d.%d.%d%s",
		req->ipAddr.caddr[3],
		req->ipAddr.caddr[2],
		req->ipAddr.caddr[1],
		req->ipAddr.caddr[0],
		pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Current time:");
	p+=sprintf(p,"%s%s",buf,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Server uptime:",time(NULL)-stats->startTime);
	p+=sprintf(p,"%d sec(s)%s",time(NULL)-stats->startTime,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Connected clients:");
	p+=sprintf(p,"%d%s",stats->clientCount,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Maximum clients:");
	p+=sprintf(p,"%d%s",stats->clientCountMax,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Requests:");
	p+=sprintf(p,"%d%s",stats->reqCount,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Files sent:");
	p+=sprintf(p,"%d%s",stats->fileSentCount,pageCellEnd);

	p+=sprintf(p,pageCellBegin,"Bytes sent:");
	p+=sprintf(p,"%d bytes%s</table>",stats->fileSentBytes,pageCellEnd);

	p+=sprintf(p,"<br>Connected peers:<hr>");
	{
		HttpSocket *phsSocketCur;
		time_t curtime=time(NULL);
		for (phsSocketCur=((HttpParam*)param->hp)->phsSocketHead; phsSocketCur; phsSocketCur=phsSocketCur->next) {
			IP ip=phsSocketCur->request.ipAddr;
			p+=sprintf(p,"<br>Socket %d: %d.%d.%d.%d / Reqs: %d / Expire in %d ",
				phsSocketCur->socket,ip.caddr[3],ip.caddr[2],ip.caddr[1],ip.caddr[0],phsSocketCur->iRequestCount,phsSocketCur->tmExpirationTime-curtime);
			if (phsSocketCur->request.pucPath)
				p+=sprintf(p,"(%d/%d)",phsSocketCur->response.iSentBytes,phsSocketCur->response.iContentLength);
			else
				p+=sprintf(p,"(idle)");
		}
	}

	p+=sprintf(p,pageTail);

	//return data to server
	param->iDataBytes=(int)p-(int)(param->pucBuffer);
	param->fileType=HTTPFILETYPE_HTML;
	return ret;
}

#define TOTAL_COUNTERS 8
static unsigned long counter[TOTAL_COUNTERS];
char *pchCounterFile=NULL;

void saveWebCounter()
{
	int fd;
	if (!pchCounterFile) return;
	printf("Saving counter data...\n");
	fd=open(pchCounterFile,O_WRONLY|O_CREAT);
	if (fd<=0) return;
	write(fd,counter,sizeof(counter));
	close(fd);
}

int loadWebCounter()
{
	int fd;
	memset(&counter,0,sizeof(counter));
	if (!pchCounterFile) return -1;
	fd=open(pchCounterFile,O_RDONLY);
	if (fd<=0) {
		return -1;
	}
	read(fd,counter,sizeof(counter));
	close(fd);
	return 0;
}

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

int uhWebCounter(UrlHandlerParam* param)
{
	static int visitCount=0;
	int idx,mode=0;
	char *p;
	
	p=strchr(param->pucRequest,'&');
	if (p) {
		*(p++)=0;
		if (GETDWORD(p)==DEFDWORD('m','o','d','e')) {
			mode=atoi(p+5);
		}
	}
	p=strchr(param->pucRequest,'=');
	if (p) *p=0;
	idx=atoi(param->pucRequest);
	if (idx>=TOTAL_COUNTERS) return 0;
	counter[idx]++;
	if (p) counter[idx]=atoi(p+1);
	p=param->pucBuffer;
	if ((mode & 4)==0) {
		p+=sprintf(p,"document.write('");
	}
	switch (mode & 3) {
	case 0:
		p+=sprintf(p,"%d",counter[idx]);
		break;
	case 1:
		p+=itoc(counter[idx],p,0);
		break;
	case 2:
		p+=itoc(counter[idx],p,1);
		break;
	}
	if ((mode & 4)==0) {
		p+=sprintf(p,"');");
	}
	param->iDataBytes=(int)p-(int)param->pucBuffer;

	param->fileType=HTTPFILETYPE_TEXT;
	if (((++visitCount) & 0xf)==0) {
		saveWebCounter();
	}
	return FLAG_DATA_RAW;
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

int MiniWebQuit(int arg) {
	int i;
	if (arg) printf("\nCaught signal (%d). MiniWeb shutting down...\n",arg);
	for (i=0;i<nInst;i++) {
		(httpParam+i)->bKillWebserver=1;
	}
	return 0;
}

int main(int argc,char* argv[])
{
	printf("MiniWeb %d.%d.%d (C)2005 Written by Stanley Huang\n\n",VER_MAJOR,VER_MINOR,BUILD_NO);

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
				case 'c':
					if ((i<argc-1 && argv[i+1][0]!='-'))	
						pchCounterFile=argv[++i];
					else
						pchCounterFile="counters.dat";
					break;
				case 'd':
					(httpParam+inst)->flags &= ~FLAG_DIR_LISTING;
					break;
				}
			}
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

	if (pchCounterFile) {
		int i;
		//load counter values from file
		loadWebCounter();
		printf("Counter enabled (%s)\n",pchCounterFile);
		for (i=0;urlHandlerList[i].pchUrlPrefix;i++);
		urlHandlerList[i].pchUrlPrefix="counter/";
		urlHandlerList[i].pfnUrlHandler=&uhWebCounter;
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

	//shutdown server
	{
		saveWebCounter();
		if (nInst>1) {
			int i;
			for (i=0;i<nInst;i++) {
				printf("Shutting down instance %d\n",i);
				mwServerShutdown(&httpParam[i]);
			}
		} else {
			mwServerShutdown(&httpParam[0]);
		}
		fclose(fpLog);
	}
	UninitSocket();
	return 0;
}
////////////////////////////// END OF FILE //////////////////////////////
