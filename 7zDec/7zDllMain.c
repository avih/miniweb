/* 
7zMain.c
Test application for 7z Decoder
LZMA SDK 4.43 Copyright (c) 1999-2006 Igor Pavlov (2006-06-04)
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "7zCrc.h"
#include "7zIn.h"
#include "7zExtract.h"

typedef struct _SzContext
{
  ISzInStream InStream;
  FILE *File;
  char* archive;
  CArchiveDatabaseEx db;
  SZ_RESULT res;
  ISzAlloc allocImp;
  ISzAlloc allocTempImp;
  Byte *outBuffer;
  size_t outBufferSize;
} SzContext;

#ifdef _LZMA_IN_CB

#define kBufferSize (1 << 12)
Byte g_Buffer[kBufferSize];

SZ_RESULT SzFileReadImp(void *object, void **buffer, size_t maxRequiredSize, size_t *processedSize)
{
  SzContext *s = (SzContext *)object;
  size_t processedSizeLoc;
  if (maxRequiredSize > kBufferSize)
    maxRequiredSize = kBufferSize;
  processedSizeLoc = fread(g_Buffer, 1, maxRequiredSize, s->File);
  *buffer = g_Buffer;
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
}

#else

SZ_RESULT SzFileReadImp(void *object, void *buffer, size_t size, size_t *processedSize)
{
  SzContext *s = (SzContext *)object;
  size_t processedSizeLoc = fread(buffer, 1, size, s->File);
  if (processedSize != 0)
    *processedSize = processedSizeLoc;
  return SZ_OK;
}

#endif

SZ_RESULT SzFileSeekImp(void *object, CFileSize pos)
{
  SzContext *s = (SzContext *)object;
  int res = fseek(s->File, (long)pos, SEEK_SET);
  if (res == 0)
    return SZ_OK;
  return SZE_FAIL;
}

void PrintError(char *sz)
{
  printf("\nERROR: %s\n", sz);
}

void* SzInit()
{
	void* ctx = calloc(1, sizeof(SzContext));
	return ctx;
}

void SzUninit(void* ctx)
{
	SzContext* archiveStream = (SzContext*)ctx;
	if (archiveStream->File) {
		archiveStream->allocImp.Free(archiveStream->outBuffer);
		SzArDbExFree(&archiveStream->db, archiveStream->allocImp.Free);
		fclose(archiveStream->File);
	}
	if (archiveStream->archive) free(archiveStream->archive);
	free(ctx);
}

int SzExtractContent(void* ctx, char* archive, char* filename, void** pbuf)
{
	SzContext* archiveStream = (SzContext*)ctx;
	FILE* fp = 0;
	SZ_RESULT res;
	if (!archiveStream->archive || strcmp(archive, archiveStream->archive)) {
		fp = fopen(archive, "rb");
		if (!fp) return -1;
	}
	if (archiveStream->File) {
		if (!archive || fp) {
			archiveStream->allocImp.Free(archiveStream->outBuffer);
			SzArDbExFree(&archiveStream->db, archiveStream->allocImp.Free);
			fclose(archiveStream->File);
			archiveStream->File = 0;
			archiveStream->outBuffer = 0; /* it must be 0 before first call for each new archive. */
			archiveStream->outBufferSize = 0;  /* it can have any value before first call (if outBuffer = 0) */
			if (archiveStream->archive) {
				free(archiveStream->archive);
				archiveStream->archive = 0;
			}
		}
	}
	if (!archive) return -1;
	if (!archiveStream->File) {
		archiveStream->File = fp;

		archiveStream->InStream.Read = SzFileReadImp;
		archiveStream->InStream.Seek = SzFileSeekImp;

		archiveStream->allocImp.Alloc = SzAlloc;
		archiveStream->allocImp.Free = SzFree;

		archiveStream->allocTempImp.Alloc = SzAllocTemp;
		archiveStream->allocTempImp.Free = SzFreeTemp;

		InitCrcTable();
		SzArDbExInit(&archiveStream->db);
		res = SzArchiveOpen(&archiveStream->InStream, &archiveStream->db, &archiveStream->allocImp, &archiveStream->allocTempImp);
		if (res != SZ_OK) {
			SzArDbExFree(&archiveStream->db, archiveStream->allocImp.Free);
			fclose(archiveStream->File);
			archiveStream->File = 0;
			return -1;
		}
		archiveStream->archive = _strdup(archive);
	}

	if (filename) {
		UInt32 i;
		/*
		if you need cache, use these 3 variables.
		if you use external function, you can make these variable as static.
		*/
		UInt32 blockIndex = 0xFFFFFFFF; /* it can have any value before first call (if outBuffer = 0) */

		for (i = 0; i < archiveStream->db.Database.NumFiles; i++)
		{
			size_t offset;
			size_t outSizeProcessed;

			CFileItem *f = archiveStream->db.Database.Files + i;
			if (f->IsDirectory) {
				// is directory
				continue;
			}
			if (strcmp(f->Name, filename)) {
				// not matched
				continue;
			}
			res = SzExtract(&archiveStream->InStream, &archiveStream->db, i, 
				&blockIndex, &archiveStream->outBuffer, &archiveStream->outBufferSize, 
				&offset, &outSizeProcessed, 
				&archiveStream->allocImp, &archiveStream->allocTempImp);
			if (res != SZ_OK) return -1;
			if (pbuf) *pbuf = (void*)(archiveStream->outBuffer + offset);
			return (int)outSizeProcessed;
		}
		return -1;
	} else {
		return archiveStream->db.Database.NumFiles;
	}
}
