#include <string.h>
#include <stdlib.h>

#include "mysql.h"
#include "fcgi_stdio.h"


int get_modifypwd_info(char **p_username, char **p_oldpassword, char **p_newpassword)
{
	char *username, *oldpassword, *newpassword;
	char buf[1024];

	username = (char *)malloc(64);
	oldpassword = (char *)malloc(64);
	newpassword = (char *)malloc(64);
	if(username == NULL || oldpassword == NULL || newpassword == NULL)
	{
		return -1;	
	}

	fread(buf, 1, 1024, stdin);

	sscanf(buf, "%s %s %s ", username, oldpassword, newpassword);

	*p_username = username;
	*p_oldpassword = oldpassword;
	*p_newpassword = newpassword;

	return 0;

}

int modify_passwd(char *username, char *oldpassword, char *newpassword)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char mysqlquery[1024];

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		return -1;
	}


	// 查询数据是否存在
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from registered where username='%s' and password='%s';", username, oldpassword);
	mysql_query(&mysql, mysqlquery);

	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) == NULL)
		{
			mysql_free_result(result);
			mysql_close(&mysql);
			return -1;
		}
	}
	else
	{
		return -1;
	}
	mysql_free_result(result);

	// 修改密码 
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "update registered set password='%s' where username='%s' and password='%s';", newpassword, username, oldpassword);
	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");

	mysql_close(&mysql);
	return 0;
}


int main()
{
	while(FCGI_Accept() >= 0)
	{
		
		//int get_modifypwd_info(char **p_username, char **p_password)
		char *username, *oldpassword, *newpassword;
		int ok = -1;

		printf("Content-type: application/json\r\n\r\n");
		int flag = get_modifypwd_info(&username, &oldpassword, &newpassword);

		//int modify_passwd(char *username, char *oldpassword, char *newpassword)
		if(flag == 0)
		{
			//printf("%s  %s   %s \n", username , oldpassword, newpassword);
			ok = modify_passwd(username, oldpassword, newpassword);
		}
		
		switch(ok)
		{
		case 0:
			printf("{\"code\":\"001\"}");
			break;
		default:
			printf("{\"code\":\"002\"}");
			break;
		}

		if(username != NULL)
			free(username);
		if(oldpassword != NULL)
			free(oldpassword);
		if(newpassword != NULL)
			free(newpassword);
	}

	return 0;
}
