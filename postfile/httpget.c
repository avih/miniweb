/*******************************************************************
* HTTP helper
* Distributed under GPL license
* Copyright (c) 2005-06 Stanley Huang <stanleyhuangyc@yahoo.com.cn>
* All rights reserved.
*******************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <Winsock2.h>
#include "httpget.h"

/****************************************************************************
* HTTP protocol client
****************************************************************************/
#define HEADER_END_FLAG  "\r\n\r\n"
#ifdef _DEBUG
#define DEBUG printf
#else
#define DEBUG
#endif
#define CONN_RETRIES 3
#define HTTP_GET_HEADER "%s %s HTTP/1.0\r\nAccept: */*\r\nConnection: %s\r\nUser-Agent: Mozilla/5.0\r\nHost: %s\r\n%s\r\n"
#define HTTP_POST_HEADER "POST %s HTTP/1.0\r\nHost: %s\r\nContent-Type: application/x-www-form-urlencoded\r\nUser-Agent: Mozilla/5.0\r\nContent-Length: %d\r\n\r\n"
#define HTTP_POST_MULTIPART_HEADER "POST %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\r\nAccept-Language: en-us,en;q=0.5\r\nAccept-Encoding: gzip,deflate\r\nAccept-Charset: ISO-8859-1;q=0.7,*;q=0.7\r\nKeep-Alive: 300\r\nConnection: keep-alive\r\nContent-Type: multipart/form-data; boundary=%s\r\nContent-Length: %d\r\n\r\n"
#define MULTIPART_BOUNDARY "---------------------------24464570528145"
#define HTTP_POST_STREAM_HEADER "POST %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: Mozilla/5.0\r\nAccept: text/xml,application/xml,application/xhtml+xml,text/html;q=0.9,text/plain;q=0.8,image/png,*/*;q=0.5\r\nAccept-Language: en-us,en;q=0.5\r\nAccept-Encoding: gzip,deflate\r\nAccept-Charset: ISO-8859-1;q=0.7,*;q=0.7\r\nKeep-Alive: 300\r\nConnection: close\r\nContent-Type: application/octet-stream; filename=%s\r\nContent-Length: %d\r\n\r\n"

void httpInitReq(HTTP_REQUEST* req, char* url, char* proxy)
{
	memset(req, 0, sizeof(HTTP_REQUEST));
	req->url = url;
	if (proxy && *proxy) req->proxy = proxy;
}

static char* parseURL(char* url, HTTP_REQUEST* param)
{
	char *ptr;
	size_t len;
	char *path;

	if (strncmp(url,"http://",7) != 0 ) { 
		return NULL;
	}
	
	url += 7;
	ptr = strchr(url, '/');
	if (ptr == NULL) {
		path="/";
		len = strlen(url);
	} else {
		len = (int)(ptr - url);
		path = ptr;
	}
	ptr = strrchr(url, ':');
	if (ptr && *(ptr + 1) != '/') {
		unsigned short int port = atoi(ptr + 1);
		if (port) {
			param->port = port;
		}
		len = (int)(ptr - url);
	}
	param->hostname = (char*)malloc(len + 1);
	memcpy(param->hostname, url, len);
	param->hostname[len] = 0;
	if (!param->port) param->port = 80;
	return path;
}
	
#define MAX_HEADER_SIZE 4095

static size_t LoopSend(HTTP_REQUEST* param, char* data, size_t length)
{
	size_t offset = 0;
	size_t bytes;
	do {
		bytes = send(param->sockfd, data + offset, (int)(length - offset), 0);
		if (bytes <= 0) break;
		offset += bytes;
		DEBUG("Sent %d/%d bytes\n", offset, length);
	} while(offset < length);
	return offset;
}

void httpClean(HTTP_REQUEST* param)
{
	if (param->sockfd) {
		closesocket(param->sockfd);
		param->sockfd = 0;
	}
	if (param->buffer) {
		free(param->buffer);
		param->buffer = 0;
	}
	if (param->hostname){
		free(param->hostname);
		param->hostname = 0;
	}
}

