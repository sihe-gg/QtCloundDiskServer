#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mysql.h"
#include "fcgi_stdio.h"

// 获取客户端发送的用户名
char *get_username()
{
	char *username;

	username = (char *)malloc(64);
	if(username == NULL)
	{
		return NULL;
	}

	fread(username, 1, 64, stdin);

	return username;
}

// 发送给客户端文件列表信息
void send_commonfile_list(char *filename, char *md5, char *num, char *size, char *date)
{
	// 发送json数组
	printf("{\"files\":{\"filename\":\"%s\",\"md5\":\"%s\",\"num\":\"%s\",\"size\":\"%s\",\"date\":\"%s\"}}\n", filename, md5, num, size, date);
}

// 发送给客户端分享文件信息
void send_sharefile_list(char *username, char *sharefile, char *sharemd5, char *sharesize, char *downloadcount, char *sharedate)
{
	// 发送json数组
	printf("{\"share\":{\"username\":\"%s\",\"sharefile\":\"%s\",\"sharemd5\":\"%s\",\"sharesize\":\"%s\",\"downloadcount\":\"%s\",\"sharedate\":\"%s\"}}\n", 
		username, sharefile, sharemd5, sharesize, downloadcount, sharedate);
}

// 通过用户名查询上传的文件
int get_user_file_list(char *username)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char *filename, *md5, *num, *size, *date;
	char *sharefile, *sharemd5, *sharesize, *downloadcount, *user, *sharedate;
	char mysqlquery[1024];
	int n;

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		//printf("Error connecting to databases:%s\n", mysql_error(&mysql));
		return -1;
	}
	

//---------------------------查询发送拥有的文件------------------------------------------------------------
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where user='%s';", username);

	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		//printf("execute mysql query error:%s\n", mysql_error(&mysql));
		return -1;
	}
	
	// 检索完整结果
	result = mysql_store_result(&mysql);

	while((row = mysql_fetch_row(result)) != NULL)
	{
		for(n = 0; n < mysql_num_fields(result); n++)
		{
			switch(n)
			{
			case 0:
				filename = row[n];
				break;
			case 1:
				md5 = row[n];
				break;
			case 2:
				num = row[n];
				break;
			case 4:
				size = row[n];
				break;
			case 5:
				date = row[n];
				break;
			default:
				break;
			}
		}

		// 发送给客户端文件列表信息
		//void send_commonfile_list(char *filename, char *md5, char *num, char *size)
		send_commonfile_list(filename, md5, num, size, date);
	}

	mysql_free_result(result);

//---------------------------查询发送共享文件------------------------------------------------------------

	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from sharedList;");

	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}

	result = mysql_store_result(&mysql);
	while((row = mysql_fetch_row(result)) != NULL)
	{
		for(n = 0; n < mysql_num_fields(result); n++)
		{
			switch(n)
			{
			case 0:
				sharefile = row[n];
				break;
			case 1:
				sharemd5 = row[n];
				break;
			case 2:
				sharesize = row[n];
				break;
			case 3:
				downloadcount = row[n];
				break;
			case 4:
				user = row[n];
				break;
			case 5:
				sharedate = row[n];
				break;
			default:
				break;
			}
		}

		// 发送分享数据给客户端
 		//void send_sharefile_list(char *username, char *sharefile, char *sharemd5, char *sharesize, char *downloadcount)
		send_sharefile_list(user, sharefile, sharemd5, sharesize, downloadcount, sharedate);
	}
	mysql_free_result(result);

	mysql_close(&mysql);

	return 0;
}


int main()
{

	
	while(FCGI_Accept() >= 0)
	{
		char *username;
		printf("Content-type: application/json\r\n\r\n");

		username = get_username();

		if(username != NULL)
		{
			// 查询用户名下文件
			//int get_user_file_list(char *username)
			get_user_file_list(username);

			free(username);
		}

	}


	return 0;

}
