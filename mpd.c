#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"
#include "httpxml.h"
#include "httpvod.h"

typedef enum {
	MP_IDLE = 0,
	MP_LOADING,
	MP_PLAYING,
	MP_PAUSED,
} MP_STATES;

static char* states[] = {"idle", "loading", "playing", "paused"};

MP_STATES mpState = MP_IDLE;
int mpPos = 0;
static SHELL_PARAM mpx;
pthread_t mpThreadHandle = 0;
pthread_mutex_t mpConsoleMutex;

static PL_ENTRY* playlist = 0;

int mpRead(char* buf, int bufsize)
{
	int n;
	if (mpState == MP_IDLE || !mpx.fdStdoutRead) return 0;
	mpx.buffer = buf;
	mpx.iBufferSize = bufsize;
	n = ShellRead(&mpx);
	if (n < 0) return -1;
	buf[n] = 0;
	return n;
}

int mpCommand(char* cmd)
{
	int ret=write(mpx.fdStdinWrite,cmd,strlen(cmd));
	write(mpx.fdStdinWrite,"\n",1);
	return ret;
}

void mpClose()
{
	mpPos = -1;
	if (mpState != MP_IDLE) {
		if (mpCommand("quit") > 0) ShellWait(&mpx,1000);
		ShellClean(&mpx);
	}
}

int mpOpen(char* pchFilename, char* opts)
{
	char buf[512];
	mpClose();
	snprintf(buf, sizeof(buf), "mplayer \"%s\" -slave -quiet %s",pchFilename, opts);
	printf("MPlayer command line:\n%s\n", buf);
	mpx.flags = SF_REDIRECT_STDIN | SF_REDIRECT_STDOUT;
	mpState = MP_LOADING;
	if (ShellExec(&mpx, buf)) return -1;
	return 0;
}

int ehMpd(MW_EVENT event, int argi, void* argp)
{
	switch (event) {
	case MW_INIT:
		memset(&mpx,0,sizeof(mpx));
		MutexCreate(&mpConsoleMutex);
		break;
	case MW_UNINIT:
		MutexDestroy(&mpConsoleMutex);
		mpClose();
		break;
	}
	return 0;
}

void* mpThread(void* _args)
{
	char *p = NULL;
	char buf[1024];
	char* args = strdup(_args);
	int n;
	int offset;
	void* data;
	while (data = plGetEntry(&playlist)) {
		if (!data) continue;
		n = mpOpen(data, args);
		free(data);
		if (n) break;

		mpState = MP_PLAYING;
		MutexLock(&mpConsoleMutex);
		while (mpCommand("get_time_pos") <= 0) msleep(500);
		do {
			offset = 0;
			while (offset < sizeof(buf) - 1) {
				n = mpRead(buf + offset, sizeof(buf) - 1 - offset);
				if (n <= 0) break;
				offset += n;
				buf[offset] = 0;
				if (p = strstr(buf, "ANS_TIME_POSITION=")) {
					mpPos = atoi(p + 18);
					break;
				}
			}
			// stop here when paused
			MutexUnlock(&mpConsoleMutex);
			do {
				msleep(500);
			} while (mpState == MP_PAUSED);
			MutexLock(&mpConsoleMutex);
		} while (mpCommand("get_time_pos") > 0);
		MutexUnlock(&mpConsoleMutex);
		ShellTerminate(&mpx);
		ShellClean(&mpx);
	}
	free(args);
	mpState = MP_IDLE;
	mpThreadHandle = 0;
	return 0;
}