int httpRequest(HTTP_REQUEST* param)
{
	char *path;
	struct hostent *target_host;
	int ret = 0;
	int bytes;

	param->state = HS_REQUESTING;
	param->iPayloadSize=0;

	if (param->proxy) {
		parseURL(param->proxy, param);
		path = param->url;
	} else {
		path = parseURL(param->url, param);
		if (!path) return -1;
	}

	if (param->header) free(param->header);
	param->header = malloc(MAX_HEADER_SIZE + 1);
	if ((param->iBytesStart|param->iBytesEnd) == 0) {
		switch (param->method) {
		case HM_GET:
			sprintf(param->header, HTTP_GET_HEADER, "GET",
				path, (param->flags & FLAG_KEEP_ALIVE) ? "Keep-Alive" : "close", param->hostname,"");
			break;
		case HM_HEAD:
			sprintf(param->header, HTTP_GET_HEADER, "HEAD",
				path, (param->flags & FLAG_KEEP_ALIVE) ? "Keep-Alive" : "close", param->hostname,"");
			break;
		case HM_POST:
			sprintf(param->header,HTTP_POST_HEADER, path, param->hostname, param->iPostDataSize);
			break;
		case HM_POST_STREAM: {
			sprintf(param->header,HTTP_POST_STREAM_HEADER, path, param->hostname, param->filename, 0);
			break;
			} break;
		case HM_POST_MULTIPART: {
			size_t bytes = 0;
			int i;
			for (i = 0; i < param->iChunkCount; i++) {
				if (param->chunk[i].type == POSTDATA_STRING) {
					param->chunk[i].length = strlen(param->chunk[i].data);
				}
				bytes += param->chunk[i].length;
				bytes += sizeof(MULTIPART_BOUNDARY) - 1 + 6;
			}
			bytes += sizeof(MULTIPART_BOUNDARY) - 1 + 4;
			sprintf(param->header,HTTP_POST_MULTIPART_HEADER, path, param->hostname, MULTIPART_BOUNDARY, bytes);
			} break;
		}
	} else {
		char tokenRange[48],*p=tokenRange;
		p+=sprintf(p, "Range: bytes=%u-", param->iBytesStart);
		if (param->iBytesEnd>0) {
			sprintf(p,"%u\r\n",param->iBytesEnd);
		} else {
			strcpy(p,"\r\n");
		}
		sprintf(param->header, HTTP_GET_HEADER, path, param->hostname, tokenRange);
	}
	
	do {
		int offset=0;
		int hdrsize = (int)strlen(param->header);
		char *p=NULL;

		if ((target_host = gethostbyname((const char*)param->hostname)) == NULL) {
			ret = -1;
			continue;
		}


		if (!param->sockfd) {
			struct sockaddr_in server_addr;
			memset(&server_addr.sin_zero,0,8);
			server_addr.sin_family = AF_INET;
			server_addr.sin_addr.s_addr = ((struct in_addr *)(target_host->h_addr))->s_addr;
			server_addr.sin_port = htons(param->port);
			DEBUG("Connecting to server...\n");
			if ((param->sockfd = socket(AF_INET,SOCK_STREAM,0)) == -1) {
				DEBUG("Failed to open socket\n");
				ret = -1;
				continue;
			}
			if (connect(param->sockfd,(struct sockaddr *)&server_addr,sizeof(struct sockaddr)) < 0) {
				DEBUG("Failed to connect\n");
				ret = -1;
				continue;
			}
		}
		DEBUG("Sending request...\n");
		if (LoopSend(param, param->header, hdrsize) != hdrsize) break;

		if (param->method == HM_POST) {
			if (LoopSend(param, param->postData, param->iPostDataSize) != param->iPostDataSize) break;
		} else if (param->method == HM_POST_MULTIPART) {
			POST_CHUNK* chunk;
			int ret = 0;
			int i;
			char* sendbuf = (char*)malloc(POST_BUFFER_SIZE);
			for (i = 0; i < param->iChunkCount && param->state != HS_STOPPING; i++) {
				chunk = param->chunk + i;
				bytes = sprintf(sendbuf, "--%s\r\n", MULTIPART_BOUNDARY);
				if (LoopSend(param, sendbuf, bytes) != bytes) break;
				switch (chunk->type) {
				case POSTDATA_STRING:
				case POSTDATA_BINARY:
					if (LoopSend(param, chunk->data, chunk->length) != chunk->length) break;
					break;
				case POSTDATA_FD:
					for(;;) {
						bytes = _read((int)chunk->data, sendbuf, POST_BUFFER_SIZE);
						if (bytes <= 0) break;
						if (LoopSend(param, sendbuf, bytes) != bytes) break;
					}
					break;
				case POSTDATA_CALLBACK:
					for(;;) {
						bytes = (*(PFNPOSTDATACALLBACK)chunk->data)(sendbuf, POST_BUFFER_SIZE);
						if (bytes < 0) {
							param->state = HS_STOPPING;
							break;
						}
						if (bytes == 0) break;
						if (LoopSend(param, sendbuf, bytes) != bytes) break;
					}
					break;
				}
				if (LoopSend(param, "\r\n", 2) != 2) break;
			}
			free(sendbuf);
			LoopSend(param, "--", 2);
			LoopSend(param, MULTIPART_BOUNDARY, sizeof(MULTIPART_BOUNDARY) - 1);
			LoopSend(param, "--\r\n", 4);
		}
	} while(0);

	return ret;
}

