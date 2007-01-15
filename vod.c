#include "httppil.h"
#include "httpapi.h"

#define MAX_SESSIONS 2
#define PRE_ALLOC_UNIT 16
#define LOOP_VIDEO "vodloop.mp4"

typedef struct _PL_ENTRY {
	struct _PL_ENTRY *next;
	void* data;
} PL_ENTRY;

typedef enum {
	ACT_NOTHING=0,
	ACT_SKIP,
} VOD_CLIENT_ACTIONS;

typedef int (*PL_ENUM_CALLBACK)(PL_ENTRY *entry, void *arg);
	
PL_ENTRY* plAddEntry(PL_ENTRY **hdr, void* data);
PL_ENTRY* plFindEntry(PL_ENTRY *hdr, void* data);
void* plGetEntry(PL_ENTRY **hdr);
void* plDelEntry(PL_ENTRY **hdr, void* data);
void* plAdvanceEntry(PL_ENTRY **hdr, void* data);
int plEnumEntries(PL_ENTRY *hdr, PL_ENUM_CALLBACK pfnEnumCallback, 
						   void *arg, int from, int count);

static PL_ENTRY* plhdr[MAX_SESSIONS];
static int listindex=0,listcount=0;
static PL_ENTRY played={NULL,NULL};
static PL_ENTRY *lastplayed=&played;
static int playcount=0;
static char vodbuf[256];
static int playingcode = 0;
static char *vodroot=NULL;
static char *htmlbuf=NULL;

static char** filelist = NULL;
static int filecount = 0;
static char** vodlist;
static int prefixlen = 0;

int EnumDir(char* pchDir)
{
	int i;
	char buf[256];
	char* path;
	int dirlen = strlen(pchDir) + 1;
	int pathlen = 0;
	char** dirlist = NULL;
	int dircount = 0;

	for (i = ReadDir(pchDir, buf); !i; i = ReadDir(NULL, buf)) {
		int len;
		if (buf[0] == '.' && (buf[1] == 0 || (buf[1] == '.' && buf[2] == 0))) continue;
		len = dirlen + strlen(buf) + 1;
		if (len > pathlen) {
			if (pathlen) free(path);
			path = malloc(len);
			pathlen = len;
		}
		sprintf(path, "%s/%s", pchDir, buf);
		if (IsDir(path)) {
			if (!(dircount % PRE_ALLOC_UNIT)) {
				dirlist = realloc(dirlist, (dircount + PRE_ALLOC_UNIT) * sizeof(char*));
			}
			dirlist[dircount++] = strdup(buf);
		} else {
			if (!(filecount % PRE_ALLOC_UNIT)) {
				filelist = realloc(filelist, (filecount + PRE_ALLOC_UNIT) * sizeof(char*));
			}
			filelist[filecount++] = strdup(path + prefixlen);
		}
	}
	for (i = 0; i < dircount; i++) {
		int len = dirlen + strlen(dirlist[i]) + 1;
		if (len > pathlen) {
			if (pathlen) free(path);
			path = malloc(len);
			pathlen = len;
		}
		sprintf(path, "%s/%s", pchDir, dirlist[i]);
		free(dirlist[i]);
		printf("%s\n", path);
		EnumDir(path);
	}
	free(dirlist);
	if (pathlen) free(path);
	return 0;
}

static int EnumSelections(PL_ENTRY *entry, void *arg)
{
	char **p=(char**)arg;
	*p+=sprintf(*p,"%05d ",entry->data);
	return 0;
}

static int EnumSelectionsHtml(PL_ENTRY *entry, void *arg)
{
	char **p=(char**)arg,*q;
	int len;
	char *filename;
	
	q = vodlist[(int)entry->data];
	q = strrchr(q, '/');
	if (!q) return -1;
	filename = strdup(++q);
	q = strrchr(filename, '.');
	if (!q) return 0;
	*q=0;
	len=sprintf(*p,"<p>%s <a href='/vod/del=%05d&inf=list'>[Del]</a> <a href='/vod/adv=%05d&inf=list'>[Top]</a></p>",filename,entry->data,entry->data);
	*p+=len;
	free(filename);
	return 0;
}

void vodInit(HttpParam* hp)
{
	int i;
	int code = 0;
	memset(plhdr, 0, sizeof(plhdr));
	vodroot=malloc(strlen(hp->pchWebPath)+7);
	sprintf(vodroot,"%s",hp->pchWebPath);
	vodlist = calloc(100000, sizeof(char*));
	prefixlen = strlen(vodroot) + 1;
 	EnumDir(vodroot);
	for (i = 0; i < filecount; i++) {
		char *p = strrchr(filelist[i], '/');
		if (p && *(p + 6) == ' ') {
			code = atoi(p + 1);
			if (code > 0) {
				vodlist[code] = filelist[i];
			}
		}
	}
	for (i = 0; i < 100000; i++) {
		if (vodlist[i]) {
			printf("%s\n", vodlist[i]);
		}
	}
	printf("Count: %d\n", filecount);
}

