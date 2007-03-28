#include "httppil.h"
#include "httpapi.h"
#include "httpxml.h"
#include "crc32.h"

#define MAX_SESSIONS 2
#define PRE_ALLOC_UNIT 16
#define LOOP_VIDEO "vodloop.mp4"

typedef struct _PL_ENTRY {
	struct _PL_ENTRY *next;
	void* data;
	int datalen;
} PL_ENTRY;

typedef enum {
	ACT_NOTHING=0,
	ACT_SKIP,
} VOD_CLIENT_ACTIONS;

typedef int (*PL_ENUM_CALLBACK)(PL_ENTRY *entry, void *arg);
	
PL_ENTRY* plAddEntry(PL_ENTRY **hdr, void* data, int datalen);
PL_ENTRY* plFindEntry(PL_ENTRY *hdr, void* data, int datalen);
PL_ENTRY* plGetEntry(PL_ENTRY **hdr);
void* plDelEntry(PL_ENTRY **hdr, void* data);
PL_ENTRY* plPinEntry(PL_ENTRY **hdr, void* data);
int plEnumEntries(PL_ENTRY *hdr, PL_ENUM_CALLBACK pfnEnumCallback, 
						   void *arg, int from, int count);

static PL_ENTRY* plhdr[MAX_SESSIONS];
static int listindex=0,listcount=0;
static int playcount=0;
static char vodbuf[256];
static unsigned long* hashmap;

#define MAX_CHARS 30
static int charsinfo[MAX_CHARS + 1];
static char *vodroot=NULL;

static char *htmlbuf=NULL;

typedef struct _CLIP_INFO {
	char* filename;
	char* title;
	int chars;
	int flags;
	int hash;
	struct _CLIP_INFO* next;
} CLIP_INFO;

typedef struct _CATEGORY_INFO {
	char* name;
	int count;
	int flags;
	int hash;
	CLIP_INFO* clips;
	struct _CATEGORY_INFO* next;
} CATEGORY_INFO;

static char** filelist = NULL;
static int filecount = 0;
static CATEGORY_INFO cats;
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
			//printf("%s\n", path);
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
		EnumDir(path);
	}
	free(dirlist);
	if (pathlen) free(path);
	return 0;
}

int GetCategoryHash(CATEGORY_INFO* ci)
{
	char *p = ci->name;
	int i;
	unsigned sum = 0;
	for (i = 0; p[i]; i++) sum += p[i];
	return sum % 100;
}

int GetTitleHash(CLIP_INFO* ci)
{
	unsigned long val;
	crc32Init(&val);
	crc32Update(&val, ci->title, strlen(ci->title));
	crc32Finish(&val);
	return val % 10000;
}

char* FilterDup(char* str)
{
	char *newstr = (char*)malloc(strlen(str) + 1);
	char *p, *q;
	for (p = str, q = newstr; *p; p++, q++) {
		switch (*p) {
		case '_':
			*q = ' ';
			break;
		case '&':
			*q = ',';
			break;
		default:
			*q = *p;
		}
	}
	*q = 0;
	return newstr;
}

CATEGORY_INFO* FindCategory(char* name)
{
	int pos = 0;
	CATEGORY_INFO* ptr;
	CATEGORY_INFO* prev = 0;
	if (!name || !*name) {
		cats.count++;
		return &cats;
	}
	ptr = &cats;
	for (;;) {
		int n = strcmp(ptr->name, name);
		if (n == 0) {
			ptr->count++;
			return ptr;
		} else if (n > 0 && prev) {
			prev->next = (CATEGORY_INFO*)calloc(1, sizeof(CATEGORY_INFO));
			prev->next->next = ptr;
			ptr = prev->next;
			break;
		} else if (!ptr->next) {
			ptr->next = (CATEGORY_INFO*)calloc(1, sizeof(CATEGORY_INFO));
			ptr = ptr->next;
			break;
		}
		prev = ptr;
		ptr = ptr->next;		
	}

	ptr->name = FilterDup(name);
	ptr->count = 1;
	ptr->hash = GetCategoryHash(ptr);
	return ptr;
}

int IsHashed(int hash, int autoset)
{
	int dwoff = hash >> 5;
	int bitmask = 1 << (hash & 0x1f);
	if (hashmap[dwoff] & bitmask) return 1;
	if (autoset) hashmap[dwoff] |= bitmask;
	return 0;
}

