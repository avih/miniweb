/////////////////////////////////////////////////////////////////////////////
//
// httpint.h
//
// http implementation internal header file
//
/////////////////////////////////////////////////////////////////////////////
#ifndef _HTTPINT_H_
#define _HTTPINT_H_

#include "httpapi.h"

/////////////////////////////////////////////////////////////////////////////
// defines
/////////////////////////////////////////////////////////////////////////////

// HTTP messages/part messages
#define HTTP200_HEADER "HTTP/1.1 %s\r\nServer: MiniWeb\r\nCache-control: no-cache\r\nPragma: no-cache\r\nAccept-Ranges: bytes\r\nKeep-Alive: timeout=%d, max=%d\r\nConnection: %s\r\nLast-Modified: "
#define HTTP200_HDR_EST_SIZE ((sizeof(HTTP200_HEADER)+128)&(-4))
#define HTTP404_HEADER "HTTP/1.1 404 Not Found\r\nServer: MiniWeb\r\nConnection: Keep-Alive\r\nContent-length: 180\r\nContent-Type: text/html\r\n\r\n<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this server.</p><hr><i>MiniWeb - the tiny HTTPd written by Stanley Huang</i></body></html>"
#define HTTPBODY_REDIRECT "<html><head><meta http-equiv=\"refresh\" content=\"0; URL=%s\"></head><body></body></html>"
#define HTTPTYPE_HTML "text/html"
#define HTTPTYPE_XML "text/xml"
#define HTTPTYPE_TEXT "text/plain"
#define HTTPTYPE_GIF "image/gif"
#define HTTPTYPE_JPEG "image/jpeg"
#define HTTPTYPE_PNG "image/png"
#define HTTPTYPE_JS "application/x-javascript"
#define HTTPTYPE_CSS "text/css"
#define HTTPTYPE_SWF "application/x-shockwave-flash"
#define HTTPTYPE_MPA "audio/mpeg"
#define HTTPTYPE_MPEG "video/mpeg"
#define HTTPTYPE_AVI "video/x-msvideo"
#define HTTPTYPE_QUICKTIME "video/quicktime"
#define HTTPTYPE_OCTET "application/octet-stream"
#define HTTPTYPE_STREAM "application/x-datastream"
#define HTTP_CONTENTLENGTH "Content-Length:"
#define HTTP_MULTIPARTHEADER "multipart/form-data"
#define HTTP_MULTIPARTCONTENT "Content-Disposition: form-data; name="
#define HTTP_MULTIPARTBOUNDARY "boundary="
#define HTTP_FILENAME "filename="
#define HTTP_GET DEFDWORD('G','E','T',' ')
#define HTTP_POST DEFDWORD('P','O','S','T')
#define HTTP_HEADEREND DEFDWORD('\r','\n','\r','\n')
#define HTTP_HEADEREND_STR "\r\n\r\n"
#define HTTP_SUBST_PATTERN (WORD)(('$' << 8) + '$')

// Define file extensions
#define FILEEXT_HTM DEFDWORD('H','T','M',0)
#define FILEEXT_XML DEFDWORD('X','M','L',0)
#define FILEEXT_TEXT DEFDWORD('T','X','T',0)
#define FILEEXT_GIF DEFDWORD('G','I','F',0)
#define FILEEXT_JPG DEFDWORD('J','P','G',0)
#define FILEEXT_PNG DEFDWORD('P','N','G',0)
#define FILEEXT_CSS DEFDWORD('C','S','S',0)
#define FILEEXT_JS DEFDWORD('J','S',0,0)
#define FILEEXT_SWF DEFDWORD('S','W','F',0)
#define FILEEXT_HTML DEFDWORD('H','T','M','L')
#define FILEEXT_MPG DEFDWORD('M','P','G',0)
#define FILEEXT_MPEG DEFDWORD('M','P','E','G')
#define FILEEXT_MPA DEFDWORD('M','P',('3' - 32),0)
#define FILEEXT_AVI DEFDWORD('A','V','I',0)
#define FILEEXT_MP4 DEFDWORD('M','P',('4' - 32),0)
#define FILEEXT_MOV DEFDWORD('M','O','V',0)

