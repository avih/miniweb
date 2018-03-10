#ifndef _LOADPLUGIN_H_
#define _LOADPLUGIN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "httpapi.h"

void add_handler(UrlHandler** urlHandlerList, const char* prefix, PFNURLCALLBACK uhf, PFNEVENTHANDLER ehf);
void add_handler_from_dll(UrlHandler** urlHandlerList, const char* arg);

#ifdef __cplusplus
}
#endif

#endif
