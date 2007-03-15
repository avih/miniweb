#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"

int mpState=0;
static SHELL_PARAM mpx;

int mpRead(char* buf, int bufsize)
{
	int n;
	mpx.buffer = buf;
	mpx.iBufferSize = bufsize;
	n = ShellRead(&mpx);
	if (n < 0) return -1;
	buf[n] = 0;
	return n;
}

int mpCommand(char* cmd)
{
	int ret,bytes=0;
	char* p=malloc(strlen(cmd)+2);
	ret=sprintf(p,"%s\n",cmd);
	ret=write(mpx.fdStdinWrite,p,ret);
	free(p);
	return ret;
}

void mpClose()
{
	if (mpState) {
		mpCommand("quit");
		ShellWait(&mpx,1);
		ShellClean(&mpx);
	}
}

int mpOpen(char* pchFilename, char* opts)
{
	char buf[512];
	mpClose();
#ifdef WIN32
	sprintf(buf,"mplayer %s -slave -quiet %s",pchFilename, opts);
#else
	sprintf(buf,"/usr/bin/mplayer %s -slave -quiet %s",pchFilename, opts);
#endif
	mpx.flags = SF_REDIRECT_STDIN | SF_REDIRECT_STDOUT;
	if (ShellExec(&mpx, buf)) return -1;
	mpState=1;
	return 0;
}

int ehMpd(MW_EVENT event, void* arg)
{
	switch (event) {
	case MW_INIT:
		memset(&mpx,0,sizeof(mpx));
		break;
	case MW_UNINIT:
		mpClose();
		break;
	}
	return 0;
}

int uhMpd(UrlHandlerParam* param)
{
	char *cmd=param->pucRequest;
	if (!strncmp(cmd,"open",4)) {
		char *opts = NULL;
		char *filename = NULL;
		if (mwParseQueryString(param) > 0) {
			filename = mwGetVarValue(param->pxVars, "file");
			if (filename) mwDecodeString(filename);
			opts = mwGetVarValue(param->pxVars, "opts");
			if (opts) mwDecodeString(opts);
		}
		if (!mpOpen(filename, opts)) {
			char *p = NULL;
			int n;
			int offset = 0;
			while (offset < param->iDataBytes) {
				n = mpRead(param->pucBuffer + offset, param->iDataBytes - offset);
				if (n <= 0) break;
				offset += n;
				if (p = strstr(param->pucBuffer, "Starting playback...")) break;
			}
			mpCommand("get_time_length");
			while (offset < param->iDataBytes) {
				n = mpRead(param->pucBuffer + offset, param->iDataBytes - offset);
				if (n <= 0) break;
				offset += n;
				if (p = strstr(param->pucBuffer, "ANS_LENGTH=")) break;
			}
			if (!p) mpState = 0;
		}
	} else if (!strncmp(cmd,"command=",8)) {
		mpCommand(cmd+8);
		if (mpRead(param->pucBuffer, param->iDataBytes) <= 0)
			strcpy(param->pucBuffer, "Error");
	} else {
		return 0;
	}
	param->iDataBytes=strlen(param->pucBuffer);
	param->fileType=HTTPFILETYPE_TEXT;
	return FLAG_DATA_RAW;
}

#endif
