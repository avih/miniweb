typedef struct {
	char* name;
	char* fmt;
	void* value;
	int indent;
} HTTP_XML_NODE;

int mwWriteXmlHeader(char** pbuf, int* pbufsize, int ver, char* charset, char* xsl);
int mwWriteXmlLine(char** pbuf, int* pbufsize, HTTP_XML_NODE *node, char *attr);
void mwWriteXmlString(char** pbuf, int* pbufsize, int indent, char* str);