int httpGetResponse(HTTP_REQUEST* param)
{
	int receivedBytes;
	int rspHeaderBytes;
	int ret;
	char *p = 0;

	do {
		//receive header
		DEBUG("Receiving response...\n");
		receivedBytes=0;
		for(;;) {
			ret = recv(param->sockfd, param->header + receivedBytes, MAX_HEADER_SIZE-receivedBytes, 0);
			if (ret <= 0) break;
			receivedBytes += ret;
			param->header[receivedBytes] = 0;
			DEBUG("Received %d/%d bytes\n", ret, receivedBytes);
			if ((p = strstr(param->header,"\r\n\r\n"))) {
				*(p+2) = '\0';
				break;
			}
			if (receivedBytes == MAX_HEADER_SIZE) {
				p = param->header + receivedBytes - 4;
			}
		}
		if (!p) {
			DEBUG("Invalid server response\n");
			ret = -1;
			continue;
		} else {
			ret = 0;
			p+=4;
		}
		rspHeaderBytes = (int)(p - param->header);

	//process header
	{
		char *p;
		DEBUG("Response header:\n%s\n", param->header);
		if (p = strstr(param->header,"HTTP/1.")) {
			param->iHttpCode = atoi(p+9);
		}
		if (param->iHttpCode == 404) {
			DEBUG("Invalid response or file not found on server\n");
			closesocket(param->sockfd);
			param->sockfd=0;
			param->state=HS_IDLE;
			free(param->header);
			param->header = 0;
			return -1;
		}
		while ((p = strstr(p, "\r\n"))) {
			char *q;
			q=strchr((p += 2),':');
			if (!q) continue;
			*q = 0;
			if (!_stricmp(p,"Content-length")) {
				param->iPayloadSize=atoi(q+2);
			} else if (!_stricmp(p,"Content-type")) {
				param->contentType = q+2;
			} else if (!_stricmp(p, "Transfer-Encoding")) {
				if (!strncmp(p + 19, "chunked", 7)) {
					param->flags |= FLAG_CHUNKED;
				}
			}
			*q = ':';
		}
	}
	DEBUG("Payload bytes: %d\n",param->iPayloadSize);
	if (param->iPayloadSize == 0)
		param->iPayloadSize = param->iDataSize ? param->iDataSize - 1 : receivedBytes - rspHeaderBytes;
	if (param->flags & FLAG_REQUEST_ONLY) {
		closesocket(param->sockfd);
		param->state=HS_IDLE;
		return 0;
	}
	// receive payload
	if (param->method != HM_HEAD) {
		int iPayLoadBytes;
		int bytes;
		int recvBytes = 0;
		param->state=HS_RECEIVING;
		if (param->buffer) {
			iPayLoadBytes=param->iPayloadSize>=param->iDataSize?param->iDataSize-1:param->iPayloadSize;
		} else {
			DEBUG("Allocating %d bytes for payload buffer\n",param->iPayloadSize);
			param->buffer = calloc(1, param->iPayloadSize+1);
			iPayLoadBytes=param->iPayloadSize;
		}
		recvBytes = receivedBytes - rspHeaderBytes;
		if (recvBytes > 0) {
			DEBUG("Header includes %d bytes of payload\n", recvBytes);
			memcpy(param->buffer, param->header + rspHeaderBytes, recvBytes);
		}
		//receive payload data
		for (; recvBytes < iPayLoadBytes; recvBytes += bytes) {
			int bytesToRecv = iPayLoadBytes - recvBytes;
			DEBUG("Received: %d To receive: %d\n", recvBytes, bytesToRecv);
			bytes = recv(param->sockfd, param->buffer + recvBytes, bytesToRecv, 0);
			if (bytes<=0) {
				DEBUG("End of stream\n");
				break;
			}
		}
		DEBUG("Payload received: %d bytes\n", recvBytes);
		*(param->buffer + recvBytes)=0;
		param->iDataSize = recvBytes;
	}
	
	} while(0);
	if (param->sockfd && !(param->flags & FLAG_KEEP_ALIVE)) {
		closesocket(param->sockfd);
		param->sockfd=0;
	}
	param->state=HS_IDLE;
	if (!(param->flags & FLAG_KEEP_HEADER)) {
		free(param->header);
		param->header = 0;
	}
	return ret;
}