int ehVod(MW_EVENT event, void* arg)
{
	switch (event) {
	case MW_INIT:
		vodInit(arg);
		break;
	case MW_UNINIT:
		//un-initialization
		if (vodroot) free(vodroot);
		if (htmlbuf) free(htmlbuf);
		break;
	}
	return 0;
}

#define VOD_LOOP_COUNT 4

#ifdef WIN32
#define OPEN_FLAG O_RDONLY|0x8000
#else
#define OPEN_FLAG O_RDONLY
#endif

int GetSessionId(HttpVariables* pxVars)
{
	if (pxVars) {
		char *v = mwGetVarValue(pxVars,"session");
		if (v) return atoi(v);		
	}
	return 0;
}

int uhVodStream(UrlHandlerParam* param)
{
	static int code = 0;
	static int prevtime = 0;
	int session = GetSessionId(param->pxVars);

	DBG("Session: %d\n", session);
	if (param->hs->request.iStartByte==0 && time(NULL) - prevtime > 3) {
		prevtime = time(NULL);
		while ((code=(int)plGetEntry(&plhdr[session]))) {
			char keyword[16];
			sprintf(keyword,"%05d",code);
			if (vodlist[code]) break;
			DBG("[vod] %d not available\n",code);
		}
		if (vodlist[code]) {
			lastplayed->next=malloc(sizeof(PL_ENTRY));
			lastplayed=lastplayed->next;
			lastplayed->data=(void*)code;
			lastplayed->next=NULL;
			playcount++;
			playingcode = code;
		}
	}
	if (!playingcode) {
		sprintf(param->pucBuffer,LOOP_VIDEO);
	} else {
		sprintf(param->pucBuffer,"%s", vodlist[playingcode]);
	}
	DBG("[vod] stream: %s\n",param->pucBuffer);
	return FLAG_DATA_FILE;
}

int uhVod(UrlHandlerParam* param)
{
	char *req=param->pucRequest;
	char *pbuf=param->pucBuffer;
	static VOD_CLIENT_ACTIONS action=0;
	int session = GetSessionId(param->pxVars);

	DBG("Session: %d\n", session);
	param->fileType=HTTPFILETYPE_TEXT;
	for(;;) {
		switch (GETDWORD(req)) {
		case DEFDWORD('n','o','p',0):
			strcpy(pbuf,"state=OK");
			break;
		case DEFDWORD('c','m','d','='):
			action=ACT_SKIP;
			strcpy(pbuf,"Play next");
			param->iDataBytes=9;
			return FLAG_DATA_RAW;
		case DEFDWORD('i','n','f','='): {
			int itemcount=0;
			req+=4;
			switch (GETDWORD(req)) {
			case DEFDWORD('l','i','s','t'):
				itemcount=listcount;
				break;
			case DEFDWORD('h','i','s','t'):
				itemcount=playcount;
				break;
			}
			pbuf=param->pucBuffer;
			if (itemcount>16) {
				if (htmlbuf) free(htmlbuf);
				htmlbuf=malloc(itemcount*128);
				pbuf=htmlbuf;
				param->pucBuffer=htmlbuf;
			}
			//FIXME: css file path
			pbuf+=sprintf(pbuf,"<html><head><link href='/vodsys/default.css' rel='stylesheet' type='text/css'></head><body>");
			switch (GETDWORD(req)) {
			case DEFDWORD('l','i','s','t'): {
				int count;
				count=plEnumEntries(plhdr[session], EnumSelectionsHtml, (void*)&pbuf, 0, 0);
				pbuf+=sprintf(pbuf,"<p>Total: %d</p>",count);
				} break;
			case DEFDWORD('h','i','s','t'): {
				PL_ENTRY *pl;
				pbuf+=sprintf(pbuf,"<html><body>");
				for (pl=played.next; pl; pl=pl->next) {
					pbuf+=sprintf(pbuf,"<p>%05d</p>",pl->data);
				}
				pbuf+=sprintf(pbuf,"<p>Total: %d</p>",playcount);
				} break;
			}
			pbuf+=sprintf(pbuf,"</body></html>");
			param->iDataBytes=(int)(pbuf)-(int)(param->pucBuffer);
			param->fileType=HTTPFILETYPE_HTML;
			} return FLAG_DATA_RAW;
		case DEFDWORD('l','s','t','='):
			switch (GETDWORD(req+4)) {
			case DEFDWORD('n','e','x','t'):
				if (listindex<listcount-4) listindex+=4;
				break;
			case DEFDWORD('p','r','e','v'):
				listindex-=4;
				if (listindex<0) listindex=0;
				break;
			}
		case DEFDWORD('l','s','t',0):
			if (listindex>=listcount) listindex=0;
			if (listcount==0) {
				strcpy(pbuf,"osd=[empty]");
			} else {
				char *p;
				strcpy(pbuf,"osd=");
				p=pbuf+4;
				if (listindex>0) *(p++)='<';
				plEnumEntries(plhdr[session], EnumSelections, (void*)&p, listindex, 4);
				*p=0;
				if (listindex+4<listcount) strcat(pbuf,">");
				DBG("[vod] list item %d-%d\n",listindex,listindex+3);
			}
			strcat(pbuf,"\notm=900");
			break;
		case DEFDWORD('a','d','d','='): {
			void *data=NULL;
			int code;
			req+=4;
			code=atoi(req);
			if (plFindEntry(plhdr[session],(void*)code)) {
				strcpy(pbuf,"osd=ordered");
				break;
			}
			if (vodlist[code]) {
				data=plAddEntry(&plhdr[session],(void*)code);
			}
			if (data)
				sprintf(pbuf,"osd=[%s]\n",req);
			else
				strcpy(pbuf,"osd=error");
			} break;
		case DEFDWORD('d','e','l','='): {
			void *data=NULL;
			int code=atoi(req+4);
			if (code) data=plDelEntry(&plhdr[session],(void*)code);
			if (data)
				sprintf(pbuf,"osd=%05d deleted",code);
			else
				strcpy(pbuf,"osd=error");
			} break;
		case DEFDWORD('a','d','v','='): {
			void *data=NULL;
			int code=atoi(req+4);
			if (code) data=plAdvanceEntry(&plhdr[session],(void*)code);
			if (data)
				sprintf(pbuf,"osd=%05d topped",code);
			else
				strcpy(pbuf,"osd=error");
			} break;
		case DEFDWORD('c','n','t',0):
			sprintf(pbuf,"osd=%d",playcount);
			break;
		case DEFDWORD('p','l','a','y'): {
			int code = (int)plGetEntry(&plhdr[session]);
			if (code) {
				sprintf(pbuf, "%s", vodlist[code]);
			} else {
				strcpy(pbuf, LOOP_VIDEO);
			}
			} break;
		default:
			strcpy(pbuf,"Invalid request");
		}
		pbuf+=strlen(pbuf);
		if (!(req=strchr(req,'&'))) break;
		req++;
	}
	if (action) {
		pbuf+=sprintf(pbuf,"\nact=skip");
		action=ACT_NOTHING;
	}
	param->iDataBytes=(int)pbuf-(int)(param->pucBuffer);
	return FLAG_DATA_RAW;
}

