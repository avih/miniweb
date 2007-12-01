#include <windows.h>
#include <stdio.h>
#include "httpget.h"

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	char *data;
	if (argc <= 2) return -1;
	if ( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) ) {
		return 0;
	}
	data = PostFile(argv[1], "file", argv[2]);
	printf("%s\n", data);
	WSACleanup( );
	getchar();
	return 0;
}