CLIP_INFO* GetClipByHash(int hash, CATEGORY_INFO** pcat)
{
	int cathash = hash / 10000;
	CATEGORY_INFO* cat;
	for (cat = &cats; cat; cat = cat->next) {
		if (cathash == cat->hash) {
			CLIP_INFO* ptr;
			for (ptr = cat->clips; ptr; ptr = ptr->next) {
				if (ptr->hash == hash) {
					if (pcat) *pcat = cat;
					return ptr;
				}
			}
		}
	}
	return 0;
}

CLIP_INFO* GetClipByName(char* catname, char* title, CATEGORY_INFO** pcat)
{
	CATEGORY_INFO* cat;
	for (cat = &cats; cat; cat = cat->next) {
		if (!catname || !strcmp(catname, cat->name)) {
			CLIP_INFO* ptr;
			for (ptr = cat->clips; ptr; ptr = ptr->next) {
				if (!title || !strcmp(title, ptr->title)) {
					if (pcat) *pcat = cat;
					return ptr;
				}
			}
		}
	}
	return 0;
}

CLIP_INFO* GetClipByFile(char* filename, CATEGORY_INFO** pcat)
{
	CATEGORY_INFO* cat;
	for (cat = &cats; cat; cat = cat->next) {
		CLIP_INFO* ptr;
		for (ptr = cat->clips; ptr; ptr = ptr->next) {
			if (!strcmp(filename, ptr->filename)) {
				if (pcat) *pcat = cat;
				return ptr;
			}
		}
	}
	return 0;
}

int AddClip(char* filename)
{
	CLIP_INFO* pinfo;
	CATEGORY_INFO* cat;
	char buf[256];
	char *p;
	char *s;
	int i;
	s = strrchr(filename, '/');
	if (!(s++)) s = filename;
	if (s[5] == ' ' && atoi(s) > 0) s += 5;
	while (*s == ' ') s++;
	if (*s == '.') return -1;
	pinfo = (CLIP_INFO*)malloc(sizeof(CLIP_INFO));
	pinfo->next = 0;
	pinfo->filename = filename;
	p = strrchr(s, '[');
	if (p) {
		// get category
		strcpy(buf, p + 1);
		p = strchr(buf, ']');
		if (p) *p = 0;
		cat = FindCategory(buf);

		// get title
		strcpy(buf, s);
		p = strchr(buf, '[');
		while (--p >= buf && *p == ' ');
		*(p + 1) = 0;
		pinfo->title = FilterDup(s);
	} else {
		strcpy(buf, s);
		s = buf;
		if (p = strstr(s, " - ")) {
			*p = 0;
			cat = FindCategory(s);
			s = p + 3;
			p = strrchr(s, '.');
			if (p) *p = 0;
			pinfo->title = FilterDup(s);
		} else if ((p = strchr(s, ' ')) || (p = strchr(s, '-'))) {
			*p = 0;
			cat = FindCategory(s);
			for (s = p + 1; *s == ' ' || *s == '-'; s++);
			p = strrchr(s, '.');
			if (p) *p = 0;
			pinfo->title = FilterDup(s);
		} else {
			cat = FindCategory(0);
			p = strrchr(s, '.');
			if (p) *p = 0;
			pinfo->title = FilterDup(s);
		}
	}
	pinfo->hash = cat->hash * 10000 + GetTitleHash(pinfo);
	for (i = 0, p = pinfo->title; *p && *p != '(' && *p != '['; p++) {
		if (*p < 0) {
			p++;
			i++;
		} else if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <='Z') || (*p >= '0' && *p <='9')) {
			i++;
		}
	}
	pinfo->chars = i;
	charsinfo[(i > MAX_CHARS || i < 1) ? 0 : i]++;

	while (IsHashed(pinfo->hash, 1)) {
		CATEGORY_INFO* excat;
		CLIP_INFO* exclip = GetClipByHash(pinfo->hash, &excat);
		if (excat == cat && !strcmp(exclip->title, pinfo->title)) {	
			printf("DUPLICATED @ %06d Cat: %s\nExist: %s\nNew: %s\n", pinfo->hash, cat->name, exclip->filename, pinfo->filename);	
		} else {
			//printf("HASH FAULT @ %06d Exist: %s - %s New: %s - %s\n", pinfo->hash, cat->name, exclip->title, cat->name, pinfo->title);	
		}
		pinfo->hash++;
	}

	// add clip to category
	if (!cat->clips) {
		cat->clips = pinfo;
	} else {
		CLIP_INFO *ptr = cat->clips;
		CLIP_INFO *prev = 0;
		for (;;) {
			int n = strcmp(ptr->title, pinfo->title);
			if (n > 0) {
				if (!prev) {
					pinfo->next = cat->clips;
					cat->clips = pinfo;
				} else {
					pinfo->next = prev->next;
					prev->next = pinfo;
				}
				break;
			}
			if (!ptr->next) {
				ptr->next = pinfo;
				break;
			}
			prev = ptr;
			ptr = ptr->next;
		}
	}
	return 0;
}