// Settings for http server
#define HTTP_EXPIRATION_TIME (30/*secs*/)
#define HTTP_KEEPALIVE_TIME (15/*secs*/)
#define MAX_RECV_RETRIES (3/*times*/)
#define HTTPAUTHTIMEOUT   (300/*secs*/)
#define HTTPSUBSTEXPANSION (0/*bytes*/)
#define HTTPHEADERSIZE (512/*bytes*/)
#define HTTPSMALLBUFFER (256/*bytes*/)
#define HTTPMAXRECVBUFFER HTTP_BUFFER_SIZE
#define HTTPUPLOAD_CHUNKSIZE (1024/*bytes*/)
#define MAX_REQUEST_PATH_LEN (512/*bytes*/)
#define MAX_REQUEST_SIZE (2*1024 /*bytes*/)

#ifndef WIN32
#define SLASH '/'
#else
#define SLASH '\\'
#endif

#ifdef NOCONSOLE
#define SYSLOG
#else
#define SYSLOG fprintf
#endif

/////////////////////////////////////////////////////////////////////////////
// local helper function prototypes
/////////////////////////////////////////////////////////////////////////////
SOCKET _mwAcceptSocket(HttpParam* hp, struct sockaddr_in *sinaddr);
int _mwProcessReadSocket(HttpParam* hp, HttpSocket* phsSocket);
int _mwProcessWriteSocket(HttpParam *hp, HttpSocket* phsSocket);
void _mwCloseSocket(HttpParam* hp, HttpSocket* phsSocket);
int _mwStartSendFile(HttpParam* hp, HttpSocket* phsSocket);
int _mwSendFileChunk(HttpParam *hp, HttpSocket* phsSocket);
void _mwProcessPost(HttpSocket* phsSocket);
void _mwProcessMultipartPost(HttpSocket* phsSocket);
int _mwSubstVariables(HttpParam* hp, char* pchData, int iLength, int* piBytesUsed);
char* _mwStrStrNoCase(char* pchHaystack, char* pchNeedle);
void _mwProcessPostVars(HttpSocket* phsSocket,
			  int iContentOffset, int iContentLength);
void _mwRedirect(HttpSocket* phsSocket, char* pchFilename);
int _mwSendRawDataChunk(HttpParam *hp, HttpSocket* phsSocket);
int _mwStartSendRawData(HttpParam *hp, HttpSocket* phsSocket);
void* _mwHttpThread(HttpParam* hp);
int _mwGetToken(char* pchBuffer,int iTokenNumber,char** ppchToken); 
void _mwDecodeString(char* s);
__inline char _mwDecodeCharacter(char* pchEncodedChar);
int _mwLoadFileChunk(HttpParam *hp, HttpSocket* phsSocket);
OCTET* _mwFindMultipartBoundary(OCTET *poHaystack, int iHaystackSize, 
                                  OCTET *poNeedle);
void _mwNotifyPostVars(HttpSocket* phsSocket, PostParam *pp);
BOOL _mwCheckAuthentication(HttpSocket* phsSocket);
int _mwStartSendMemoryData(HttpSocket* phsSocket);
int _GetContentType(char *pchFilename);
int _mwCheckAccess(HttpSocket* phsSocket);
int _mwGetContentType(char *pchExtname);
int _mwSendHttpHeader(HttpSocket* phsSocket);
char* _mwStrDword(char* pchHaystack, DWORD dwSub, DWORD dwCharMask);
SOCKET _mwStartListening(HttpParam* hp);
int _mwParseHttpHeader(HttpSocket* phsSocket);
__inline int _mwStrCopy(char *dest, char *src);
#endif
////////////////////////// END OF FILE //////////////////////////////////////
