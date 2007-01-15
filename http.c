/////////////////////////////////////////////////////////////////////////////
//
// http.c
//
// MiniWeb - mini webserver implementation
//
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "httppil.h"
#include "httpapi.h"
#include "httpint.h"

////////////////////////////////////////////////////////////////////////////
// global variables
////////////////////////////////////////////////////////////////////////////
// default pages
const char g_chPasswordPage[]="password.htm";

char* contentTypeTable[]={
	HTTPTYPE_OCTET,HTTPTYPE_HTML,HTTPTYPE_XML,HTTPTYPE_TEXT,HTTPTYPE_CSS,HTTPTYPE_PNG,HTTPTYPE_JPEG,HTTPTYPE_GIF,HTTPTYPE_SWF,
	HTTPTYPE_MPA,HTTPTYPE_MPEG,HTTPTYPE_AVI,HTTPTYPE_QUICKTIME,HTTPTYPE_QUICKTIME,HTTPTYPE_JS,HTTPTYPE_OCTET,HTTPTYPE_STREAM
};

char* defaultPages[]={"index.htm","index.html","default.htm",NULL};

FILE *fpLog=NULL;

#define LOG_INFO fpLog

////////////////////////////////////////////////////////////////////////////
// API callsc
////////////////////////////////////////////////////////////////////////////

const char *dayNames="Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat";
const char *monthNames="Jan\0Feb\0Mar\0Apr\0May\0Jun\0Jul\0Aug\0Sep\0Oct\0Nov\0Dec";
const char *httpDateTimeFormat="%s, %02d %s %02d %02d:%02d:%02d GMT";

char* mwGetVarValue(HttpVariables* vars, char *varname)
{
	int i;
	if (vars && varname) {
		for (i=0; (vars+i)->name; i++) {
			if (!strcmp((vars+i)->name,varname))
				return (vars+i)->value;
		}
	}
	return NULL;
}

int mwGetVarValueInt(HttpVariables* vars, char *varname, int defval)
{
	int i;
	if (vars && varname) {
		for (i=0; (vars+i)->name; i++) {
			if (!strcmp((vars+i)->name,varname)) {
				char *p = (vars+i)->value;
				return p ? atoi(p) : defval;
			}
		}
	}
	return defval;
}

int mwGetHttpDateTime(time_t timer, char *buf)
{
	struct tm *btm;
	btm=gmtime(&timer);
	return sprintf(buf,httpDateTimeFormat,
		dayNames+(btm->tm_wday<<2),
		btm->tm_mday,
		monthNames+(btm->tm_mon<<2),
		1900+btm->tm_year,
		btm->tm_hour,
		btm->tm_min,
		btm->tm_sec);
}

////////////////////////////////////////////////////////////////////////////
// mwServerStart
// Start the webserver
////////////////////////////////////////////////////////////////////////////
int mwServerStart(HttpParam* hp)
{
	if (hp->bWebserverRunning) {
		DBG("Error: Webserver thread already running\n");
		return -1;
	}
	if (!fpLog) fpLog=stderr;
	if (!InitSocket()) {
		DBG("Error initializing Winsock\n");
		return -1;
	}

	{
		int i;
		for (i=0;(hp->pxUrlHandler+i)->pchUrlPrefix;i++) {
			if ((hp->pxUrlHandler+i)->pfnEventHandler &&
				(hp->pxUrlHandler+i)->pfnEventHandler(MW_INIT, hp)) {
				//remove the URL handler
				(hp->pxUrlHandler+i)->pfnUrlHandler=NULL;
			}
		}
	}
/*
#ifdef HTTPPOST
	if (!hp->pfnPost)
		hp->pfnPost=DefaultWebPostCallback;
	if (!hp->pfnFileUpload)
		hp->pfnFileUpload=DefaultWebFileUploadCallback;
#endif
*/

	if (!(hp->listenSocket=_mwStartListening(hp))) return -1;

	hp->stats.startTime=time(NULL);
	hp->bKillWebserver=FALSE;
	hp->bWebserverRunning=TRUE;

#ifndef NOTHREAD
	if (ThreadCreate(&hp->tidHttpThread,_mwHttpThread,(void*)hp)) {
		DBG("Error creating server thread\n");
		return -1;
	}
#else
	_mwHttpThread((void*)hp);
#endif

	return 0;
}

////////////////////////////////////////////////////////////////////////////
// mwServerShutdown
// Shutdown the webserver
////////////////////////////////////////////////////////////////////////////
int mwServerShutdown(HttpParam* hp)
{
	int i;

	DBG("Shutting down web server thread\n");
	// signal webserver thread to quit
	hp->bKillWebserver=TRUE;
  
	// and wait for thread to exit
	for (i=0;hp->bWebserverRunning && i<10;i++) msleep(100);

	for (i=0;(hp->pxUrlHandler+i)->pchUrlPrefix;i++) {
		if ((hp->pxUrlHandler+i)->pfnUrlHandler && (hp->pxUrlHandler+i)->pfnEventHandler)
			(hp->pxUrlHandler+i)->pfnEventHandler(MW_UNINIT, hp);
	}

	UninitSocket();
	DBG("Webserver shutdown complete\n");
	return 0;
}

int mwGetLocalFileName(HttpFilePath* hfp)
{
	char ch;
	char *p=hfp->cFilePath,*s=hfp->pchHttpPath,*upLevel=NULL;

	hfp->pchExt=NULL;
	hfp->fTailSlash=0;
	if (hfp->pchRootPath) {
		p+=_mwStrCopy(hfp->cFilePath,hfp->pchRootPath);
		if (*(p-1)!=SLASH) {
			*p=SLASH;
			*(++p)=0;
		}
	}
	while ((ch=*s) && ch!='?' && (int)p-(int)hfp->cFilePath<sizeof(hfp->cFilePath)-1) {
		if (ch=='%') {
			*(p++) = _mwDecodeCharacter(++s);
			s += 2;
		} else if (ch=='/') {
			*p=SLASH;
			upLevel=(++p);
			while (*(++s)=='/');
			continue;
		} else if (ch=='+') {
			*(p++)=' ';
			s++;
		} else if (ch=='.') {
			if (upLevel && GETWORD(s+1)==DEFWORD('.','/')) {
				s+=2;
				p=upLevel;
			} else {
				*(p++)='.';
				hfp->pchExt=p;
				while (*(++s)=='.');	//avoid '..' appearing in filename for security issue
			}
		} else {
			*(p++)=*(s++);
		}
	}
	if (*(p-1)==SLASH) {
		p--;
		hfp->fTailSlash=1;
	}
	*p=0;
	return (int)p-(int)hfp->cFilePath;
}

////////////////////////////////////////////////////////////////////////////
// Internal (private) helper functions
////////////////////////////////////////////////////////////////////////////

