#include <stdio.h>
#include <string.h>
#include "httppil.h"
#include "httpapi.h"
#include "processpil.h"
#include "httpclient.h"
#include "revision.h"

extern HttpParam *httpParam;

int uhMediaItems(UrlHandlerParam* param)
{
	if (!param->hs->ptr) {
		// first request
		char path[256];
		int startByte = param->hs->request.startByte;
		sprintf(path, "%s", param->pucRequest);
		param->hs->ptr = (void*)open(path, O_BINARY | O_RDONLY);
		if ((int)param->hs->ptr < 0) return 0;
		if (startByte > 0)
			lseek((int)param->hs->ptr, startByte, SEEK_SET);
	}
	if (param->pucBuffer) {
		param->dataBytes = read((int)param->hs->ptr, param->pucBuffer, param->dataBytes);
	} else {
		// connection closed
		param->dataBytes = 0;
	}
	if (param->dataBytes <= 0) {
		close((int)param->hs->ptr);
		param->hs->ptr = 0;
	}
	param->fileType = HTTPFILETYPE_OCTET;
	return param->dataBytes > 0 ? (FLAG_DATA_STREAM) : 0;
}

#define TRANSCODE_CMD "codecs/mencoder.exe \"%s\" -o - -really-quiet -ofps 25 -ovc lavc -oac lavc -vf scale=352:288,harddup -of mpeg -mpegopts format=mpeg2 -lavcopts vcodec=mpeg2video:vbitrate=1024:vrc_maxrate=4000:vrc_buf_size=917:acodec=mp2:abitrate=224"

static char* ip = 0;

char* QueryObjectPath(char* id)
{
	HTTP_REQUEST req;
	char buf[128];
	char *result = 0;
	char *p;
	sprintf(buf, "http://%s/presentation/query/object?id=%s", ip, id);
	p = strrchr(buf + 71, '.');
	if (p) *p = 0;
	httpInitReq(&req, buf, 0); 
	if (httpRequest(&req)) return 0;
	if (req.buffer)
		result = strdup(req.buffer);
	httpClean(&req);
	return result;
}

int ehMediaItemsEvent(MW_EVENT msg, int argi, void* argp)
{
	HTTP_REQUEST req;
	char buf[128];

	switch (msg) {
	case MW_INIT:
		sprintf(buf, "http://%s/presentation/query/set?url=%d", ip, httpParam->httpPort);
		httpInitReq(&req, buf, 0); 
		httpRequest(&req);
		httpClean(&req);
		break;
	case MW_PARSE_ARGS: {
		char** argv = (char**)argp;
		int i;
		for (i=1; i<argi; i++) {
			if (!strcmp(argv[i], "--upnpserver")) {
				ip = argv[i + 1];
				break;
			}
		}
		} break;
	}
	return 0;
}

int uhMediaItemsTranscode(UrlHandlerParam* param)
{
	SHELL_PARAM* proc;

	if (!param->hs->ptr) {
		// first request
		char path[512];
		int startByte = param->hs->request.startByte;
		char *filename = QueryObjectPath(param->pucRequest);
		if (filename) {
			sprintf(path, TRANSCODE_CMD, filename);
			free(filename);
		} else {
			sprintf(path, TRANSCODE_CMD, param->pucRequest);
		}
		proc = (SHELL_PARAM*)calloc(1, sizeof(SHELL_PARAM));
		proc->flags = SF_REDIRECT_STDOUT;
		if (ShellExec(proc, path, 0))
			return 0;
		//param->hs->ptr = (void*)open(path, O_BINARY | O_RDONLY);
		if ((int)param->hs->ptr < 0) return 0;
		if (startByte > 1) {
			int n = 0;
			int bytesToSkip = startByte - 1;
			proc->buffer = param->pucBuffer;
			proc->iBufferSize = param->dataBytes;
			do {
				int bytes;
				proc->iBufferSize = min(proc->iBufferSize, bytesToSkip - n);
				bytes = ShellRead(proc, 1000);
				if (bytes <= 0) break;
				n += bytes;
			} while (n < proc->iBufferSize);
		}
		param->hs->ptr = proc;
	} else {
		proc = (SHELL_PARAM*)param->hs->ptr;
	}
	if (param->hs->ptr && param->pucBuffer) {
		proc->buffer = param->pucBuffer;
		proc->iBufferSize = param->dataBytes;
		param->dataBytes = ShellRead(proc, 1000);
	} else {
		// connection closed
		param->dataBytes = 0;
	}
	if (param->dataBytes <= 0) {
		ShellTerminate(proc);
		ShellClean(proc);
		free(param->hs->ptr);
		param->hs->ptr = 0;
	}
	param->fileType = HTTPFILETYPE_OCTET;
	return param->dataBytes > 0 ? (FLAG_DATA_STREAM) : 0;
}
