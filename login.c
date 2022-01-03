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

// 数据库验证登录信息
int verify_login_data(const char *username, const char *password, const char *date)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_FIELD *field;
	MYSQL_ROW row;
	char mysqlquery[2048];

	// 数据库初始化
	mysql_init(&mysql);

	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
		return -1;

	// 准备数据
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from registered where username='%s' and password='%s';", username, password);

	// 查询验证数据
	mysql_query(&mysql, mysqlquery);

	// 将查询结果读取
	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) != NULL)
		{
			mysql_free_result(result);

			memset(mysqlquery, 0, sizeof(mysqlquery));
			sprintf(mysqlquery, "update registered set lastlogintime='%s' where username='%s' and password='%s';", date, username, password);
			mysql_query(&mysql, mysqlquery);
			mysql_query(&mysql, "flush privileges;");

			mysql_close(&mysql);
			return 0;
		}
		else
		{
			mysql_free_result(result);
			mysql_close(&mysql);
			return -1;
		}
	}

	mysql_close(&mysql);

	return -1;
}



int main()
{
	while (FCGI_Accept() >= 0) 
	{
		int len = 0;
		char *contentlength = getenv("CONTENT_LENGTH");

		// 发送回应
		printf("Content-type: application/json\r\n\r\n");

		if(contentlength != NULL)
		{
			len = strtol(contentlength, NULL, 10);
		}

		if(len <= 0)
		{
			printf("{\"code\":\"002\"}");	//验证失败
		}
		else
		{
			char *buf = (char *)malloc(len);
			if(buf == NULL)
			{
				printf("{\"code\":\"002\"}");	//验证失败
				continue;
			}

			// 从客户端读入数据
			int readlength = fread(buf, 1, len, stdin);
			if(readlength <= 0)
			{
				printf("{\"code\":\"002\"}");	//验证失败
				free(buf);
				continue;
			}

			char *username = get_json(buf, "username");
			char *password = get_json(buf, "password");
			char *date = get_json(buf, "logindate");

			int ok = verify_login_data(username, password, date);

			if(ok == 0)
			{
				printf("{\"code\":\"003\"}");	//验证成功
			}
			else
			{
				printf("{\"code\":\"002\"}");	//验证失败
			}

			if(username != NULL)
				free(username);
			if(password != NULL)
				free(password);
			if(date != NULL)
				free(date);
			if(buf != NULL)
				free(buf);
		}
	}

	return 0;
}