SOCKET _mwStartListening(HttpParam* hp)
{
	SOCKET listenSocket;
	int iRc;

    // create a new socket
    listenSocket=socket(AF_INET,SOCK_STREAM,0);
    if (listenSocket<0) return 0;

#if 0
    // allow reuse of port number
    {
      int iOptVal=1;
      iRc=setsockopt(listenSocket,SOL_SOCKET,SO_REUSEADDR,
                     (char*)&iOptVal,sizeof(iOptVal));
      if (iRc<0) return 0;
    }
#endif

    // bind it to the http port
    {
      struct sockaddr_in sinAddress;
      memset(&sinAddress,0,sizeof(struct sockaddr_in));
      sinAddress.sin_family=AF_INET;
	  sinAddress.sin_addr.s_addr=htonl((hp->flags & FLAG_LOCAL_BIND) ? 0x7f000001 : INADDR_ANY);
      sinAddress.sin_port=htons(hp->httpPort); // http port
      iRc=bind(listenSocket,(struct sockaddr*)&sinAddress,
               sizeof(struct sockaddr_in));
	  if (iRc<0) {
		DBG("Error binding on port %d\n",hp->httpPort);
		return 0;
	  }
    }

#ifndef WIN32
    // set to non-blocking to avoid lockout issue (see Section 15.6
    // in Unix network programming book)
    {
      int iSockFlags;
      iSockFlags = fcntl(listenSocket, F_GETFL, 0);
      iSockFlags |= O_NONBLOCK;
      iRc = fcntl(listenSocket, F_SETFL, iSockFlags);
    }
#endif

    // listen on the socket for incoming calls
	if (listen(listenSocket,hp->maxClients-1)) {
		DBG("Unable to listen on socket %d\n",listenSocket);
		return 0;
	}

    DBG("Http listen socket %d opened\n",listenSocket);
	return listenSocket;
}

void _mwInitSocketData(HttpSocket *phsSocket)
{
	memset(&phsSocket->response,0,sizeof(HttpResponse));
	memset(&phsSocket->request,0,sizeof(HttpRequest));
	phsSocket->fd=0;
	phsSocket->flags=FLAG_RECEIVING;
	phsSocket->pucData=phsSocket->buffer;
	phsSocket->iDataLength=0;
	phsSocket->response.iBufferSize=HTTP_BUFFER_SIZE;
	phsSocket->ptr=NULL;
}

////////////////////////////////////////////////////////////////////////////
// _mwHttpThread
// Webserver independant processing thread. Handles all connections
////////////////////////////////////////////////////////////////////////////
void* _mwHttpThread(HttpParam *hp)
{ 
	HttpSocket *phsSocketCur;
	SOCKET socket;
	struct sockaddr_in sinaddr;
    int iRc;

  // main processing loop
	while (!hp->bKillWebserver) {
		time_t tmCurrentTime;
		SOCKET iSelectMaxFds;
		fd_set fdsSelectRead;
		fd_set fdsSelectWrite;

		// clear descriptor sets
		FD_ZERO(&fdsSelectRead);
		FD_ZERO(&fdsSelectWrite);
		FD_SET(hp->listenSocket,&fdsSelectRead);
		iSelectMaxFds=hp->listenSocket;

		// get current time
		tmCurrentTime=time(NULL);  
		// build descriptor sets and close timed out sockets
		for (phsSocketCur=hp->phsSocketHead; phsSocketCur; phsSocketCur=phsSocketCur->next) {
			int iError=0;
			int iOptSize=sizeof(int);
			// get socket fd
			socket=phsSocketCur->socket;
			if (!socket) continue;
			if (getsockopt(socket,SOL_SOCKET,SO_ERROR,(char*)&iError,&iOptSize)) {
				// if a socket contains a error, close it
				SYSLOG(LOG_INFO,"[%d] Socket no longer vaild.\n",socket);
				phsSocketCur->flags=FLAG_CONN_CLOSE;
				_mwCloseSocket(hp, phsSocketCur);
				continue;
			}
			// check expiration timer (for non-listening, in-use sockets)
			if (tmCurrentTime > phsSocketCur->tmExpirationTime) {
				SYSLOG(LOG_INFO,"[%d] Http socket expired\n",phsSocketCur->socket);
				hp->stats.timeOutCount++;
				// close connection
				phsSocketCur->flags=FLAG_CONN_CLOSE;
				_mwCloseSocket(hp, phsSocketCur);
			} else {
				if (ISFLAGSET(phsSocketCur,FLAG_RECEIVING)) {
					// add to read descriptor set
					FD_SET(socket,&fdsSelectRead);
				}
				if (ISFLAGSET(phsSocketCur,FLAG_SENDING)) {
					// add to write descriptor set
					FD_SET(socket,&fdsSelectWrite);
				}
				// check if new max socket
				if (socket>iSelectMaxFds) {
				iSelectMaxFds=socket;
				}
			}
		}

		{
			struct timeval tvSelectWait;
			// initialize select delay
			tvSelectWait.tv_sec = 1;
			tvSelectWait.tv_usec = 0; // note: using timeval here -> usec not nsec

			// and check sockets (may take a while!)
			iRc=select(iSelectMaxFds+1, &fdsSelectRead, &fdsSelectWrite,
					NULL, &tvSelectWait);
		}
		if (iRc<0) {
			if (hp->bKillWebserver) break;
			DBG("Select error\n");
			msleep(1000);
			continue;
		}
		if (iRc>0) {
			HttpSocket *phsSocketNext;
			// check which sockets are read/write able
			phsSocketCur=hp->phsSocketHead;
			while (phsSocketCur) {
				BOOL bRead;
				BOOL bWrite;

				phsSocketNext=phsSocketCur->next;
				// get socket fd
				socket=phsSocketCur->socket;
		          
				// get read/write status for socket
				bRead=FD_ISSET(socket, &fdsSelectRead);
				bWrite=FD_ISSET(socket, &fdsSelectWrite);

				if ((bRead|bWrite)!=0) {
					//DBG("socket %d bWrite=%d, bRead=%d\n",phsSocketCur->socket,bWrite,bRead);
					// if readable or writeable then process
					if (bWrite && ISFLAGSET(phsSocketCur,FLAG_SENDING)) {
						iRc=_mwProcessWriteSocket(hp, phsSocketCur);
					} else if (bRead && ISFLAGSET(phsSocketCur,FLAG_RECEIVING)) {
						iRc=_mwProcessReadSocket(hp,phsSocketCur);
					} else {
						iRc=-1;
						DBG("Invalid socket state (flag: %08x)\n",phsSocketCur->flags);
						SETFLAG(phsSocketCur,FLAG_CONN_CLOSE);
					}
					if (!iRc) {
						// and reset expiration timer
						phsSocketCur->tmExpirationTime=time(NULL)+HTTP_EXPIRATION_TIME;
					} else {
						_mwCloseSocket(hp, phsSocketCur);
					}
				}
				phsSocketCur=phsSocketNext;
			}

			// check if any socket to accept and accept the socket
			if (FD_ISSET(hp->listenSocket, &fdsSelectRead) &&
					hp->stats.clientCount<hp->maxClients &&
					(socket=_mwAcceptSocket(hp,&sinaddr))) {
				// create a new socket structure and insert it in the linked list
				phsSocketCur=(HttpSocket*)malloc(sizeof(HttpSocket));
				if (!phsSocketCur) {
					DBG("Out of memory\n");
					break;
				}
				phsSocketCur->next=hp->phsSocketHead;
				hp->phsSocketHead=phsSocketCur;	//set new header of the list
				//fill structure with data
				_mwInitSocketData(phsSocketCur);
				phsSocketCur->tmAcceptTime=time(NULL);
				phsSocketCur->socket=socket;
				phsSocketCur->tmExpirationTime=time(NULL)+HTTP_EXPIRATION_TIME;
				phsSocketCur->iRequestCount=0;
				phsSocketCur->request.ipAddr.laddr=ntohl(sinaddr.sin_addr.s_addr);
				hp->stats.clientCount++;
				//update max client count
				if (hp->stats.clientCount>hp->stats.clientCountMax)
					hp->stats.clientCountMax=hp->stats.clientCount;
				{
					IP ip=phsSocketCur->request.ipAddr;
					SYSLOG(LOG_INFO,"[%d] IP: %d.%d.%d.%d\n",phsSocketCur->socket,ip.caddr[3],ip.caddr[2],ip.caddr[1],ip.caddr[0]);
				}
				SYSLOG(LOG_INFO,"Connected clients: %d\n",hp->stats.clientCount);
			}
		} else {
			HttpSocket *phsSocketPrev=NULL;
			// select timeout
			// clean up the link list
			phsSocketCur=hp->phsSocketHead;
			while (phsSocketCur) {
				if (!phsSocketCur->socket) {
					DBG("Free up socket structure 0x%08x\n",phsSocketCur);
					if (phsSocketPrev) {
						phsSocketPrev->next=phsSocketCur->next;
						free(phsSocketCur);
						phsSocketCur=phsSocketPrev->next;
					} else {
						hp->phsSocketHead=phsSocketCur->next;
						free(phsSocketCur);
						phsSocketCur=hp->phsSocketHead;
					}
				} else {
					phsSocketPrev=phsSocketCur;
					phsSocketCur=phsSocketCur->next;
				}
			}
		}
	}

	{
		phsSocketCur=hp->phsSocketHead;
		while (phsSocketCur) {
			HttpSocket *phsSocketNext;
			phsSocketCur->flags=FLAG_CONN_CLOSE;
			_mwCloseSocket(hp, phsSocketCur);
			phsSocketNext=phsSocketCur->next;
			free(phsSocketCur);
			phsSocketCur=phsSocketNext;
	}
	}

	// clear state vars
	hp->bKillWebserver=FALSE;
	hp->bWebserverRunning=FALSE;
  
	return NULL;
} // end of _mwHttpThread

