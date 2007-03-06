// The include file for using 7z library

void* SzInit();
void SzUninit(void* ctx);
int SzExtractContent(void* ctx, char* archive, char* filename, void** pbuf);
