#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
	
	int myfile_num = 0;
	int share_num = 0;

	// 文件数量
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select count(*) from fileInfo where user='%s';", username);
	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}

	result = mysql_store_result(&mysql);
	if((row = mysql_fetch_row(result)) != NULL)
	{
		char *temp_num = row[0];
		myfile_num = strtol(temp_num, NULL, 10);
	}
	mysql_free_result(result);
	
	// 共享文件数量
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select count(*) from sharedList");
	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}
	result = mysql_store_result(&mysql);
	if((row = mysql_fetch_row(result)) != NULL)
	{
		char *temp_num = row[0];
		share_num = strtol(temp_num, NULL, 10);
	}
	mysql_free_result(result);

//---------------------------查询发送拥有的文件------------------------------------------------------------
	char json_file_head[] = "{\"files\":[\n";
	char json_file_end[] = "],\n";
	char json_share_head[] = "\"share\":[\n";
	char json_share_end[] = "]}";

	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where user='%s';", username);

	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}
	
	printf("%s", json_file_head);
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
		if(myfile_num  > 1)
		{
			printf("{\"filename\":\"%s\",\"md5\":\"%s\",\"num\":\"%s\",\"size\":\"%s\",\"date\":\"%s\"},\n", filename, md5, num, size, date);
		}
		else
		{
			printf("{\"filename\":\"%s\",\"md5\":\"%s\",\"num\":\"%s\",\"size\":\"%s\",\"date\":\"%s\"}\n", filename, md5, num, size, date);
		}
		myfile_num--;
	}
	printf("%s", json_file_end);

	mysql_free_result(result);

//---------------------------查询发送共享文件------------------------------------------------------------
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from sharedList;");

	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}

	printf("%s", json_share_head);
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
		if(share_num > 1)
		{
			printf("{\"username\":\"%s\",\"sharefile\":\"%s\",\"sharemd5\":\"%s\",\"sharesize\":\"%s\",\"downloadcount\":\"%s\",\"sharedate\":\"%s\"},\n",
				 user, sharefile, sharemd5, sharesize, downloadcount, sharedate);
		}
		else
		{
			printf("{\"username\":\"%s\",\"sharefile\":\"%s\",\"sharemd5\":\"%s\",\"sharesize\":\"%s\",\"downloadcount\":\"%s\",\"sharedate\":\"%s\"}\n",
				 user, sharefile, sharemd5, sharesize, downloadcount, sharedate);
		}
		share_num--;
	}
	printf("%s", json_share_end);

	mysql_free_result(result);
	mysql_close(&mysql);

	return 0;
}

int main()
{
	while(FCGI_Accept() >= 0)
	{
		char *contentlength = getenv("CONTENT_LENGTH");
		int len = 0;
		
		printf("Content-type: application/json\r\n\r\n");
		if(contentlength != NULL)
		{
			len = strtol(contentlength, NULL, 10);
		}

		if(len <= 0)
		{
			printf("{\"code\" : \"000\"}");
		}
		else
		{
			char *buf = (char *)malloc(len);
			if(buf == NULL)
			{
				printf("{\"code\" : \"000\"}");
				continue;
			}

			int readlength = fread(buf, 1, len, stdin);
			if(readlength <= 0)
			{
				free(buf);
				printf("{\"code\" : \"000\"}");
				continue;
			}
				
			char *username = get_json(buf, "username");
			if(username != NULL)
			{
				// 查询用户名下文件
				get_user_file_list(username);
				free(username);
			}
			else
			{
				printf("{\"code\" : \"000\"}");
			}

			free(buf);
		}
	}

	return 0;
}
