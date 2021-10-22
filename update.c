#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fcgi_stdio.h"

void checkupdate()
{
	char buf[4096];
	FILE *pf = fopen("/home/jaks/version.json", "r");
	if(pf == NULL)
	{
		printf("\"code\":\"001\"");
		return;
	}

	memset(buf, 0, sizeof(buf));
	int len = fread(buf, 1, sizeof(buf), pf);

	fwrite(buf, 1, len, stdout);

	fclose(pf);
}

int main()
{
	while(FCGI_Accept() >= 0)
	{
		printf("Content-type: application/json\r\n\r\n");
		checkupdate();	
	}

	return 0;
}