void vodInit()
{
	int i;
	int code = 0;
	int count = 0;
	if (!vodroot)
		return;
	memset(plhdr, 0, sizeof(plhdr));
	memset(&charsinfo, 0, sizeof(charsinfo));
	hashmap = calloc(1000000 / 32, sizeof(long));
	//sprintf(vodroot,"%s",hp->pchWebPath);
	cats.name = "";
	cats.hash = GetCategoryHash(&cats);
	cats.next = 0;
	prefixlen = strlen(vodroot) + 1;
 	EnumDir(vodroot);
	for (i = 0; i < filecount; i++) {
		if (!AddClip(filelist[i])) count++;
	}

	printf("\n\nCount: %d\n", count);
}

int ehVod(MW_EVENT event, int argi, void* argp)
{
	switch (event) {
	case MW_INIT:
		vodInit();
		break;
	case MW_UNINIT:
		//un-initialization
		free(hashmap);
		break;
	case MW_PARSE_ARGS: {
		int i = 0;
		char** argv = (char**)argp;
		for (i = 0; i < argi; i++) {
			if (argv[i][0] == '-' && !strcmp(argv[i] + 1, "vodroot")) {
				vodroot = argv[i + 1];
				break;
			}
		}
		}
	}
	return 0;
}

#define VOD_LOOP_COUNT 4

#ifdef WIN32
#define OPEN_FLAG O_RDONLY|0x8000
#else
#define OPEN_FLAG O_RDONLY
#endif

int uhVodStream(UrlHandlerParam* param)
{
	static int code = 0;
	static int prevtime = 0;
	int session;
	int id;
	char* file;
	mwParseQueryString(param);
	session = mwGetVarValueInt(param->pxVars, "session", 0);
	file = mwGetVarValue(param->pxVars, "file");
	id = mwGetVarValueInt(param->pxVars, "id" , -1);
	if (!file) {
		CLIP_INFO* clip = GetClipByHash(id, 0);
		if (clip) {
			snprintf(param->pucBuffer, param->iDataBytes, "~%s/%s", vodroot, clip->filename);
			return FLAG_DATA_FILE;
		}
	} else {
		snprintf(param->pucBuffer, param->iDataBytes, "~%s/%s", vodroot, file);
	}
	return 0;
	/*
	DBG("Session: %d\n", session);
	if (param->hs->request.iStartByte==0 && time(NULL) - prevtime > 3) {
		prevtime = (int)time(NULL);
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
	*/
}

void OutputItemInfo(char** pbuf, int* pbufsize, char* id)
{
	char buf[256];
	CATEGORY_INFO *cat;
	CLIP_INFO* clip;
	int hash = atoi(id);
	if (hash > 0)
		clip = GetClipByHash(hash, &cat);
	else
		clip = GetClipByName(0, id, &cat);
	if (!clip) return;
	snprintf(buf, sizeof(buf), "<item id=\"%d\">", id);
	mwWriteXmlString(pbuf, pbufsize, 2, buf);
	snprintf(buf, sizeof(buf), "<file>%s</file>", clip->filename);
	mwWriteXmlString(pbuf, pbufsize, 3, buf);
	snprintf(buf, sizeof(buf), "<title>%s</title>", clip->title);
	mwWriteXmlString(pbuf, pbufsize, 3, buf);
	snprintf(buf, sizeof(buf), "<category>%s</category>", cat->name);
	mwWriteXmlString(pbuf, pbufsize, 3, buf);
	mwWriteXmlString(pbuf, pbufsize, 2, "</item>");
}