////////////////////////////////////////////////////////////////////////////
// _mwAcceptSocket
// Accept an incoming connection
////////////////////////////////////////////////////////////////////////////
SOCKET _mwAcceptSocket(HttpParam* hp,struct sockaddr_in *sinaddr)
{
    SOCKET socket;
	int namelen=sizeof(struct sockaddr);

	socket=accept(hp->listenSocket, (struct sockaddr*)sinaddr,&namelen);

    SYSLOG(LOG_INFO,"[%d] connection accepted @ %s\n",socket,GetTimeString());

	if ((int)socket<=0) {
		DBG("Error accepting socket\n");
		return 0;
    } 

#ifndef WIN32
    // set to non-blocking to stop sends from locking up thread
	{
        int iRc;
        int iSockFlags;
        iSockFlags = fcntl(socket, F_GETFL, 0);
        iSockFlags |= O_NONBLOCK;
        iRc = fcntl(socket, F_SETFL, iSockFlags);
	}
#endif

	if (hp->socketRcvBufSize) {
		int iSocketBufSize=hp->socketRcvBufSize<<10;
		setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (const char*)&iSocketBufSize, sizeof(int));
	}

	return socket;
} // end of _mwAcceptSocket

int _mwBuildHttpHeader(HttpParam* hp, HttpSocket *phsSocket, time_t contentDateTime, unsigned char* buffer)
{
	char *p=buffer;
	p+=sprintf(p,HTTP200_HEADER,
		(phsSocket->request.iStartByte==0)?"200 OK":"206 Partial content",
		HTTP_KEEPALIVE_TIME,hp->maxReqPerConn,
		ISFLAGSET(phsSocket,FLAG_CONN_CLOSE)?"close":"Keep-Alive");
	p+=mwGetHttpDateTime(contentDateTime, p);
	SETWORD(p,DEFWORD('\r','\n'));
	p+=2;
	p+=sprintf(p,"Content-Type: %s\r\n",contentTypeTable[phsSocket->response.fileType]);
	if (phsSocket->response.iContentLength >= 0) {
		p+=sprintf(p,"Content-Length: %d\r\n",phsSocket->response.iContentLength);
	}
	SETDWORD(p,DEFDWORD('\r','\n',0,0));
	return (int)p-(int)buffer+2;
}

int mwParseQueryString(UrlHandlerParam* up)
{
	if (up->iVarCount==-1) {
		//parsing variables from query string
		char *p,*s = NULL;
		if (ISFLAGSET(up->hs,FLAG_REQUEST_GET)) {
			// get start of query string
			s = strchr(up->pucRequest, '?');
			if (s) {
				*(s++) = 0;
			}
#ifdef HTTPPOST
		} else {
			s = up->hs->request.pucPayload;
#endif
		}
		if (s && *s) {
			int i;
			int n = 1;
			//get number of variables
			for (p = s; *p ; ) if (*(p++)=='&') n++;
			up->pxVars = calloc(n + 1, sizeof(HttpVariables));
			up->iVarCount = n;
			//store variable name and value
			for (i = 0, p = s; i < n; p++) {
				switch (*p) {
				case '=':
					if (!(up->pxVars + i)->name) {
						*p = 0;
						(up->pxVars + i)->name = s;
						s=p+1;
					}
					break;
				case 0:
				case '&':
					*p = 0;
					if ((up->pxVars + i)->name) {
						(up->pxVars + i)->value = s;
						_mwDecodeString(s);
					} else {
						(up->pxVars + i)->name = s;
						(up->pxVars + i)->value = p;
					}
					s = p + 1;
					i++;
					break;
				}
			}
			(up->pxVars + n)->name = NULL;
		}
	}
	return up->iVarCount;
}