//////////////////////////////////////////////////////////////////
// Playlist implementation
//////////////////////////////////////////////////////////////////

PL_ENTRY* plAddEntry(PL_ENTRY **hdr, void* data)
{
	PL_ENTRY *ptr=*hdr;
	if (!ptr) {
		// allocate header
		*hdr=(PL_ENTRY*)malloc(sizeof(PL_ENTRY));
		ptr=*hdr;
	} else {
		// travel to the end of list
		while (ptr->next) ptr=ptr->next;
		ptr->next=(PL_ENTRY*)malloc(sizeof(PL_ENTRY));
	 	ptr=ptr->next;
	}
	ptr->data=data;
	ptr->next=NULL;
	listcount++;
	return ptr;
}

void* plGetEntry(PL_ENTRY **hdr)
{
	PL_ENTRY *ptr=*hdr;
	void* data;
	if (!ptr) return NULL;
	data=ptr->data;
	*hdr=ptr->next;
	free(ptr);
	listcount--;
	return data;
}

PL_ENTRY* plFindEntry(PL_ENTRY *hdr, void* data)
{
	PL_ENTRY *ptr;
	for (ptr=hdr; ptr; ptr=ptr->next) {
		if (ptr->data==data) return ptr;
	}
	return NULL;
}

void* plDelEntry(PL_ENTRY **hdr, void* data)
{
	PL_ENTRY *ptr=*hdr,*prev=NULL;
	void* deleted;
	if (!ptr) return NULL;
	if (ptr->data==data) {
		deleted=ptr->data;
		*hdr=ptr->next;
		free(ptr);
		listcount--;
		return deleted;
	}
	for (prev=ptr, ptr=ptr->next; ptr; prev=ptr, ptr=ptr->next) {
		if (ptr->data==data) { 
			deleted=ptr->data;
			prev->next=ptr->next;
			free(ptr);
			listcount--;
			return deleted;
		}
	}
	return NULL;
}

void* plAdvanceEntry(PL_ENTRY **hdr, void* data)
{
	PL_ENTRY *ptr=*hdr,*prev=NULL;
	while (ptr) {
		if (ptr->data==data) {
			if (!prev) return 0;
			prev->next=ptr->next;
			ptr->next=*hdr;
			*hdr=ptr;
			return ptr;
		}
		prev=ptr;
		ptr=ptr->next;
	}
	return NULL;
}

int plEnumEntries(PL_ENTRY *hdr, PL_ENUM_CALLBACK pfnEnumCallback, 
						   void *arg, int from, int count)
{
	PL_ENTRY *ptr=hdr;
	int hits;
	if (!pfnEnumCallback) return -1;
	for (hits=0;ptr;ptr=ptr->next,hits++) {
		if (count && hits>=from+count) return hits;
		if (hits>=from)
			if ((*pfnEnumCallback)(ptr, arg)) break;
	}
	return hits;
}
