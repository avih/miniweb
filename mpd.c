#ifdef _MPD
#include "httppil.h"
#include "httpapi.h"
#include "procpil.h"
#include "httpxml.h"

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
	if (!strcmp(cmd, "quit")) {
		ShellClean(&mpx);
	}
	return ret;
}

void mpClose()
{
	mpPos = -1;
	if (mpState != MP_IDLE) {
		if (mpCommand("quit") > 0) ShellWait(&mpx,1000);
		ShellClean(&mpx);
		mpState = MP_IDLE;
		ThreadKill(mpThreadHandle);
		mpThreadHandle = 0;
	}
}

int mpOpen(char* pchFilename, char* opts)
{
	char buf[512];
	mpClose();
#ifdef WIN32
	sprintf(buf,"mplayer \"%s\" -slave -quiet %s",pchFilename, opts);
#else
	sprintf(buf,"/usr/bin/mplayer %s -slave -quiet %s",pchFilename, opts);
#endif
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
		break;
	case MW_UNINIT:
		mpClose();
		break;
	}
	return 0;
}

void* mpThread(void* arg)
{
	char *p = NULL;
	char buf[1024];
	int n;
	int offset = 0;
	while (offset < sizeof(buf)) {
		n = mpRead(buf + offset, sizeof(buf) - offset);
		if (n <= 0) break;
		offset += n;
		buf[offset] = 0;
		if (p = strstr(buf, "Starting playback...")) break;
	}
	mpState = MP_PLAYING;
	while (mpCommand("get_time_pos") <= 0) msleep(500);
	do {
		offset = 0;
		while (offset < sizeof(buf)) {
			n = mpRead(buf + offset, sizeof(buf) - offset);
			if (n <= 0) break;
			offset += n;
			buf[offset] = 0;
			if (p = strstr(buf, "ANS_TIME_POSITION=")) {
				mpPos = atoi(p + 18);
				break;
			}
		}
		do {
			msleep(500);
		} while (mpState == MP_PAUSED);
	} while (mpCommand("get_time_pos") > 0);
	mpClose();
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

	mwWriteXmlHeader(&pbuf, &bufsize, 10, "utf-8", mwGetVarValue(param->pxVars, "xsl", 0));
	mwWriteXmlString(&pbuf, &bufsize, 0, "<response>");

	node.indent = 1;
	node.name = "state";
	node.fmt = "%s";
	node.flags = 0;

	if (!action) {
	} else 	if (!strcmp(action, "open")) {
		char *filename = mwGetVarValue(param->pxVars, "stream", 0);
		char *args = mwGetVarValue(param->pxVars, "arg", 0);
		int bytes;
		if (filename) mwDecodeString(filename);
		if (args) mwDecodeString(args);

		mpClose();

		node.value = mpOpen(filename, args) ? "error" : "OK";
		bytes = _snprintf(pbuf, bufsize,  "  <console><![CDATA[");
		pbuf += bytes;
		bufsize -= bytes;

		for (bytes = 0;;) {
			int n = mpRead(pbuf + bytes, bufsize - bytes);
			if (n > 0) {
				bytes += n;
				pbuf[bytes] = 0;
				if (strstr(pbuf, "\nPlaying ")) break;
			} else {
				
			}
		}
		pbuf += bytes;
		bufsize -= bytes;

		ThreadCreate(&mpThreadHandle, mpThread, 0);

		bytes = _snprintf(pbuf, bufsize,  "]]>\n  </console>");
		pbuf += bytes;
		bufsize -= bytes;

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
			_snprintf(buf, sizeof(buf), "seek %s", args);
			mpCommand(buf);
		}
	} else if (!strcmp(action, "command")) {
		char *cmd = mwGetVarValue(param->pxVars, "arg", 0);
		if (cmd) {
			int bytes;
			bytes = _snprintf(pbuf, bufsize,  "  <console><![CDATA[");
			pbuf += bytes;
			bufsize -= bytes;
			
			if (mpCommand(cmd) > 0 && (bytes = mpRead(pbuf, bufsize)) > 0) {
				node.value = "OK";
			} else {
				node.value = "error";
			}
			pbuf += bytes;
			bufsize -= bytes;

			bytes = _snprintf(pbuf, bufsize,  "]]>\n  </console>");
			pbuf += bytes;
			bufsize -= bytes;

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
