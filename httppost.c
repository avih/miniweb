/////////////////////////////////////////////////////////////////////////////
//
// http.c
//
// MiniWeb HTTP POST implementation for 
//
/////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "httppil.h"
#include "httpapi.h"
#include "httpint.h"

#ifdef HTTPPOST

extern HttpParam g_httpParam;

////////////////////////////////////////////////////////////////////////////
// _mwFindMultipartBoundary
// Searches a memory buffer for a multi-part boundary string
////////////////////////////////////////////////////////////////////////////
OCTET* _mwFindMultipartBoundary(OCTET *poHaystack, int iHaystackSize, OCTET *poNeedle)
{
  int i;
  int iNeedleLength = strlen(poNeedle);
  
  ASSERT(iNeedleLength > 0);
  for (i=0; i <= (iHaystackSize-iNeedleLength-2); i++){
    if (*(poHaystack+i)==0x0d && *(poHaystack+i+1)==0x0a &&
        memcmp(poHaystack+i+2, poNeedle, iNeedleLength) == 0) {
      return (poHaystack+i);
    }
  }
  
  return NULL;
}

////////////////////////////////////////////////////////////////////////////
// mwFileUploadRegister
// Register a MULTIPART FILE UPLOAD callback
////////////////////////////////////////////////////////////////////////////
PFNFILEUPLOADCALLBACK mwFileUploadRegister(PFNFILEUPLOADCALLBACK pfnUploadCb) 
{
  PFNFILEUPLOADCALLBACK pfnUploadPrevCb=g_httpParam.pfnFileUpload;
  
  // save new CB
  if (pfnUploadCb==NULL) return NULL;
  g_httpParam.pfnFileUpload=pfnUploadCb;
  
  // return previous CB (so app can chain onto it)
  return pfnUploadPrevCb;
}

////////////////////////////////////////////////////////////////////////////
// mwPostRegister
// Register a POST callback
////////////////////////////////////////////////////////////////////////////
PFNPOSTCALLBACK mwPostRegister(PFNPOSTCALLBACK pfnPostCb) 
{
  PFNPOSTCALLBACK pfnPostPrevCb=g_httpParam.pfnPost;

  // save new CB
  if (pfnPostCb==NULL) return NULL;
  g_httpParam.pfnPost=pfnPostCb;

  // return previous CB (so app can chain onto it)
  return pfnPostPrevCb;
}

////////////////////////////////////////////////////////////////////////////
// _mwNotifyPostVars
// Process posted variables and do callback with parameter list
////////////////////////////////////////////////////////////////////////////
void _mwNotifyPostVars(HttpSocket* phsSocket, PostParam *pp)
{
  // if found any vars
  if (pp->iNumParams>0) {
    int iReturn;
    
    // call app callback to process post vars
    //ASSERT(g_httpParam.pfnPost!=NULL);
    iReturn=(*g_httpParam.pfnPost)(pp);
    
#ifdef HTTPAUTH
    switch(iReturn) {
    case WEBPOST_AUTHENTICATIONON:
      DEBUG("Http authentication on\n");
	  SETFLAG(phsSocket,FLAG_AUTHENTICATION)
      break;
    case WEBPOST_AUTHENTICATIONOFF:
      DEBUG("Http authentication off\n");
	  CLRFLAG(phsSocket,FLAG_AUTHENTICATION)
      break;
    case WEBPOST_AUTHENTICATED:
      {
        struct sockaddr_in sinAddress;
        socklen_t sLength=sizeof(struct sockaddr_in);
        getpeername(phsSocket->socket,
                    (struct sockaddr*)&sinAddress,&sLength);
        
        g_httpParam.dwAuthenticatedNode=ntohl(sinAddress.sin_addr.s_addr);
        
        DEBUG("Http authenticated node %s\n",
               inet_ntoa(sinAddress.sin_addr));
        
        // Set authentication period
        g_httpParam.tmAuthExpireTime = time(NULL) + HTTPAUTHTIMEOUT;
      }
      break;
    case WEBPOST_NOTAUTHENTICATED:
      {
        struct sockaddr_in sinAddress;
        socklen_t sLength=sizeof(struct sockaddr_in);
        getpeername(phsSocket->socket,
                    (struct sockaddr*)&sinAddress,&sLength); 
        DEBUG("Http authentication fail! (%s NOT authenticated)\n",
               inet_ntoa(sinAddress.sin_addr));
        g_httpParam.dwAuthenticatedNode=0;
      }
      break;
    }
#endif
  }
  
  // was a redirect filename returned
  if (strlen(pp->chFilename)>0) {
    // redirect to specified file
    _mwRedirect(phsSocket, pp->chFilename);
  } else {
    // redirect to index page
    _mwRedirect(phsSocket, "/");
  }
}

