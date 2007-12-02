/*******************************************************************
* HTTP helper
* Distributed under GPL license
* Copyright (c) 2005-06 Stanley Huang <stanleyhuangyc@yahoo.com.cn>
* All rights reserved.
*******************************************************************/

#define FLAG_REQUEST_ONLY 0x1
#define FLAG_KEEP_ALIVE 0x2
#define FLAG_KEEP_HEADER 0x4
#define FLAG_CHUNKED 0x8

typedef enum {
	HS_IDLE=0,
	HS_REQUESTING,
	HS_RECEIVING,
	HS_STOPPING,
} HTTP_STATES;

typedef enum {
	HM_GET = 0,
	HM_HEAD,
	HM_POST,
	HM_POST_STREAM,
	HM_POST_MULTIPART,
} HTTP_METHOD;

#define POSTDATA_STRING 0
#define POSTDATA_BINARY 1
#define POSTDATA_FD 2
#define POSTDATA_CALLBACK 3

#define POST_BUFFER_SIZE 1024
typedef int (*PFNPOSTDATACALLBACK)(void* buffer, int bufsize);

typedef struct {
	void* data;
	int type;
	size_t length;
} POST_CHUNK;

typedef struct {
	int sockfd;
	HTTP_METHOD method;
	HTTP_STATES state;
	char *url;
	char *proxy;
	unsigned short flags;
	unsigned short port;
	char* header;
	char* buffer;
	char* postData;
	char* hostname;
	int iPostDataSize;
	int iDataSize;
	int iBytesStart;
	int iBytesEnd;
	int iPayloadSize;
	//info parsed from response header
	char* contentType;
	int iHttpCode;
	//Multipart-Post 
	POST_CHUNK* chunk;
	int iChunkCount;
	char* filename;
} HTTP_REQUEST;

#ifdef __cplusplus
extern "C" {
#endif

void httpInitReq(HTTP_REQUEST* req, char* url, char* proxy);
int httpRequest(HTTP_REQUEST* param);
void HttpClean(HTTP_REQUEST* param);
char* PostFile(char* url, char* fieldname, char* filename);
int PostFileStream(char* url, char* filename);

#ifdef __cplusplus
}
#endif
