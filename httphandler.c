#include <stdio.h>
#include <string.h>
#include "httppil.h"
#include "httpapi.h"
#include "revision.h"
#ifdef _7Z
#include "7zDec/7zInc.h"
#endif
#include "httpxml.h"

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