////////////////////////////////////////////////////////////////////////////
// _mwProcessMultipartPost
// Process a multipart POST request
////////////////////////////////////////////////////////////////////////////
void _mwProcessMultipartPost(HttpSocket* phsSocket)
{
  int sLength;
  char *pchBoundarySearch = NULL;
  HttpMultipart *pxMP = (HttpMultipart*)phsSocket->ptr;
  
  if (phsSocket == NULL || pxMP == NULL) {
    _mwCloseSocket(phsSocket);
    return;
  }
  
  // Grab more POST data from the socket
  sLength = recv(phsSocket->socket, 
                 phsSocket->buffer + pxMP->iWriteLocation,
                 HTTPMAXRECVBUFFER - pxMP->iWriteLocation, 
                 0);
  
  if (sLength < 0) {
    DEBUG("Socket closed by peer\n");
    _mwCloseSocket(phsSocket);
    return;
  }
  else if (sLength > 0) {
    // reset expiration timer
    phsSocket->tmExpirationTime=time(NULL)+HTTP_EXPIRATION_TIME;
  }
  
  pxMP->iWriteLocation += (sLength > 0 ? sLength : 0);
  //ASSERT(pxMP->iWriteLocation <= HTTPMAXRECVBUFFER);
  
  // Search new data for boundary indicator
  pchBoundarySearch = _mwFindMultipartBoundary(phsSocket->buffer, 
                                                 HTTPMAXRECVBUFFER, 
                                                 pxMP->pchBoundaryValue);
  
  while (pchBoundarySearch != NULL) {
    if (pxMP->pchFilename != NULL) {
      // It's the last chunk of the posted file
      // Warning: MAY BE BIGGER THAN HTTPUPLOAD_CHUNKSIZE
      pxMP->oFileuploadStatus |= HTTPUPLOAD_LASTCHUNK;
      (*g_httpParam.pfnFileUpload)(pxMP->pchFilename,
                         pxMP->oFileuploadStatus,
                         phsSocket->buffer, 
                         (DWORD)pchBoundarySearch - (DWORD)phsSocket->buffer);
      pxMP->pchFilename = NULL;
      
      DEBUG("Multipart file POST on socket %d complete\n",
                   phsSocket->socket);
    }
    
    else {
      char *pchStart = _mwStrStrNoCase(phsSocket->buffer, HTTP_MULTIPARTCONTENT);
      char *pchEnd = _mwStrDword(phsSocket->buffer, HTTP_HEADEREND, 0);
      char *pchFilename = _mwStrStrNoCase(phsSocket->buffer, HTTP_FILENAME);
      
      if (pchStart == NULL || pchEnd == NULL) {
        DEBUG("Waiting for multipart header description on socket %d\n",
                     phsSocket->socket);
        break;
      }
      
      if (pchFilename == NULL || 
          pchFilename > pchEnd) {    // check filename belongs to this variable
        // It's a normal (non-file) var
        // check we have the entire section (up to start of next boundary)
        pchFilename = NULL;
        if (strstr(pchEnd+4, "\r\n") == NULL) {
          DEBUG("Waiting for multipart variable value on socket %d\n",
                       phsSocket->socket);
          break;
        }
      }
      
      pchStart+=strlen(HTTP_MULTIPARTCONTENT)+1; // move past first quote
      pchEnd=strchr(pchStart,0x22);              // find end quote
      
      // Is peer authenticated to post this var?
	  if (
#ifdef HTTPAUTH
		_mwCheckAuthentication(phsSocket) ||
#endif
		(*pchStart=='.')) {
        
        pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamName = 
          calloc(pchEnd-pchStart+1, sizeof(char));
        memcpy(pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamName, pchStart,
               pchEnd-pchStart);
        
        if (pchFilename!=NULL) {
          // use filename as var value
          pchStart=pchFilename+strlen(HTTP_FILENAME)+1;  // move past first quote
          pchEnd=strchr(pchStart,0x22);                  // find end quote
        } else {
          // use data as var value
          pchStart=_mwStrDword(pchEnd, HTTP_HEADEREND, 0) + 4;
          pchEnd=strstr(pchStart,"\r\n");
        }
        
        pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamValue = 
          calloc(pchEnd-pchStart+1, sizeof(char));  
        memcpy(pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamValue, pchStart, 
               pchEnd-pchStart);
        
        DEBUG("Http multipart POST var %d [%s]=[%s]\n",
               pxMP->pp.iNumParams,
               pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamName,
               pxMP->pp.stParams[pxMP->pp.iNumParams].pchParamValue);
        
        pxMP->pp.iNumParams++;
        
        if (pchFilename!=NULL) {
          pxMP->pchFilename = pxMP->pp.stParams[pxMP->pp.iNumParams-1].pchParamValue;
          
          // shift to start of file data
          pxMP->oFileuploadStatus = HTTPUPLOAD_FIRSTCHUNK;
          pchEnd=_mwStrDword(pchFilename, HTTP_HEADEREND, 0) + 4;  //move past "\r\n\r\n"
          pxMP->iWriteLocation -= (DWORD)pchEnd - (DWORD)phsSocket->buffer;
          memmove(phsSocket->buffer, pchEnd, pxMP->iWriteLocation);
          memset(phsSocket->buffer + pxMP->iWriteLocation, 0,
                HTTPMAXRECVBUFFER - pxMP->iWriteLocation);
          break;
        } 
        else {
          // move to start of next boundary indicator
          pchBoundarySearch = pchEnd;
        }
      }
    }
    
    // Shift to start of next boundary section
    pxMP->iWriteLocation -= (DWORD)pchBoundarySearch - (DWORD)phsSocket->buffer;
    memmove(phsSocket->buffer, pchBoundarySearch, pxMP->iWriteLocation);
    memset(phsSocket->buffer + pxMP->iWriteLocation, 0, HTTPMAXRECVBUFFER - pxMP->iWriteLocation);
    
    // check if this is the last boundary indicator?
    if (strncmp(phsSocket->buffer + strlen(pxMP->pchBoundaryValue) + 2, "--\r\n",4) == 0) {
      // yes, we're all done
      int i;
      
      _mwNotifyPostVars(phsSocket, &(pxMP->pp));
      
      // clear multipart structure
      for (i=0; i<pxMP->pp.iNumParams; i++) {
        free(pxMP->pp.stParams[i].pchParamName);
        free(pxMP->pp.stParams[i].pchParamValue);
      }
      free((HttpMultipart*)phsSocket->ptr);
      (HttpMultipart*)phsSocket->ptr = NULL;
      
      DEBUG("Multipart POST on socket %d complete!\n",
                   phsSocket->socket);
      
      return;
    }
    
    // Search for next boundary indicator
    pchBoundarySearch = _mwFindMultipartBoundary(phsSocket->buffer, 
                                                   HTTPMAXRECVBUFFER, 
                                                   pxMP->pchBoundaryValue);
  }
  
  // check if buffer is full
  if (pxMP->iWriteLocation == HTTPMAXRECVBUFFER) {
    if (pxMP->pchFilename != NULL) {
      // callback with next chunk of posted file
      (*g_httpParam.pfnFileUpload)(pxMP->pchFilename,
                         pxMP->oFileuploadStatus,
                         phsSocket->buffer, 
                         HTTPUPLOAD_CHUNKSIZE);
      pxMP->oFileuploadStatus = HTTPUPLOAD_MORECHUNKS;
      pxMP->iWriteLocation -= HTTPUPLOAD_CHUNKSIZE;
      memmove(phsSocket->buffer, phsSocket->buffer + HTTPUPLOAD_CHUNKSIZE, 
              HTTPMAXRECVBUFFER - HTTPUPLOAD_CHUNKSIZE);
      memset(phsSocket->buffer + HTTPUPLOAD_CHUNKSIZE, 0, HTTPMAXRECVBUFFER - HTTPUPLOAD_CHUNKSIZE);
    } 
    else {
      // error, posted variable too large?
      _mwCloseSocket(phsSocket);
    }
  }
  
  return;
} // end of _mwProcessMultipartPost

