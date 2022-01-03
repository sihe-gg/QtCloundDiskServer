#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include "mysql.h"
#include "fcgi_stdio.h"

char *get_json(char *json, char *json_key)
{
        char *value;

        char *begin = strstr(json, json_key);
        if(begin == NULL)
        {   
                return NULL;
        }   
        begin += strlen(json_key);

        // 找到第一个是字母数字的位置
        while(!isalnum(json[begin - json]))
        {   
                begin++;
        }   

        char *end = strchr(begin, '"');

        value = (char *)malloc(end - begin);
        if(value == NULL)
        {   
                return NULL;
        }   

        strncpy(value, begin, end - begin);
        value[end - begin] = '\0';

        return value;
}

// 将信息写入数据库
int write_download_data(char *filename, char *md5, char *size)
{
	MYSQL mysql;
	char mysqlquery[2048];

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
		return -1;

	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "update sharedList set downloadCount=downloadCount+1 where filename='%s' and md5='%s'", filename, md5);
	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");

	mysql_close(&mysql);

	return 0;	
}


// 传输文件到客户端
int download_file(char *filename, char *md5, char *size)
{
	char buf[4096];
	int n;
	FILE *pf = fopen(filename, "rb");
	if(pf == NULL)
	{
		return -1;
	}

	while((n = fread(buf, 1, 4096, pf)) > 0)
	{
		fwrite(buf, 1, n, stdout);

		memset(buf, 0, sizeof(buf));
	}

	fclose(pf);

	// 添加数据库
	//int write_download_data(char *filename, char *md5, long *size)
	write_download_data(filename, md5, size);

	return 0;
}


int main()
{
	while(FCGI_Accept() >= 0)
	{
		char *contentlength = getenv("CONTENT_LENGTH");
		int len = 0;

		printf("Content-type: multipart/form-data\r\n\r\n");
		if(contentlength != NULL)
		{
			len = strtol(contentlength, NULL, 10);
		}
		
		if(len <= 0)
		{
			printf("Nothing!");
		}
		else
		{
			char *buf = (char *)malloc(len);
			if(buf == NULL)
			{
				printf("malloc error!");
				continue;
			}

			int readlength = fread(buf, 1, len, stdin);
			if(readlength <= 0)
			{
				printf("read length <= 0");
				free(buf);
				continue;
			}
			
			char *username = get_json(buf, "username");
			char *filename = get_json(buf, "filename");
			char *md5 = get_json(buf, "md5");
			char *size = get_json(buf, "size");

			//int download_file(char *filename, char *md5, long *size)
			download_file(filename, md5, size);

			if(username != NULL)
				free(username);
			if(filename != NULL)
				free(filename);
			if(md5 != NULL)
				free(md5);
			if(size != NULL)
				free(size);
			if(buf != NULL)
				free(buf);

		}

	}

	return 0;
}