int _mwCheckUrlHandlers(HttpParam* hp, HttpSocket* phsSocket)
{
	UrlHandler* puh;
	UrlHandlerParam up;
	int ret=0;

	up.pxVars=NULL;
	for (puh=hp->pxUrlHandler; puh->pchUrlPrefix; puh++) {
		int iPrefixLen=strlen(puh->pchUrlPrefix);
		if (puh->pfnUrlHandler && !strncmp(phsSocket->request.pucPath,puh->pchUrlPrefix,iPrefixLen)) {
			//URL prefix matches
			memset(&up, 0, sizeof(up));
			up.hp=hp;
			up.hs = phsSocket;
			up.iDataBytes=phsSocket->response.iBufferSize;
			up.pucRequest=phsSocket->request.pucPath+iPrefixLen;
			up.pucHeader=phsSocket->buffer;
			up.pucBuffer=phsSocket->pucData;
			up.pucBuffer[0]=0;
			up.iVarCount=-1;
			phsSocket->ptr=(void*)puh->pfnUrlHandler;
			ret=(*(PFNURLCALLBACK)phsSocket->ptr)(&up);
			if (!ret) continue;
			if (ret & FLAG_DATA_REDIRECT) {
				_mwRedirect(phsSocket, up.pucBuffer);
				DBG("URL handler: redirect\n");
			} else {
				phsSocket->flags|=ret;
				phsSocket->response.fileType=up.fileType;
				hp->stats.urlProcessCount++;
				if (ret & FLAG_TO_FREE) {
					phsSocket->ptr=up.pucBuffer;	//keep the pointer which will be used to free memory later
				}
				if (ret & FLAG_DATA_RAW) {
					phsSocket->pucData=up.pucBuffer;
					phsSocket->iDataLength=up.iDataBytes;
					phsSocket->response.iContentLength=up.iContentBytes>0?up.iContentBytes:up.iDataBytes;
					DBG("URL handler: raw data)\n");
				} else if (ret & FLAG_DATA_FILE) {
					phsSocket->flags|=FLAG_DATA_FILE;
					if (up.pucBuffer[0])
						phsSocket->request.pucPath=up.pucBuffer;
					DBG("URL handler: file\n");
				} else if (ret & FLAG_DATA_FD) {
					phsSocket->flags |= FLAG_DATA_FILE;
					DBG("URL handler: file descriptor\n");
				}
				break;
			}
		}
	}
	if (up.pxVars) free(up.pxVars);
	return ret;
}

////////////////////////////////////////////////////////////////////////////
// _mwProcessReadSocket
// Process a socket (read)
////////////////////////////////////////////////////////////////////////////
int _mwProcessReadSocket(HttpParam* hp, HttpSocket* phsSocket)
{
	char *p;

#ifdef HTTPPOST
    if ((HttpMultipart*)phsSocket->ptr != NULL) {
      //_mwProcessMultipartPost(phsSocket);
      return 0;
    }
#endif
	// check if receive buffer full
	if (phsSocket->iDataLength>=MAX_REQUEST_SIZE) {
		// close connection
		SYSLOG(LOG_INFO,"Invalid request header size (%d bytes)\n",phsSocket->iDataLength);
		SETFLAG(phsSocket, FLAG_CONN_CLOSE);
		return -1;
	}
	// read next chunk of data
	{
		int sLength;
		sLength=recv(phsSocket->socket, 
						phsSocket->pucData+phsSocket->iDataLength,
						phsSocket->response.iBufferSize-phsSocket->iDataLength, 0);
		if (sLength <= 0) {
			SYSLOG(LOG_INFO,"[%d] socket closed by client\n",phsSocket->socket);
			SETFLAG(phsSocket, FLAG_CONN_CLOSE);
			return -1;
		}
		// add in new data received
		phsSocket->iDataLength+=sLength;
	}
	//check request type
	switch (GETDWORD(phsSocket->pucData)) {
	case HTTP_GET:
		SETFLAG(phsSocket,FLAG_REQUEST_GET);
		phsSocket->request.pucPath=phsSocket->pucData+5;
		break;
#ifdef HTTPPOST
	case HTTP_POST:
		SETFLAG(phsSocket,FLAG_REQUEST_POST);
		phsSocket->request.pucPath=phsSocket->pucData+6;
		break;
#endif
	}

	// check if end of request
	if (phsSocket->request.siHeaderSize==0) {
		int i=0;
		while (GETDWORD(phsSocket->buffer + i) != HTTP_HEADEREND) {
			if (++i > phsSocket->iDataLength - 3) return 0;
		}
		// reach the end of the header
		if (!ISFLAGSET(phsSocket,FLAG_REQUEST_GET|FLAG_REQUEST_POST)) {
			SYSLOG(LOG_INFO,"[%d] Unsupported method\n",phsSocket->socket);		
			SETFLAG(phsSocket,FLAG_CONN_CLOSE);
			return -1;
		}
		phsSocket->request.siHeaderSize = i + 4;
		DBG("[%d] header size: %d bytes\n",phsSocket->socket,phsSocket->request.siHeaderSize);
		if (_mwParseHttpHeader(phsSocket)) {
			SYSLOG(LOG_INFO,"Error parsing request\n");
			SETFLAG(phsSocket, FLAG_CONN_CLOSE);
			return -1;
#ifdef HTTPPOST
		} else if (ISFLAGSET(phsSocket,FLAG_REQUEST_POST)) {
			hp->stats.reqPostCount++;
			phsSocket->request.pucPayload=malloc(phsSocket->response.iContentLength+1);
			phsSocket->request.pucPayload[phsSocket->response.iContentLength]=0;
			phsSocket->iDataLength -= phsSocket->request.siHeaderSize;
			memcpy(phsSocket->request.pucPayload, phsSocket->buffer + phsSocket->request.siHeaderSize, phsSocket->iDataLength);
			phsSocket->pucData = phsSocket->request.pucPayload;
#endif
		}
		// add header zero terminator
		phsSocket->buffer[phsSocket->request.siHeaderSize]=0;
		DBG("%s",phsSocket->buffer);
	}
	if ( phsSocket->iDataLength < phsSocket->response.iContentLength ) {
		return 0;
	}
	p=phsSocket->buffer + phsSocket->request.siHeaderSize + 4;
	p=(unsigned char*)((unsigned long)p & (-4));	//keep 4-byte aligned
	*p=0;
	//keep request path
	{
		char *q;
		int iPathLen;
		for (q=phsSocket->request.pucPath;*q && *q!=' ';q++);
		iPathLen=(int)q-(int)(phsSocket->request.pucPath);
		if (iPathLen>=MAX_REQUEST_PATH_LEN) {
			DBG("Request path too long and is stripped\n");
			iPathLen=MAX_REQUEST_PATH_LEN-1;
		}
		if (iPathLen>0)
			memcpy(p,phsSocket->request.pucPath,iPathLen);
		*(p+iPathLen)=0;
		phsSocket->request.pucPath=p;
		p=(unsigned char*)(((unsigned long)(p+iPathLen+4+1))&(-4));	//keep 4-byte aligned
	}
	phsSocket->pucData=p;	//free buffer space
	phsSocket->response.iBufferSize=(HTTP_BUFFER_SIZE-(phsSocket->pucData-phsSocket->buffer)-1)&(-4);

	SYSLOG(LOG_INFO,"[%d] request path: /%s\n",phsSocket->socket,phsSocket->request.pucPath);
	hp->stats.reqCount++;
	if (ISFLAGSET(phsSocket,FLAG_REQUEST_GET|FLAG_REQUEST_POST)) {
		if (hp->pxUrlHandler) {
			if (!_mwCheckUrlHandlers(hp,phsSocket))
				SETFLAG(phsSocket,FLAG_DATA_FILE);
		}
		// set state to SENDING (actual sending will occur on next select)
		CLRFLAG(phsSocket,FLAG_RECEIVING)
		SETFLAG(phsSocket,FLAG_SENDING);
		hp->stats.reqGetCount++;
		if (ISFLAGSET(phsSocket,FLAG_DATA_FILE)) {
			// send requested page
			return _mwStartSendFile(hp,phsSocket);
		} else if (ISFLAGSET(phsSocket,FLAG_DATA_RAW)) {
			return _mwStartSendRawData(hp, phsSocket);
		}
	}
	SYSLOG(LOG_INFO,"Error occurred (might be a bug)\n");
	return -1;
} // end of _mwProcessReadSocket