////////////////////////////////////////////////////////////////////////////
// _mwProcessPostVars
// Extract and process POST variables
// NOTE: the function damages the recvd data
////////////////////////////////////////////////////////////////////////////
void _mwProcessPostVars(HttpSocket* phsSocket,
                          int iContentOffset,
                          int iContentLength)
{
  BOOL bAuthenticated;
  
#ifdef HTTPAUTH
  bAuthenticated=_mwCheckAuthentication(phsSocket);
#else
  bAuthenticated=TRUE;
#endif

  //ASSERT(iContentOffset+iContentLength<=phsSocket->iDataLength);

  // extract the posted vars
  if (g_httpParam.pfnPost!=NULL) {
    int i;
    char* pchPos;
    char* pchVar=phsSocket->buffer+iContentOffset;
    PostParam pp;
    
    // init number of param block
    memset(&pp, 0, sizeof(PostParam));
    
    // null terminate content data
    *(pchVar+iContentLength)='\0';
    
    // process each param
    for (i=0;i<MAXPOSTPARAMS;i++) {
      // find =
      pchPos=strchr(pchVar,'=');
      if (pchPos==NULL) {
        break;
      }
      // terminate var name and add to parm list
      *pchPos='\0'; 
      pp.stParams[pp.iNumParams].pchParamName=pchVar;
      
      // terminate var value and add to parm list
      pp.stParams[pp.iNumParams].pchParamValue=pchPos+1;
      pchPos=strchr(pchPos+1,'&');
      if (pchPos!=NULL) {
        *pchPos='\0'; // null term current value
      }
      
      // if not authenticated then only process vars starting with .
      if (bAuthenticated || 
          (*pp.stParams[pp.iNumParams].pchParamName=='.')) {
        // convert any encoded characters
        _mwDecodeString(pp.stParams[pp.iNumParams].pchParamValue);
        
        DEBUG("Http POST var %d [%s]=[%s]\n",
               pp.iNumParams,
               pp.stParams[pp.iNumParams].pchParamName,
               pp.stParams[pp.iNumParams].pchParamValue);
        
        pp.iNumParams++;
      } else {
        DEBUG("Http POST var [%s]=[%s] skipped - not authenticated\n",
               pp.stParams[pp.iNumParams].pchParamName,
               pp.stParams[pp.iNumParams].pchParamValue);
      }
      
      // if last var then quit
      if (pchPos==NULL) {
        break;
      }
      
      // move to next var
      pchVar=pchPos+1;
    }

    // process and callback with list of vars
    _mwNotifyPostVars(phsSocket, &pp);

  } else {
    // redirect to index page
    _mwRedirect(phsSocket, "/");
  }
} // end of _mwProcessPostVars

