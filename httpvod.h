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