////////////////////////////////////////////////////////////////////////////
// _mwProcessWriteSocket
// Process a socket (write)
////////////////////////////////////////////////////////////////////////////
int _mwProcessWriteSocket(HttpParam *hp, HttpSocket* phsSocket)
{
	if (phsSocket->iDataLength<=0) {
		SYSLOG(LOG_INFO,"[%d] Data sending completed (%d/%d)\n",phsSocket->socket,phsSocket->response.iSentBytes,phsSocket->response.iContentLength);
		return 1;
	}
	SYSLOG(LOG_INFO,"[%d] sending data\n",phsSocket->socket);
	if (ISFLAGSET(phsSocket,FLAG_DATA_RAW|FLAG_DATA_STREAM)) {
		return _mwSendRawDataChunk(hp, phsSocket);
	} else if (ISFLAGSET(phsSocket,FLAG_DATA_FILE)) {
		return _mwSendFileChunk(hp, phsSocket);
	} else {
		SYSLOG(LOG_INFO,"Invalid content source\n");
		return -1;
	}
} // end of _mwProcessWriteSocket

////////////////////////////////////////////////////////////////////////////
// _mwCloseSocket
// Close an open connection
////////////////////////////////////////////////////////////////////////////
void _mwCloseSocket(HttpParam* hp, HttpSocket* phsSocket)
{
	if (phsSocket->fd > 0) {
		close(phsSocket->fd);
	}
	phsSocket->fd = 0;
	if (ISFLAGSET(phsSocket,FLAG_TO_FREE) && phsSocket->ptr) {
		free(phsSocket->ptr);
		phsSocket->ptr=NULL;
	}
#ifdef HTTPPOST
	if (phsSocket->request.pucPayload) {
		free(phsSocket->request.pucPayload);
	}
#endif
	if (!ISFLAGSET(phsSocket,FLAG_CONN_CLOSE) && phsSocket->iRequestCount<hp->maxReqPerConn) {
		_mwInitSocketData(phsSocket);
		//reset flag bits
		phsSocket->iRequestCount++;
		phsSocket->tmExpirationTime=time(NULL)+HTTP_KEEPALIVE_TIME;
		return;
	}
    if (phsSocket->socket != 0) {
		closesocket(phsSocket->socket);
	} else {
		SYSLOG(LOG_INFO,"[%d] bug: socket=0 (structure: 0x%x \n",phsSocket->socket,phsSocket);
	}

	hp->stats.clientCount--;
	phsSocket->iRequestCount=0;
	SYSLOG(LOG_INFO,"[%d] socket closed after responded for %d requests\n",phsSocket->socket,phsSocket->iRequestCount);
	SYSLOG(LOG_INFO,"Connected clients: %d\n",hp->stats.clientCount);
	phsSocket->socket=0;

} // end of _mwCloseSocket

__inline int _mwStrCopy(char *dest, char *src)
{
	int i;
	for (i=0; src[i]; i++) {
		dest[i]=src[i];
	}
	dest[i]=0;
	return i;
}

int _mwListDirectory(HttpSocket* phsSocket, char* dir)
{
	char cFileName[128];
	char cFilePath[MAX_PATH];
	char *p=phsSocket->pucData;
	int ret;
	char *pagebuf=phsSocket->pucData;
	int bufsize=phsSocket->response.iBufferSize;
	
	p+=sprintf(p,"<html><head><title>/%s</title></head><body><table border=0 cellpadding=0 cellspacing=0 width=100%%><h2>Directory of /%s</h2><hr>",
		phsSocket->request.pucPath,phsSocket->request.pucPath);
	if (!*dir) SETWORD(dir,DEFWORD('.',0));
	DBG("Listing directory: %s\n",dir);
	for (ret=ReadDir(dir,cFileName); !ret; ret=ReadDir(NULL,cFileName)) {
		struct stat st;
		char *s;
		int bytes;
		if (GETWORD(cFileName)==DEFWORD('.',0)) continue;
		DBG("Checking %s ...\n",cFileName);
		bytes=p-pagebuf;
		if (bytes+384>bufsize) {
			//need to expand buffer
			bufsize+=2048;
			if (!ISFLAGSET(phsSocket,FLAG_TO_FREE)) {
				//first time expanding
				SETFLAG(phsSocket,FLAG_TO_FREE);
				pagebuf=malloc(bufsize);
				memcpy(pagebuf,phsSocket->pucData,bytes);
			} else {
				pagebuf=realloc(pagebuf,bufsize);
			}
			p=pagebuf+bytes;
			DBG("Buffer expanded to %d bytes\n",bufsize);
		}
		sprintf(cFilePath,"%s/%s",dir,cFileName);
		if (stat(cFilePath,&st)) continue;
		if (st.st_mode & S_IFDIR) {
			p+=sprintf(p,"<tr><td width=35%%><a href='%s/'>%s</a></td><td width=15%%>&lt;dir&gt;</td><td width=15%%>",
				cFileName,cFileName);
		} else {
			p+=sprintf(p,"<tr><td width=35%%><a href='%s'>%s</a></td><td width=15%%>%d bytes</td><td width=15%%>",
				cFileName,cFileName,st.st_size);
			s=strrchr(cFileName,'.');
			if (s) {
				int filetype=_mwGetContentType(++s);
				if (filetype!=HTTPFILETYPE_OCTET)
					p+=_mwStrCopy(p,contentTypeTable[filetype]);
				else
					p+=sprintf(p,"%s file",s);
			}
		}
		p+=_mwStrCopy(p,"</td><td>");
		p+=mwGetHttpDateTime(st.st_mtime,p);
		p+=_mwStrCopy(p,"</td></tr>");
	}
	p+=sprintf(p,"</table><hr><i>Directory content generated by MiniWeb</i></body></html>");
	ReadDir(NULL,NULL);
	phsSocket->response.iContentLength=(phsSocket->iDataLength=p-pagebuf);
	phsSocket->response.fileType=HTTPFILETYPE_HTML;
	if (ISFLAGSET(phsSocket,FLAG_TO_FREE)) {
		phsSocket->pucData=pagebuf;
		phsSocket->ptr=pagebuf;
	}
	return 0;
}