static int fd;
static char fileheader[512];
static int filelen;

static int ReadData(void* buffer, int bufsize)
{
	if (fileheader[0]) {
		int ret = _snprintf(buffer, bufsize, "%s", fileheader);
		fileheader[0] = 0;
		return ret;
	} else {
		return _read(fd, buffer, bufsize);
	}
}

#define FILE_CHUNK_HEADER "Content-Disposition: form-data; name=\"%s\"; filename=\"%s\"\r\n\r\n"

char* PostFile(char* url, char* fieldname, char* filename)
{
	int ret;
	POST_CHUNK chunk;
	HTTP_REQUEST req;
	char *html;

	fd = _open(filename, _O_BINARY | _O_RDONLY);
	if (fd <= 0) return 0;

	req.filename = strrchr(filename, '\\');
	if (!req.filename)
		req.filename = filename;
	else
		req.filename++;

	filelen = _lseek(fd, 0, SEEK_END);
	chunk.data = (void*)ReadData;
	chunk.length = filelen + _snprintf(fileheader, sizeof(fileheader), FILE_CHUNK_HEADER, fieldname, req.filename);
	chunk.type = POSTDATA_CALLBACK;
	_lseek(fd, 0, SEEK_SET );
	memset(&req, 0, sizeof(req));
	req.url = url;
	req.method = HM_POST_MULTIPART;
	req.chunk = &chunk;
	req.iChunkCount = 1;
	ret = httpRequest(&req);
	_close(fd);
	if (!ret) {
		ret = httpGetResponse(&req);
	}
	html = !ret ? _strdup(req.buffer) : 0;
	httpClean(&req);
	return html;
}

int PostFileStream(char* url, char* filename)
{
	int ret;
	HTTP_REQUEST req;
	
	char buf[1024];
	int bytes;

	memset(&req, 0, sizeof(req));

	fd = _open(filename, _O_BINARY | _O_RDONLY);
	if (fd <= 0) return 0;

	req.filename = strrchr(filename, '\\');
	if (!req.filename)
		req.filename = filename;
	else
		req.filename++;

	req.url = url;
	req.method = HM_POST_STREAM;
	req.iChunkCount = 1;
	ret = httpRequest(&req);

	while ((bytes = _read(fd, buf, sizeof(buf))) > 0 
		&& LoopSend(&req, buf, bytes) == bytes);

	_close(fd);
	return ret;
}
