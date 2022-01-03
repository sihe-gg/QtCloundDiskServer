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
int verify_register_data(const char *username, const char *nickname, const char *password, const char *mail, const char *phone, const char *date)
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
	sprintf(mysqlquery, "select * from registered where username='%s';", username);

	// 查询验证数据
	mysql_query(&mysql, mysqlquery);

	// 将查询结果读取
	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) != NULL)
		{
			// 查询到重复用户
			mysql_free_result(result);
			mysql_close(&mysql);
			return -2;
		}
	}
	else
	{
		mysql_close(&mysql);
		return -1;
	}
	mysql_free_result(result);

	// 没有查询到用户名，继续查询
	// 准备数据
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from registered where mail='%s';", mail);

	// 查询验证数据
	mysql_query(&mysql, mysqlquery);

	// 将查询结果读取
	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) != NULL)
		{
			// 查询到重复邮箱
			mysql_free_result(result);
			mysql_close(&mysql);
			return -3;
		}
	}
	else
	{
		mysql_close(&mysql);
		return -1;
	}
	mysql_free_result(result);

	// 没有查询到邮箱，继续查询
	// 准备数据
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from registered where phone='%s';", phone);

	// 查询验证数据
	mysql_query(&mysql, mysqlquery);

	// 将查询结果读取
	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) != NULL)
		{
			// 查询到重复邮箱
			mysql_free_result(result);
			mysql_close(&mysql);
			return -4;
		}
	}
	else
	{
		mysql_close(&mysql);
		return -1;
	}
	mysql_free_result(result);

	// 注册信息不在数据库中，写入注册信息
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "insert into registered(username,nickname,password,mail,phone,regtime) values('%s','%s','%s','%s','%s','%s');"
			, username, nickname, password, mail, phone, date);

	// 插入数据
	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");
	mysql_close(&mysql);
	return 0;
}


int main()
{
	while (FCGI_Accept() >= 0) 
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
			printf("{\"code\":\"008\"}");	//验证失败,服务器错误
		}
		else
		{
			char *username, *nickname, *password, *mail, *phone, *date;

			char *buf = (char *)malloc(len);
			if(buf == NULL)
			{
				printf("{\"code\":\"008\"}");	//验证失败,服务器错误
				continue;
			}
			
			// 从客户端读入数据
			int readlength = fread(buf, 1, len, stdin);
			if(readlength <= 0)
			{       
			        free(buf);
				printf("{\"code\":\"008\"}");	//验证失败,服务器错误
				continue;
			}

			// nick password mail phone date username
			username = get_json(buf, "username");
			nickname = get_json(buf, "nickname");
			password = get_json(buf, "password");
			mail = get_json(buf, "mail");
			phone = get_json(buf, "phone");
			date = get_json(buf, "registerdate");

			int ok = verify_register_data(username, nickname, password, mail, phone, date);

			switch(ok)
			{
				case 0:
					printf("{\"code\":\"007\"}");	//验证成功
					break;
				case -1:
					printf("{\"code\":\"008\"}");	//验证失败,服务器错误
					break;
				case -2:
					printf("{\"code\":\"004\"}");	//验证失败,用户名存在
					break;
				case -3:
					printf("{\"code\":\"005\"}");	//验证失败,邮箱存在
					break;
				case -4:
					printf("{\"code\":\"006\"}");	//验证失败,手机存在
					break;
				default:
					printf("{\"code\":\"008\"}");	//验证失败,服务器错误
					break;
			}

			if(username != NULL)
				free(username);
			if(nickname != NULL)
				free(nickname);
			if(password != NULL)
				free(password);
			if(mail != NULL)
				free(mail);
			if(phone != NULL)
				free(phone);
			if(date != NULL)
				free(date);
			if(buf != NULL)
				free(buf);
		}
	}

	return 0;
}