////////////////////////////////////////////////////////////////////////////
// _mwProcessPost
// Process a POST request 
////////////////////////////////////////////////////////////////////////////
void _mwProcessPost(HttpSocket* phsSocket)
{
  int iContentLength=-1;
  int iHeaderLength=0;
  
  //ASSERT(phsSocket->buffer!=NULL);
  
  // null terminate the buffer
  *(phsSocket->buffer+phsSocket->iDataLength)=0;
  
  // find content length
  {
    char* pchContentLength;
    
    pchContentLength=strstr(phsSocket->buffer,
                                       HTTP_CONTENTLENGTH);
    if (pchContentLength!=NULL) {
      pchContentLength+=strlen(HTTP_CONTENTLENGTH);
      iContentLength=atoi(pchContentLength);
    }
  }
  
  // check if content length found
  if (iContentLength>0) {
    
    // check if this is a multipart POST
    if ((HttpMultipart*)phsSocket->ptr == NULL) {
      char *pchMultiPart = _mwStrStrNoCase(phsSocket->buffer, 
                                             HTTP_MULTIPARTHEADER);
      
      if (pchMultiPart != NULL) {
        // We need the full HTTP header before processing (ends in '\r\n\r\n')
        char *pchHttpHeaderEnd = _mwStrDword(phsSocket->buffer, HTTP_HEADEREND, 0);
        
        if (pchHttpHeaderEnd != NULL) {
          char *pchBoundarySearch = NULL;
          int iHttpHeaderLength = (DWORD)pchHttpHeaderEnd + 2 - (DWORD)phsSocket->buffer;
          
          DEBUG("Http multipart POST received on socket %d\n",
                 phsSocket->socket);
          
          // Allocate multipart structure information for socket
          (HttpMultipart*)phsSocket->ptr = calloc(1,sizeof(HttpMultipart));
          //ASSERT((HttpMultipart*)phsSocket->ptr != NULL);
          
          // What is the 'boundary' value
          strcpy((HttpMultipart*)phsSocket->ptr->pchBoundaryValue,"--");
          pchBoundarySearch = _mwStrStrNoCase(phsSocket->buffer, 
                                                HTTP_MULTIPARTBOUNDARY);
          if (pchBoundarySearch != NULL) {
            sscanf(pchBoundarySearch+9,"%s",
                   (HttpMultipart*)phsSocket->ptr->pchBoundaryValue+2);
          } else {
            DEBUG("Error! Http multipart POST header recvd on socket %d does not contain a boundary value\n",
                   phsSocket->socket);
            _mwCloseSocket(phsSocket);
            return;
          }
          
          //ASSERT(phsSocket->buffer != NULL);
          
          // Shift window to start at first boundary indicator
          (HttpMultipart*)phsSocket->ptr->iWriteLocation = 
            phsSocket->iDataLength - iHttpHeaderLength;
          //ASSERT((HttpMultipart*)phsSocket->ptr->iWriteLocation >= 0);
          memmove(phsSocket->buffer, pchHttpHeaderEnd + 2, 
                  (HttpMultipart*)phsSocket->ptr->iWriteLocation);
          memset(phsSocket->buffer + (HttpMultipart*)phsSocket->ptr->iWriteLocation, 0,
                HTTPMAXRECVBUFFER - (HttpMultipart*)phsSocket->ptr->iWriteLocation);
        } 
        else {
          DEBUG("Http multipart POST on socket %d waiting for additional header info\n",
                       phsSocket->socket);
        }
        
        return;
      }
    }
    
    // it's a normal POST. find body of message
    {
      int iLineLength;
      
      do {
        iLineLength=strcspn(phsSocket->buffer+iHeaderLength,"\r\n");
        iHeaderLength+=(iLineLength+2); // move to next line
      } while (iLineLength>0 && iHeaderLength<=phsSocket->iDataLength);
    }
    
    // check if we have the whole message
    if (iHeaderLength+iContentLength <= phsSocket->iDataLength) {
      // process the variables
      _mwProcessPostVars(phsSocket,iHeaderLength,iContentLength);
    } else {
      // not enough content received yet
      DEBUG("Http POST on socket %d waiting for additional data (%d of %d recvd)\n",
                   phsSocket->socket,phsSocket->iDataLength-iHeaderLength,
                   iContentLength);
    }
  } else {
    #if 0
    // header does not contain content length
    SYSLOG(LOG_ERR,"Error! Http POST header recvd on socket %d does not contain content length\n",
           phsSocket->socket);
    #endif
   
  }
} // end of _mwProcessPost
#endif	//HTTPPOST
