///////////////////////////////////////////////////////////////////////
//
// httpapi.h
//
// External API header file for http protocol
//
///////////////////////////////////////////////////////////////////////

#ifndef _HTTPAPI_H_
#define _HTTPAPI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

#define VER_MAJOR 0
#define VER_MINOR 8

#ifndef min
#define min(x,y) (x>y?y:x)
#endif

#ifndef NODEBUG
#define DBG printf
#else
#define DBG
#endif
#define LOG_ERR 1

#ifdef WIN32
#ifndef pthread_t
#endif
#endif
#define ASSERT
#define GETDWORD(ptrData) (*(DWORD*)(ptrData))
#define SETDWORD(ptrData,data) (*(DWORD*)(ptrData)=data)
#define GETWORD(ptrData) (*(WORD*)(ptrData))
#define SETWORD(ptrData,data) (*(WORD*)(ptrData)=data)
#ifndef BIG_ENDINE
#define DEFDWORD(char1,char2,char3,char4) (char1+(char2<<8)+(char3<<16)+(char4<<24))
#define DEFWORD(char1,char2) (char1+(char2<<8))
#else
#define DEFDWORD(char1,char2,char3,char4) (char4+(char3<<8)+(char2<<16)+(char1<<24))
#define DEFWORD(char1,char2) (char2+(char1<<8))
#endif

///////////////////////////////////////////////////////////////////////
// Public definitions
///////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C" {
#endif

// file types
typedef enum {
  HTTPFILETYPE_UNKNOWN = 0,
  HTTPFILETYPE_HTML,
  HTTPFILETYPE_XML,
  HTTPFILETYPE_TEXT,
  HTTPFILETYPE_CSS,
  HTTPFILETYPE_PNG,
  HTTPFILETYPE_JPEG,
  HTTPFILETYPE_GIF,
  HTTPFILETYPE_SWF,
  HTTPFILETYPE_MPA,
  HTTPFILETYPE_MPEG,
  HTTPFILETYPE_AVI,
  HTTPFILETYPE_MP4,
  HTTPFILETYPE_MOV,
  HTTPFILETYPE_JS,
  HTTPFILETYPE_OCTET,
  HTTPFILETYPE_STREAM,
} HttpFileType;

#define MAXPOSTPARAMS 50
#define MAXPOSTREDIRECTFILENAME (200)

/////////////////////////////////////////////////////////////////////////////
// typedefs
/////////////////////////////////////////////////////////////////////////////

typedef struct _tagPostParam {
  //  char* pchPageName;
  struct {
    char* pchParamName;
    char* pchParamValue;
  } stParams[MAXPOSTPARAMS];
  int iNumParams;
  char chFilename[MAXPOSTREDIRECTFILENAME];
} PostParam;

// multipart file upload post (per socket) structure
typedef struct {
  char pchBoundaryValue[80];
  OCTET oFileuploadStatus;
  int iWriteLocation;
  PostParam pp;
  char *pchFilename;
} HttpMultipart;

typedef struct _tagSubstParam {
  char* pchParamName;
  char* pchParamValue;	// returned
  int iMaxValueBytes;
} SubstParam;

#define FLAG_REQUEST_GET	0x1
#define FLAG_REQUEST_POST	0x2
#define FLAG_CONN_CLOSE		0x10
#define FLAG_SUBST			0x20
#define FLAG_AUTHENTICATION	0x40
#define FLAG_MORE_CONTENT	0x80
#define FLAG_TO_FREE		0x100

#define FLAG_DATA_FILE		0x10000
#define FLAG_DATA_RAW		0x20000
#define FLAG_DATA_FD		0x40000
#define FLAG_DATA_REDIRECT	0x80000
#define FLAG_DATA_STREAM	0x100000

#define FLAG_RECEIVING		0x80000000
#define FLAG_SENDING		0x40000000

#define SETFLAG(hs,bit) (hs->flags|=(bit));
#define CLRFLAG(hs,bit) (hs->flags&=~(bit));
#define ISFLAGSET(hs,bit) ((hs->flags&(bit)))

typedef union {
	unsigned long laddr;
	unsigned short saddr[2];
	unsigned char caddr[4];
} IP;

typedef struct {
	IP ipAddr;
	int iStartByte;
	unsigned char *pucPath;
	int ofReferer;
	int ofHost;
	int siHeaderSize;
	unsigned char* pucPayload;
} HttpRequest;

typedef struct {
	int iSentBytes;
	int iContentLength;
	HttpFileType fileType;
	int iBufferSize;			// the size of buffer pucData pointing to
} HttpResponse;

typedef struct {
	char *name;
	char *value;
} HttpVariables;

// Callback function protos
typedef int (*PFNPOSTCALLBACK)(PostParam*);
typedef int (*PFNSUBSTCALLBACK)(SubstParam*);
typedef int (*PFNFILEUPLOADCALLBACK)(char*, OCTET, OCTET*, DWORD);

typedef enum {
	MW_INIT = 0,
	MW_UNINIT,
} MW_EVENT;

typedef int (*MW_EVENT_HANDLER)(MW_EVENT msg, void* arg);

typedef struct {
	time_t startTime;
	int clientCount;
	int clientCountMax;
	int reqCount;
	int reqGetCount;
	int fileSentCount;
	int fileSentBytes;
	int varSubstCount;
	int urlProcessCount;
	int timeOutCount;
	int authFailCount;
	int reqPostCount;
	int fileUploadCount;
} HttpStats;

