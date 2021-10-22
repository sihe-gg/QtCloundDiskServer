#include <string.h>
#include <stdlib.h>
#include <unistd.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>

#include "mysql.h"
#include "fcgi_stdio.h"


// 客户端下载文件解析
int get_download_fileinfo(char **p_username, char **p_filename, char **p_md5, long **p_size)
{
	char *user, *file, *md5, *tempsize;
	char buf[1024];
	long *size;

	user = (char *)malloc(64);
	file = (char *)malloc(128);
	md5 = (char *)malloc(33);
	size = (long *)malloc(sizeof(long));
	if(user == NULL || file == NULL || md5 == NULL || size == NULL)
	{
		return -1;
	}

	tempsize = (char *)malloc(20);
	if(tempsize == NULL)
	{
		return -1;
	}
	
	fread(buf, 1, 1024, stdin);
	
	sscanf(buf, "%s %s %s %s ", user, file, md5, tempsize);

	*size = strtol(tempsize, NULL, 10);

	//printf("%s  %s  %s  %d\n", user, file, md5, *size);

	*p_username = user;
	*p_filename = file;
	*p_md5 = md5;
	*p_size = size;
	
	free(tempsize);

	return 0;
}

// 将信息写入数据库
int write_download_data(char *filename, char *md5, long *size)
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
int download_file(char *filename, char *md5, long *size)
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
		char *username, *filename, *md5;
		long *size;

		printf("Content-type: multipart/form-data\r\n\r\n");

		//int get_download_fileinfo(char **p_username, char **p_filename, char **p_md5, long **p_size)
		int flag = get_download_fileinfo(&username, &filename, &md5, &size);
		// 获取用户信息成功
		if(flag == 0)
		{
			//int download_file(char *filename, char *md5, long *size)
			download_file(filename, md5, size);
		}

		if(username != NULL)
			free(username);
		if(filename != NULL)
			free(filename);
		if(md5 != NULL)
			free(md5);
		if(size != NULL)
			free(size);
	}

	return 0;
}