void _mwSend404Page(HttpSocket* phsSocket)
{
	int bytes,offset=0;
	char *p=HTTP404_HEADER;

	SYSLOG(LOG_INFO,"[%d] Http file not found\n",phsSocket->socket);
	// send file not found header
	do {
		bytes=send(phsSocket->socket, p+offset,sizeof(HTTP404_HEADER)-1-offset,0);
		if (bytes<=0) break;
		offset+=bytes;
	} while (offset<sizeof(HTTP404_HEADER)-1);
}

#ifdef WIN32
#define OPEN_FLAG O_RDONLY|0x8000
#else
#define OPEN_FLAG O_RDONLY
#endif

////////////////////////////////////////////////////////////////////////////
// _mwStartSendFile
// Setup for sending of a file
////////////////////////////////////////////////////////////////////////////
int _mwStartSendFile(HttpParam* hp, HttpSocket* phsSocket)
{
	struct stat st;
	HttpFilePath hfp;

#ifdef HTTPAUTH
	// check if authenticated
	if (FALSE == _mwCheckAuthentication(phsSocket)) {
		// Not authenticated
		if (phsSocket->response.fileType==HTTPFILETYPE_HTML) {
		// send password page only
			pchFilename=(char*)g_chPasswordPage;
		}
	}
#endif

	hfp.pchRootPath=hp->pchWebPath;
	// check type of file requested
	if (!(phsSocket->flags & FLAG_DATA_FD)) {
		hfp.pchHttpPath=phsSocket->request.pucPath;
		mwGetLocalFileName(&hfp);
		// open file
		phsSocket->fd=open(hfp.cFilePath,OPEN_FLAG);
	} else {
		strcpy(hfp.cFilePath, phsSocket->request.pucPath);
		hfp.pchExt = strrchr(hfp.cFilePath, '.');
		if (hfp.pchExt) hfp.pchExt++;
	}

	if (phsSocket->fd < 0) {
		char *p;
		int i;
		if (stat(hfp.cFilePath,&st) < 0 || !(st.st_mode & S_IFDIR)) {
			// file/dir not found
			_mwSend404Page(phsSocket);
			return -1;
		}
		for (p = hfp.cFilePath; *p; p++);
		
		//requesting for directory, first try opening default pages
		*(p++)=SLASH;
		for (i=0; defaultPages[i]; i++) {
			strcpy(p,defaultPages[i]);
			phsSocket->fd=open(hfp.cFilePath,OPEN_FLAG);
			if (phsSocket->fd > 0) break;
		}

		if (phsSocket->fd <= 0 && (hp->flags & FLAG_DIR_LISTING)) {
			SETFLAG(phsSocket,FLAG_DATA_RAW);
			if (!hfp.fTailSlash) {
				p=phsSocket->request.pucPath;
				while(*p) p++;				//seek to the end of the string
				SETWORD(p,DEFWORD('/',0));	//add a tailing slash
				while(--p>(char*)phsSocket->request.pucPath) {
					if (*p=='/') {
						p++;
						break;
					}
				}
				_mwRedirect(phsSocket,p);
			} else {
				*(p-1)=0;
				_mwListDirectory(phsSocket,hfp.cFilePath);
			}
			return _mwStartSendRawData(hp, phsSocket);
		}
		phsSocket->response.fileType = HTTPFILETYPE_HTML;
	} else {
		phsSocket->response.iContentLength = !fstat(phsSocket->fd, &st) ? st.st_size - phsSocket->request.iStartByte : 0;
		if (phsSocket->response.iContentLength <= 0) {
			phsSocket->request.iStartByte = 0;
		}
		if (phsSocket->request.iStartByte) {
			lseek(phsSocket->fd, phsSocket->request.iStartByte, SEEK_SET);
		}
		if (!phsSocket->response.fileType && hfp.pchExt) {
			phsSocket->response.fileType=_mwGetContentType(hfp.pchExt);
		}
		// mark if substitution needed
		if (hp->pfnSubst && (phsSocket->response.fileType==HTTPFILETYPE_HTML ||phsSocket->response.fileType==HTTPFILETYPE_JS)) {
			SETFLAG(phsSocket,FLAG_SUBST);
		}
	}


	SYSLOG(LOG_INFO,"File/requested size: %d/%d\n",st.st_size,phsSocket->response.iContentLength);

	// build http header
	phsSocket->iDataLength=_mwBuildHttpHeader(
		hp,
		phsSocket,
		st.st_mtime,
		phsSocket->pucData);

	phsSocket->response.iSentBytes=-phsSocket->iDataLength;
	hp->stats.fileSentCount++;
	return 0;
} // end of _mwStartSendFile

////////////////////////////////////////////////////////////////////////////
// _mwSendFileChunk
// Send a chunk of a file
////////////////////////////////////////////////////////////////////////////
int _mwSendFileChunk(HttpParam *hp, HttpSocket* phsSocket)
{
	int iBytesWritten;
	int iBytesRead;

	// send a chunk of data
	iBytesWritten=send(phsSocket->socket, phsSocket->pucData,phsSocket->iDataLength, 0);
	if (iBytesWritten<=0) {
		// close connection
		DBG("[%d] error sending data\n", phsSocket->socket);
		SETFLAG(phsSocket,FLAG_CONN_CLOSE);
		close(phsSocket->fd);
		phsSocket->fd = 0;
		return -1;
	}
	phsSocket->response.iSentBytes+=iBytesWritten;
	phsSocket->pucData+=iBytesWritten;
	phsSocket->iDataLength-=iBytesWritten;
	SYSLOG(LOG_INFO,"[%d] sent %d bytes of %d\n",phsSocket->socket,phsSocket->response.iSentBytes,phsSocket->response.iContentLength);
	// if only partial data sent just return wait the remaining data to be sent next time
	if (phsSocket->iDataLength>0)	return 0;

	// used all buffered data - load next chunk of file
	phsSocket->pucData=phsSocket->buffer;
	if (phsSocket->fd > 0)
		iBytesRead=read(phsSocket->fd,phsSocket->buffer,HTTP_BUFFER_SIZE);
	else
		iBytesRead=0;
	if (iBytesRead<=0) {
		// finished with a file
		int iRemainBytes=phsSocket->response.iContentLength-phsSocket->response.iSentBytes;
		DBG("[%d] EOF reached\n",phsSocket->socket);
		if (iRemainBytes>0) {
			if (iRemainBytes>HTTP_BUFFER_SIZE) iRemainBytes=HTTP_BUFFER_SIZE;
			DBG("Sending %d padding bytes\n",iRemainBytes);
			memset(phsSocket->buffer,0,iRemainBytes);
			phsSocket->iDataLength=iRemainBytes;
			return 0;
		} else {
			DBG("Closing file (fd=%d)\n",phsSocket->fd);
			hp->stats.fileSentBytes+=phsSocket->response.iSentBytes;
			if (phsSocket->fd > 0) close(phsSocket->fd);
			phsSocket->fd = 0;
			return 1;
		}
	}
	if (ISFLAGSET(phsSocket,FLAG_SUBST)) {
		int iBytesUsed;
		// substituted file - read smaller chunk
		phsSocket->iDataLength=_mwSubstVariables(hp, phsSocket->buffer,iBytesRead,&iBytesUsed);
		if (iBytesUsed<iBytesRead) {
			lseek(phsSocket->fd,iBytesUsed-iBytesRead,SEEK_CUR);
		}
	} else {
		phsSocket->iDataLength=iBytesRead;
	}
	return 0;
} // end of _mwSendFileChunk