int uhMpd(UrlHandlerParam* param)
{
	char *action;
	char *pbuf = param->pucBuffer;
	int bufsize = param->iDataBytes;
	HTTP_XML_NODE node;

	mwParseQueryString(param);
	action = mwGetVarValue(param->pxVars, "action", 0);

	mwWriteXmlHeader(&pbuf, &bufsize, 10, "gb2312", mwGetVarValue(param->pxVars, "xsl", 0));
	mwWriteXmlString(&pbuf, &bufsize, 0, "<response>");

	node.indent = 1;
	node.name = "state";
	node.fmt = "%s";
	node.flags = 0;

	if (!strcmp(param->pucRequest + 1, "playlist")) {
		PL_ENTRY *ptr;
		int i;
		if (!action) {
		} else if (!strcmp(action, "add")) {
			char *filename = mwGetVarValue(param->pxVars, "stream", 0);
			if (!filename || plAddEntry(&playlist, strdup(filename), strlen(filename) + 1)) {
				mwDecodeString(filename);
				node.value = "OK";
			} else {
				node.value = "error";
			}
			mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		}
		ptr = playlist;
		mwWriteXmlString(&pbuf, &bufsize, 1, "<playlist>");
		for (i=0; ptr; ptr = ptr->next, i++) {
			node.indent = 2;
			node.name = "item";
			node.fmt = "%s";
			node.value = ptr->data;
			mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		}
		mwWriteXmlString(&pbuf, &bufsize, 1, "</playlist>");
	} else if (!action) {

	} else if (!strcmp(action, "play")) {
		char *filename = mwGetVarValue(param->pxVars, "stream", 0);
		char *args = mwGetVarValue(param->pxVars, "arg", 0);
		if (filename) mwDecodeString(filename);
		if ((!filename || !*filename) || plAddEntry(&playlist, strdup(filename), strlen(filename) + 1)) {
			if (!mpThreadHandle) {
				if (args) mwDecodeString(args);
				ThreadCreate(&mpThreadHandle, mpThread, args);
			} else {
				mpClose();
			}
			node.value = "OK";
		} else {
			node.value = "error";
		}
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
	} else if (!strcmp(action, "query")) {
		int i;
		char* info;
		for (i = 0; info = mwGetVarValue(param->pxVars, "info", i); i++) {
			if (!strcmp(info, "pos")) {
				node.name = "pos";
				node.fmt = "%d";
				node.value = (void*)mpPos;
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
			} else if (!strcmp(info, "state")) {
				node.name = "state";
				node.fmt = "%s";
				node.value = states[mpState];
				mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
			}
		}
	} else if (!strcmp(action, "pause")) {
		mpState = (mpState == MP_PLAYING ? MP_PAUSED : MP_PLAYING);
		mpCommand("pause");
	} else if (!strcmp(action, "seek")) {
		char buf[32];
		char *args = mwGetVarValue(param->pxVars, "arg", 0);
		if (args) {
			snprintf(buf, sizeof(buf), "seek %s", args);
			mpCommand(buf);
		}
	} else if (!strcmp(action, "command")) {
		char *cmd = mwGetVarValue(param->pxVars, "arg", 0);
		char *hasResult = mwGetVarValue(param->pxVars, "result", 0);
		if (cmd) {
			if (mpCommand(cmd) > 0) {
				node.value = "OK";
				if (hasResult) {
					int bytes;
					MutexLock(&mpConsoleMutex);

					bytes = snprintf(pbuf, bufsize,  "  <result><![CDATA[");
					pbuf += bytes;
					bufsize -= bytes;

					bytes = mpRead(pbuf, bufsize);
					MutexUnlock(&mpConsoleMutex);
					if (bytes > 0) {
						pbuf += bytes;
						bufsize -= bytes;
					}

					bytes = snprintf(pbuf, bufsize,  "]]></result>");
					pbuf += bytes;
					bufsize -= bytes;
				}
			} else {
				node.value = "error";
			}
			mwWriteXmlLine(&pbuf, &bufsize, &node, 0);

		}
	} else {
		return 0;
	}

	mwWriteXmlString(&pbuf, &bufsize, 0, "</response>");

	param->iDataBytes=(int)(pbuf-param->pucBuffer);
	param->fileType=HTTPFILETYPE_XML;
	return FLAG_DATA_RAW;
}

#endif
