#include <stdio.h>
#include <string.h>
#include "httppil.h"
#include "httpapi.h"
#include "revision.h"
#ifdef _7Z
#include "7zDec/7zInc.h"
#endif
#include "httpxml.h"

int _mwBuildHttpHeader(HttpSocket *phsSocket, time_t contentDateTime, unsigned char* buffer);
void _mwCloseSocket(HttpParam* hp, HttpSocket* phsSocket);

//////////////////////////////////////////////////////////////////////////
// callback from the web server whenever a valid request comes in
//////////////////////////////////////////////////////////////////////////
int uhStats(UrlHandlerParam* param)
{
	char *p;
	char buf[128];
	HttpStats *stats=&((HttpParam*)param->hp)->stats;
	HttpRequest *req=&param->hs->request;
	IPADDR ip = param->hs->ipAddr;
	HTTP_XML_NODE node;
	int bufsize = param->dataBytes;
	int ret=FLAG_DATA_RAW;

	mwGetHttpDateTime(time(NULL), buf, sizeof(buf));

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
	param->dataBytes=(int)p-(int)(param->pucBuffer);
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
	param->dataBytes = len;
	param->pucBuffer = content;
	return FLAG_DATA_RAW;
}

#endif

#ifdef WIN32
void FileReadThread(UrlHandlerParam* param)
{
	int bytes;
	fd_set fds;
	struct timeval timeout;
	FILE *fp = fopen("f:\\11.xml", "rb");
	HttpParam* hp = param->hp;
	HttpSocket* phsSocket = param->hs;
	SOCKET s = phsSocket->socket;
	char buf[1024];

	free(param);
	param = 0;


	FD_ZERO(&fds);
	FD_SET(s, &fds);

	/* Set time limit. */
	timeout.tv_sec = 3;
	timeout.tv_usec = 0;

	/* build http response header */
	phsSocket->dataLength=_mwBuildHttpHeader(
		phsSocket,
		time(0),
		phsSocket->pucData);
	phsSocket->response.fileType = HTTPFILETYPE_XML;
	phsSocket->response.headerBytes = phsSocket->dataLength;
	phsSocket->response.sentBytes = 0;

	/* wait until the socket is allowed to send */
	while (!ISFLAGSET(phsSocket,FLAG_SENDING)) msleep(100);

	bytes = send(s, phsSocket->pucData, phsSocket->dataLength, 0);
	for (;;) {
		bytes = fread(buf, 1, sizeof(buf), fp);
		if (bytes > 0) {
			int rc = select(1, NULL, &fds, NULL, &timeout);
			if (rc==-1) {
				break;
			} else if (rc > 0) {
				bytes = send(s, buf, bytes, 0);
			} else {
				continue;
			}
		} else
			break;
	}
	/* tear down connection */
	SETFLAG(phsSocket, FLAG_CONN_CLOSE);
	_mwCloseSocket(hp, phsSocket);
}

int uhFileStream(UrlHandlerParam* param)
{
	if (!param->hs->ptr) {
		// first request
		DWORD dwid;
		UrlHandlerParam* p = malloc(sizeof(UrlHandlerParam));
		memcpy(p, param, sizeof(UrlHandlerParam));
		param->hs->ptr = CreateThread(0, 0, (LPTHREAD_START_ROUTINE)FileReadThread, p, 0, &dwid);
	}
	return FLAG_DATA_SOCKET;
}
#endif

//////////////////////////////////////////////////////////////////////////
// stream handler sample
//////////////////////////////////////////////////////////////////////////
#ifndef NOTHREAD
typedef struct {
	int state;
	pthread_t thread;
	char result[16];
} HANDLER_DATA;

void* WriteContent(HANDLER_DATA* hdata)
{
	char *p = hdata->result;
	int i;
	for (i = 0; i < 10; i++, p++) {
		*p = '0' + i;
		msleep(100);
	}
	*p = 0;
	return 0;
}

int uhAsyncDataTest(UrlHandlerParam* param)
{
	int ret = FLAG_DATA_STREAM | FLAG_TO_FREE;
	HANDLER_DATA* hdata = (HANDLER_DATA*)param->hs->ptr;
	
	if (param->pucBuffer) {
		if (!hdata) {
			// first invoke
			hdata = param->hs->ptr = calloc(1, sizeof(HANDLER_DATA));
			ThreadCreate(&hdata->thread, WriteContent, hdata);
			param->dataBytes = 0;
		} else {
			if (hdata->state == 1) {
				// done
				ret = 0;
			} else if (ThreadWait(hdata->thread, 10, 0)) {
				// data not ready
				param->dataBytes = 0;
			} else {
				// data ready
				strcpy(param->pucBuffer, hdata->result);
				param->dataBytes = strlen(param->pucBuffer);
				hdata->state = 1;
			}
		}
	} else {
		// cleanup
		ret = 0;
	}
	param->fileType=HTTPFILETYPE_TEXT;
	return ret;
}
#endif