////////////////////////////////////////////////////////////////////////////
// _mwStartSendRawData
// Start sending raw data blocks
////////////////////////////////////////////////////////////////////////////
int _mwStartSendRawData(HttpParam *hp, HttpSocket* phsSocket)
{
	unsigned char header[HTTP200_HDR_EST_SIZE];
	int offset=0,hdrsize,bytes;
	hdrsize=_mwBuildHttpHeader(hp, phsSocket,time(NULL),header);
	// send http header
	do {
		bytes=send(phsSocket->socket, header+offset,hdrsize-offset,0);
		if (bytes<=0) break;
		offset+=bytes;
	} while (offset<hdrsize);
	if (bytes<=0) {
		// failed to send header (socket may have been closed by peer)
		SYSLOG(LOG_INFO,"Failed to send header\n");
		return -1;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////
// _mwSendRawDataChunk
// Send a chunk of a raw data block
////////////////////////////////////////////////////////////////////////////
int _mwSendRawDataChunk(HttpParam *hp, HttpSocket* phsSocket)
{
	int  iBytesWritten;

    // send a chunk of data
	if (phsSocket->iDataLength==0) {
		if (ISFLAGSET(phsSocket,FLAG_DATA_STREAM) && phsSocket->ptr) {
			//FIXME: further implementation of FLAG_DATA_STREAM case
		}
		hp->stats.fileSentBytes+=phsSocket->response.iSentBytes;
		return 1;
	}
	iBytesWritten=(int)send(phsSocket->socket, phsSocket->pucData,phsSocket->iDataLength, 0);
    if (iBytesWritten<=0) {
		// failure - close connection
		SYSLOG(LOG_INFO,"Connection closed\n");
		SETFLAG(phsSocket,FLAG_CONN_CLOSE);
		return -1;
    } else {
		SYSLOG(LOG_INFO,"[%d] sent %d bytes of raw data\n",phsSocket->socket,iBytesWritten);
		phsSocket->response.iSentBytes+=iBytesWritten;
		phsSocket->pucData+=iBytesWritten;
		phsSocket->iDataLength-=iBytesWritten;
	}
	if (phsSocket->iDataLength<=0 && 
			ISFLAGSET(phsSocket,FLAG_DATA_STREAM) &&
			phsSocket->response.iSentBytes<phsSocket->response.iContentLength) {
		//load next chuck of raw data
		UrlHandlerParam uhp;
		memset(&uhp,0,sizeof(UrlHandlerParam));
		uhp.hp=hp;
		uhp.iContentBytes=phsSocket->response.iContentLength;
		uhp.iSentBytes=phsSocket->response.iSentBytes;
		uhp.pucBuffer=phsSocket->buffer;
		uhp.iDataBytes=HTTP_BUFFER_SIZE;
		if ((*(PFNURLCALLBACK)(phsSocket->ptr))(&uhp)) {
			phsSocket->pucData=uhp.pucBuffer;
			phsSocket->iDataLength=uhp.iDataBytes;
		}
	}
	return 0;
} // end of _mwSendRawDataChunk

////////////////////////////////////////////////////////////////////////////
// _mwRedirect
// Setup for redirect to another file
////////////////////////////////////////////////////////////////////////////
void _mwRedirect(HttpSocket* phsSocket, char* pchPath)
{
	char* path;
	// raw (not file) data send mode
	SETFLAG(phsSocket,FLAG_DATA_RAW);

	// messages is HTML
	phsSocket->response.fileType=HTTPFILETYPE_HTML;

	// build redirect message
	SYSLOG(LOG_INFO,"[%d] Http redirection to %s\n",phsSocket->socket,pchPath);
	path = (pchPath == phsSocket->pucData) ? strdup(pchPath) : pchPath;
	phsSocket->iDataLength=sprintf(phsSocket->pucData,HTTPBODY_REDIRECT,path);
	phsSocket->response.iContentLength=phsSocket->iDataLength;
	if (path != pchPath) free(path);
} // end of _mwRedirect

////////////////////////////////////////////////////////////////////////////
// _mwSubstVariables
// Perform substitution of variables in a buffer
// returns new length
////////////////////////////////////////////////////////////////////////////
int _mwSubstVariables(HttpParam* hp, char* pchData, int iLength, int* piBytesUsed)
{
	int lastpos,pos=0,used=0;
	SubstParam sp;
	int iValueBytes;
    DBG("_SubstVariables input len %d\n",iLength);
	iLength--;
	for(;;) {
		lastpos=pos;
		for (; pos<iLength && *(WORD*)(pchData+pos)!=HTTP_SUBST_PATTERN; pos++);
		used+=(pos-lastpos);
		if (pos==iLength) {
			*piBytesUsed=used+1;
			return iLength+1;
		}
		lastpos=pos;
		for (pos=pos+2; pos<iLength && *(WORD*)(pchData+pos)!=HTTP_SUBST_PATTERN; pos++);
		if (pos==iLength) {
			*piBytesUsed=used;
			return lastpos;
		}
		pos+=2;
		used+=pos-lastpos;
		pchData[pos-2]=0;
		sp.pchParamName=pchData+lastpos+2;
		sp.iMaxValueBytes=pos-lastpos;
		sp.pchParamValue=pchData+lastpos;
		iValueBytes=hp->pfnSubst(&sp);
		if (iValueBytes>=0) {
			if (iValueBytes>sp.iMaxValueBytes) iValueBytes=sp.iMaxValueBytes;
			memmove(pchData+lastpos+iValueBytes,pchData+pos,iLength-pos);
			iLength=iLength+iValueBytes-(pos-lastpos);
			pos=lastpos+iValueBytes;
		} else {
			DBG("No matched variable for %s\n",sp.pchParamName);
			pchData[pos-2]='$';
		}
	}
} // end of _mwSubstVariables

////////////////////////////////////////////////////////////////////////////
// _mwStrStrNoCase
// Case insensitive version of ststr
////////////////////////////////////////////////////////////////////////////
char* _mwStrStrNoCase(char* pchHaystack, char* pchNeedle)
{
  char* pchReturn=NULL;

  while(*pchHaystack!='\0' && pchReturn==NULL) {
    if (toupper(*pchHaystack)==toupper(pchNeedle[0])) {
      char* pchTempHay=pchHaystack;
      char* pchTempNeedle=pchNeedle;
      // start of match
      while(*pchTempHay!='\0') {
        if(toupper(*pchTempHay)!=toupper(*pchTempNeedle)) {
          // failed to match
          break;
        }
        pchTempNeedle++;
        pchTempHay++;
        if (*pchTempNeedle=='\0') {
          // completed match
          pchReturn=pchHaystack;
          break;
        }
      }
    }
    pchHaystack++;
  }

  return pchReturn;
} // end of _mwStrStrNoCase

////////////////////////////////////////////////////////////////////////////
// _mwStrDword
////////////////////////////////////////////////////////////////////////////
char* _mwStrDword(char* pchHaystack, DWORD dwSub, DWORD dwCharMask)
{
	char* pchReturn=NULL;
	dwCharMask = dwCharMask?(dwCharMask & 0xdfdfdfdf):0xdfdfdfdf;
	dwSub &= dwCharMask;
	while(*pchHaystack) {
		if (((*(DWORD*)pchHaystack) & dwCharMask)==dwSub)
			return pchHaystack;
	    pchHaystack++;
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////
// _mwDecodeCharacter
// Convert and individual character
////////////////////////////////////////////////////////////////////////////
__inline char _mwDecodeCharacter(char* s)
{
  	register unsigned char v;
	if (!*s) return 0;
	if (*s>='a' && *s<='f')
		v = *s-('a'-'A'+7);
	else if (*s>='A' && *s<='F')
		v = *s-7;
	else
		v = *s;
	if (*(++s)==0) return v;
	v <<= 4;
	if (*s>='a' && *s<='f')
		v |= (*s-('a'-'A'+7)) & 0xf;
	else if (*s>='A' && *s<='F')
		v |= (*s-7) & 0xf;
	else
		v |= *s & 0xf;
	return v;
} // end of _mwDecodeCharacter

////////////////////////////////////////////////////////////////////////////
// _mwDecodeString
// This function converts URLd characters back to ascii. For example
// %3A is '.'
////////////////////////////////////////////////////////////////////////////
void _mwDecodeString(char* pchString)
{
  int bEnd=FALSE;
  char* pchInput=pchString;
  char* pchOutput=pchString;

  do {
    switch (*pchInput) {
    case '%':
      if (*(pchInput+1)=='\0' || *(pchInput+2)=='\0') {
        // something not right - terminate the string and abort
        *pchOutput='\0';
        bEnd=TRUE;
      } else {
        *pchOutput=_mwDecodeCharacter(pchInput+1);
        pchInput+=3;
      }
      break;
/*
    case '+':
      *pchOutput=' ';
      pchInput++;
      break;
*/
    case '\0':
      bEnd=TRUE;
      // drop through
    default:
      // copy character
      *pchOutput=*pchInput;
      pchInput++;
    }
    pchOutput++;
  } while (!bEnd);
} // end of _mwDecodeString

int _mwGetContentType(char *pchExtname)
{
	// check type of file requested
	if (pchExtname[1]=='\0') {
		return HTTPFILETYPE_OCTET;
	} else if (pchExtname[2]=='\0') {
		switch (GETDWORD(pchExtname) & 0xffdfdf) {
		case FILEEXT_JS: return HTTPFILETYPE_JS;
		}
	} else if (pchExtname[3]=='\0' || pchExtname[3]=='?') {
		//identify 3-char file extensions
		switch (GETDWORD(pchExtname) & 0xffdfdfdf) {
		case FILEEXT_HTM:	return HTTPFILETYPE_HTML;
		case FILEEXT_XML:	return HTTPFILETYPE_XML;
		case FILEEXT_TEXT:	return HTTPFILETYPE_TEXT;
		case FILEEXT_CSS:	return HTTPFILETYPE_CSS;
		case FILEEXT_PNG:	return HTTPFILETYPE_PNG;
		case FILEEXT_JPG:	return HTTPFILETYPE_JPEG;
		case FILEEXT_GIF:	return HTTPFILETYPE_GIF;
		case FILEEXT_SWF:	return HTTPFILETYPE_SWF;
		case FILEEXT_MPA:	return HTTPFILETYPE_MPA;
		case FILEEXT_MPG:	return HTTPFILETYPE_MPEG;
		case FILEEXT_AVI:	return HTTPFILETYPE_AVI;
		case FILEEXT_MP4:	return HTTPFILETYPE_MP4;
		case FILEEXT_MOV:	return HTTPFILETYPE_MOV;
		}
	} else if (pchExtname[4]=='\0' || pchExtname[4]=='?') {
		//logic-and with 0xdfdfdfdf gets the uppercase of 4 chars
		switch (GETDWORD(pchExtname)&0xdfdfdfdf) {
		case FILEEXT_HTML:	return HTTPFILETYPE_HTML;
		case FILEEXT_MPEG:	return HTTPFILETYPE_MPEG;
		}
	}
	return HTTPFILETYPE_OCTET;
}

int _mwGrabToken(char *pchToken, char chDelimiter, char *pchBuffer, int iMaxTokenSize)
{
	char *p=pchToken;
	int iCharCopied=0;

	while (*p && *p!=chDelimiter && iCharCopied<iMaxTokenSize) {
		*(pchBuffer++)=*(p++);
		iCharCopied++;
	}
	pchBuffer='\0';
	return (*p==chDelimiter)?iCharCopied:0;
}

int _mwParseHttpHeader(HttpSocket* phsSocket)
{
	int iLen;
	char buf[12];
	char *p=phsSocket->buffer;		//pointer to header data
	HttpRequest *req=&phsSocket->request;

	//analyze rest of the header
	for(;;) {
		//look for a valid field name
		while (*p && *p!='\r') p++;		//travel to '\r'
		if (!*p || GETDWORD(p)==HTTP_HEADEREND) break;
		p+=2;							//skip "\r\n"
		switch (*(p++)) {
		case 'C':
			if (!memcmp(p,"onnection: ",11)) {
				p+=11;
				if (!memcmp(p,"close",5)) {
					SETFLAG(phsSocket,FLAG_CONN_CLOSE);
					p+=5;
				}
			} else if (!memcmp(p,"ontent-Length: ",15)) {
				p+=15;
				p+=_mwGrabToken(p,'\r',buf,sizeof(buf));
				phsSocket->response.iContentLength=atoi(buf);
			}
			break;
		case 'R':
			if (!memcmp(p,"eferer: ",8)) {
				p+=8;
				phsSocket->request.ofReferer=(int)p-(int)phsSocket->buffer;
			} else if (!memcmp(p,"ange: bytes=",12)) {
				p+=12;
				iLen=_mwGrabToken(p,'-',buf,sizeof(buf));
				if (iLen==0) continue;
				p+=iLen;
				phsSocket->request.iStartByte=atoi(buf);
				iLen=_mwGrabToken(p,'/',buf,sizeof(buf));
				if (iLen==0) continue;
				p+=iLen;
                phsSocket->response.iContentLength=atoi(buf)-phsSocket->request.iStartByte+1;
			}
			break;
		case 'H':
			if (!memcmp(p,"ost: ",5)) {
				p+=5;
				phsSocket->request.ofHost=(int)p-(int)phsSocket->buffer;
			}
			break;
		}
	}
	return 0;					//end of header
}
//////////////////////////// END OF FILE ///////////////////////////////////