#define HTTP_BUFFER_SIZE (4*1024 /*bytes*/)

// per connection/socket structure
typedef struct _HttpSocket{
	struct _HttpSocket *next;
	SOCKET socket;
	int fd;
	HttpRequest request;
	HttpResponse response;
	unsigned char *pucData;
	int iDataLength;

	unsigned long flags;
	void* ptr;
	time_t tmAcceptTime;
	time_t tmExpirationTime;
	int iRequestCount;

	unsigned char buffer[HTTP_BUFFER_SIZE];
} HttpSocket;

typedef struct {
	void* hp;
	HttpSocket* hs;
	char *pucRequest;
	HttpVariables* pxVars;
	int iVarCount;
	char *pucHeader;
	char *pucBuffer;
	int iDataBytes;
	int iContentBytes;
	int iSentBytes;
	HttpFileType fileType;
} UrlHandlerParam;

typedef int (*PFNURLCALLBACK)(UrlHandlerParam*);

typedef struct {
	char* pchUrlPrefix;
	PFNURLCALLBACK pfnUrlHandler;
	MW_EVENT_HANDLER pfnEventHandler;
} UrlHandler;

#define FLAG_DIR_LISTING 1
#define FLAG_LOCAL_BIND 2

typedef struct _httpParam {
	HttpSocket *phsSocketHead;				/* head of the socket linked list */
	int   bKillWebserver; 
	int   bWebserverRunning; 
	unsigned int flags;
	SOCKET listenSocket;
	int httpPort;
	int maxReqPerConn;		/* maximum requests on one connection */
	int maxClients;
	int socketRcvBufSize;	/* socket receive buffer size in KB */
	char *pchWebPath;
	UrlHandler *pxUrlHandler;		/* pointer to URL handler array */
	// substitution callback
	PFNSUBSTCALLBACK pfnSubst;
	// post callbacks
	PFNFILEUPLOADCALLBACK pfnFileUpload;
	PFNPOSTCALLBACK pfnPost;
	DWORD dwAuthenticatedNode;
	time_t tmAuthExpireTime;
	pthread_t tidHttpThread;
	HttpStats stats;
} HttpParam;

typedef struct {
	char* pchRootPath;
	char* pchHttpPath;
	char cFilePath[MAX_PATH];
	char* pchExt;
	int fTailSlash;
} HttpFilePath;

///////////////////////////////////////////////////////////////////////
// Return codes
///////////////////////////////////////////////////////////////////////
// for post callback
#define WEBPOST_OK                (0)
#define WEBPOST_AUTHENTICATED     (1)
#define WEBPOST_NOTAUTHENTICATED  (2)
#define WEBPOST_AUTHENTICATIONON  (3)
#define WEBPOST_AUTHENTICATIONOFF (4)

// for multipart file uploads
#define HTTPUPLOAD_MORECHUNKS     (0)
#define HTTPUPLOAD_FIRSTCHUNK     (1)
#define HTTPUPLOAD_LASTCHUNK      (2)

///////////////////////////////////////////////////////////////////////
// Public functions
///////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
// mwServerStart. Startup the webserver
///////////////////////////////////////////////////////////////////////
int mwServerStart(HttpParam* hp);

///////////////////////////////////////////////////////////////////////
// mwServerShutdown. Shutdown the webserver (closes connections and
// releases resources)
///////////////////////////////////////////////////////////////////////
int mwServerShutdown(HttpParam* hp);

///////////////////////////////////////////////////////////////////////
// mwSetRcvBufSize. Change the TCP windows size of acceped sockets
///////////////////////////////////////////////////////////////////////
int mwSetRcvBufSize(WORD wSize);

///////////////////////////////////////////////////////////////////////
// mwPostRegister. Specify the callback to be called when a POST is
// recevied.
///////////////////////////////////////////////////////////////////////
PFNPOSTCALLBACK mwPostRegister(PFNPOSTCALLBACK);

///////////////////////////////////////////////////////////////////////
// mwFileUploadRegister. Specify the callback to be called whenever the 
// server has the next data chunk available from a multipart file upload.
///////////////////////////////////////////////////////////////////////
PFNFILEUPLOADCALLBACK mwFileUploadRegister(PFNFILEUPLOADCALLBACK);

///////////////////////////////////////////////////////////////////////
// Default subst, post and file-upload callback processing
///////////////////////////////////////////////////////////////////////
int DefaultWebSubstCallback(SubstParam* sp);
int DefaultWebPostCallback(PostParam* pp);
int DefaultWebFileUploadCallback(char *pchFilename,
                                 OCTET oFileuploadStatus,
                                 OCTET *poData, 
                                 DWORD dwDataChunkSize);

int mwGetHttpDateTime(time_t tm, char *buf);
int mwGetLocalFileName(HttpFilePath* hfp);
char* mwGetVarValue(HttpVariables* vars, char *varname);
int mwGetVarValueInt(HttpVariables* vars, char *varname, int defval);
int mwParseQueryString(UrlHandlerParam* up);

#ifdef __cplusplus
}
#endif

#endif // _HTTPAPI_H

////////////////////////// END OF FILE ////////////////////////////////
