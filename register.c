#include <stdlib.h>
#include <string.h>

#include "mysql.h"
#include "fcgi_stdio.h"

// 取出注册信息
int get_register_data(char **username, char **nickname, char **password, char **mail, char **phone, char **p_date)
{
	char *name, *nick, *pwd, *email, *mobilephone, *date;
	char *p, *t;
	char *begin;
	char *buf;

	// 开辟内存空间
	name = (char *)malloc(64);
	nick = (char *)malloc(64);
	pwd = (char *)malloc(64);
	email = (char *)malloc(64);
	mobilephone = (char *)malloc(12);
	date = (char *)malloc(30);
	if(name == NULL || nick == NULL || pwd == NULL || email == NULL || mobilephone == NULL || date == NULL)
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
	
	// nick password mail phone date username

	// 登录数据分割
	begin = buf;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(nick, p, t-p);
	nick[t-p] = '\0';

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
	strncpy(email, p, t-p);
	email[t-p] = '\0';

	begin = t;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(mobilephone, p, t-p);
	mobilephone[t-p] = '\0';

	begin = t;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(date, p, t-p);
	date[t-p] = '\0';

	begin = t;
	p = strchr(begin, ':');
	p += 3;		//跳过: "
	t = strchr(p, '"');
	strncpy(name, p, t-p);
	name[t-p] = '\0';

	// 传出登录信息数据
	*username = name;
	*password = pwd;
	*mail = email;
	*phone = mobilephone;
	*nickname = nick;
	*p_date = date;

	free(buf);
	return 0;

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
		char *username, *nickname, *password, *mail, *phone, *date;
		int flag;
		int ok = -1;;

		flag = get_register_data(&username, &nickname, &password, &mail, &phone, &date);

		if(flag == 0)
		{
			ok = verify_register_data(username, nickname, password, mail, phone, date);
		}

		// 发送回应
		printf("Content-type: application/json\r\n\r\n");
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
	}

	return 0;
}