int uhLib(UrlHandlerParam* param)
{
	char *pbuf;
	int bufsize = 256000;
	char buf[256];
	char *id;
	pbuf = (char*)malloc(256000);
	param->pucBuffer = pbuf;

	mwParseQueryString(param);

	mwWriteXmlHeader(&pbuf, &bufsize, 10, "gb2312", mwGetVarValue(param->pxVars, "xsl"));
	mwWriteXmlString(&pbuf, &bufsize, 0, "<response>");
	id = mwGetVarValue(param->pxVars, "id");

	if (!strcmp(param->pucRequest, "/category")) {
		int catid = id ? atoi(id) : -1;
		int hash = mwGetVarValueInt(param->pxVars, "hash", -1);
		char* name = mwGetVarValue(param->pxVars, "name");
		CATEGORY_INFO* cat;
		int i = 0;
		for (cat = &cats; cat; cat = cat->next, i++) {
			if ((hash >= 0 && hash != cat->hash) || (name && strcmp(name, cat->name)) || (catid >= 0 && catid != i))
				continue;

			_snprintf(buf, sizeof(buf), "<category id=\"%d\" hash=\"%02d\">", i, cat->hash);
			mwWriteXmlString(&pbuf, &bufsize, 1, buf);

			_snprintf(buf, sizeof(buf), "<name>%s</name>", cat->name);
			mwWriteXmlString(&pbuf, &bufsize, 1, buf);

			_snprintf(buf, sizeof(buf), "<clips>%d</clips>", cat->count);
			mwWriteXmlString(&pbuf, &bufsize, 1, buf);

			mwWriteXmlString(&pbuf, &bufsize, 1, "</category>");
		}
	} else if (!strcmp(param->pucRequest, "/title")) {
		int hash = id ? atoi(id) : -1;
		int chars = mwGetVarValueInt(param->pxVars, "chars", 0);
		char* catname = mwGetVarValue(param->pxVars, "category");
		int catid = mwGetVarValueInt(param->pxVars, "catid", -1);
		int i = 0;
		BOOL matched = 0;
		CLIP_INFO* info;
		CATEGORY_INFO* cat;
		for (cat = &cats; cat; cat = cat->next, i++) {
			if ((catid >= 0 && catid != i) || (catname && strcmp(catname, cat->name))) continue;
			for (info = cat->clips; info; info = info->next) {
				if ((hash >= 0 && hash != info->hash) || (chars && info->chars != chars)) continue;
				if (!matched) {
					_snprintf(buf, sizeof(buf), "<category name=\"%s\">", cat->name);
					mwWriteXmlString(&pbuf, &bufsize, 1, buf);
					matched = 1;
				}
				_snprintf(buf, sizeof(buf), "<item id=\"%02d\">", info->hash);
				mwWriteXmlString(&pbuf, &bufsize, 2, buf);

				_snprintf(buf, sizeof(buf), "<name>%s</name>", info->title);
				mwWriteXmlString(&pbuf, &bufsize, 2, buf);

				_snprintf(buf, sizeof(buf), "<chars>%d</chars>", info->chars);
				mwWriteXmlString(&pbuf, &bufsize, 2, buf);

				mwWriteXmlString(&pbuf, &bufsize, 2, "</item>");
			}
			if (matched) mwWriteXmlString(&pbuf, &bufsize, 1, "</category>");
			matched = FALSE;
		}
	} else if (!strcmp(param->pucRequest, "/chars")) {
		int i;
		for (i = 1; i <= MAX_CHARS; i++) {
			if (charsinfo[i]) {
				snprintf(buf, sizeof(buf), "<category chars=\"%d\" count=\"%d\"/>", i, charsinfo[i]);
				mwWriteXmlString(&pbuf, &bufsize, 2, buf);
			}
		}
	} else if (!strcmp(param->pucRequest, "/query")) {
		char buf[256];
		CATEGORY_INFO *cat;
		CLIP_INFO* clip;
		int hash = id ? atoi(id) : -1;
		if (hash > 0)
			clip = GetClipByHash(hash, &cat);
		else
			clip = GetClipByName(0, id, &cat);
		if (!clip) clip = GetClipByFile(id, &cat);
		if (clip) {
			snprintf(buf, sizeof(buf), "<item id=\"%d\">", id);
			mwWriteXmlString(&pbuf, &bufsize, 2, buf);
			snprintf(buf, sizeof(buf), "<file>%s</file>", clip->filename);
			mwWriteXmlString(&pbuf, &bufsize, 3, buf);
			snprintf(buf, sizeof(buf), "<title>%s</title>", clip->title);
			mwWriteXmlString(&pbuf, &bufsize, 3, buf);
			snprintf(buf, sizeof(buf), "<category>%s</category>", cat->name);
			mwWriteXmlString(&pbuf, &bufsize, 3, buf);
			mwWriteXmlString(&pbuf, &bufsize, 2, "</item>");
		}
	}

	mwWriteXmlString(&pbuf, &bufsize, 0, "</response>");

	param->iDataBytes=(int)(pbuf-param->pucBuffer);
	param->fileType=HTTPFILETYPE_XML;
	return FLAG_DATA_RAW | FLAG_TO_FREE;
}

