#include <stdlib.h>
#include <string.h>

#include "mysql.h"
#include "fcgi_stdio.h"

// 取出登录信息
int get_login_data(char **username, char **password, char **p_date)
{
	char *name, *pwd, *date;
	char *p, *t;
	char *begin;
	char *buf;

	// 开辟内存空间
	name = (char *)malloc(64);
	pwd = (char *)malloc(64);
	date = (char *)malloc(30);
	if(name == NULL || pwd == NULL || date == NULL)
		return -1;

	buf = (char *)malloc(4096);
	if(buf == NULL)
		return -1;

	// 从客户端读入数据
	int len = fread(buf, 1, 4096, stdin);
	if(len <= 0)
	{
		free(buf);
		return -1;
	}

	// 登录数据分割
	begin = buf;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(date, p, t-p);
	date[t-p] = '\0';

	begin = t;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(pwd, p, t-p);
	pwd[t-p] = '\0';

	begin = t;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(name, p, t-p);
	name[t-p] = '\0';

	// 传出登录信息数据
	*username = name;
	*password = pwd;
	*p_date = date;

	free(buf);
	return 0;

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
		char *username, *password, *date;
		int flag;
		int ok = -1;;

		flag = get_login_data(&username, &password, &date);

		if(flag == 0)
		{
			ok = verify_login_data(username, password, date);
		}

		// 发送回应
		printf("Content-type: application/json\r\n\r\n");
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
	}

	return 0;
}