int uhVod(UrlHandlerParam* param)
{
	HTTP_XML_NODE node;
	char *req=param->pucRequest;
	char *pbuf = param->pucBuffer;
	int bufsize = param->iDataBytes;
	//static VOD_CLIENT_ACTIONS action=0;
	int session = 0;
	char *arg;
	char *id;

	DBG("Session: %d\n", session);
	param->fileType=HTTPFILETYPE_XML;
	node.indent = 1;
	node.fmt = "%s";
	mwWriteXmlHeader(&pbuf, &bufsize, 10, 0, 0);
	mwWriteXmlString(&pbuf, &bufsize, 0, "<response>");

	mwParseQueryString(param);
	
	session = mwGetVarValueInt(param->pxVars, "session", 0);
	arg = mwGetVarValue(param->pxVars, "arg");
	id = mwGetVarValue(param->pxVars, "id");
	switch (GETDWORD(param->pucRequest + 1)) {
	case DEFDWORD('n','o','p',0):
		strcpy(pbuf,"state=OK");
		break;
	case DEFDWORD('c','m','d',0):
		//action=ACT_SKIP;
		strcpy(pbuf,"Play next");
		param->iDataBytes=9;
		return FLAG_DATA_RAW;
	case DEFDWORD('l','i','s','t'):
		mwWriteXmlString(&pbuf, &bufsize, 1, "<queued>");
		//if (listindex>=listcount) listindex=0;
		{
		PL_ENTRY *ptr = plhdr[session];
		int i;
		for (i=0; ptr; ptr = ptr->next, i++) {
			OutputItemInfo(&pbuf, &bufsize, ptr->data);
		}
		}
		mwWriteXmlString(&pbuf, &bufsize, 1, "</queued>");
		break;
	case DEFDWORD('a','d','d',0): {
		node.name = "state";
		if (plFindEntry(plhdr[session],(void*)id, strlen(id))) {
			OutputItemInfo(&pbuf, &bufsize, id);
			node.value = "ordered";
			mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
			break;
		}
		if (plAddEntry(&plhdr[session], (void*)id, strlen(id) + 1)) {
			node.value =  "OK";
			OutputItemInfo(&pbuf, &bufsize, id);
		} else {
			node.value =  "error";
		}
		node.name = "state";
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		} break;
	case DEFDWORD('d','e','l',0):
		OutputItemInfo(&pbuf, &bufsize, id);
		node.name = "state";
		node.value = plDelEntry(&plhdr[session],(void*)id) ? "OK" : "error";
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		break;
	case DEFDWORD('p','i','n',0):
		OutputItemInfo(&pbuf, &bufsize, id);
		node.name = "state";
		node.value = plPinEntry(&plhdr[session],(void*)id) ? "OK" : "error";
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		break;
	case DEFDWORD('p','l','a','y'): {
		PL_ENTRY* entry = plGetEntry(&plhdr[session]);
		node.name = "state";
		if (entry) {
			OutputItemInfo(&pbuf, &bufsize, entry->data);
			node.value = "OK";
		} else {
			node.value = "error";
		}
		mwWriteXmlLine(&pbuf, &bufsize, &node, 0);
		} break;
	default:
		strcpy(pbuf,"Invalid request");
	}
		
	/*
	if (action) {
		pbuf+=sprintf(pbuf,"\nact=skip");
		action=ACT_NOTHING;
	}
	*/
	mwWriteXmlString(&pbuf, &bufsize, 0, "</response>");
	param->iDataBytes=(int)(pbuf-param->pucBuffer);
	return FLAG_DATA_RAW;
}

//////////////////////////////////////////////////////////////////
// Playlist implementation
//////////////////////////////////////////////////////////////////

PL_ENTRY* plAddEntry(PL_ENTRY **hdr, void* data, int datalen)
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
	ptr->datalen = datalen;
	ptr->next=NULL;
	listcount++;
	return ptr;
}

PL_ENTRY* plGetEntry(PL_ENTRY **hdr)
{
	PL_ENTRY *ptr=*hdr;
	void* data;
	if (!ptr) return NULL;
	data=ptr->data;
	*hdr=ptr->next;
	free(ptr);
	listcount--;
	return ptr;
}

PL_ENTRY* plFindEntry(PL_ENTRY *hdr, void* data, int datalen)
{
	PL_ENTRY *ptr;
	for (ptr=hdr; ptr; ptr=ptr->next) {
		if (!memcmp(ptr->data, data, min(datalen, ptr->datalen))) return ptr;
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

PL_ENTRY* plPinEntry(PL_ENTRY **hdr, void* data)
{
	PL_ENTRY *ptr=*hdr,*prev=NULL;
	while (ptr) {
		if (ptr->data==data) {
			if (!prev) return ptr;
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
