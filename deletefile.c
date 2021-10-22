#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mysql.h"
#include "fcgi_stdio.h"

// 获取客户端发送的文件名
// 格式:del filename username md5 size  || share filename username md5 size 
int get_name(char **p_flag, char **username, char **filename, char **p_md5, char **p_size, char **p_date)
{
	char *buf;
	char *flag, *file, *user, *md5, *temp, *date;
	char *size;

	flag = (char *)malloc(12);
	user = (char *)malloc(64);
	file = (char *)malloc(128);
	md5 = (char *)malloc(33);
	size = (char *)malloc(30);
	date = (char *)malloc(32);
	if(file == NULL || user == NULL || flag == NULL || md5 == NULL || size == NULL || date == NULL)
	{
		return -1;
	}

	buf = (char *)malloc(1024);
	if(buf == NULL)
	{
		return -1;
	}

	fread(buf, 1, 1024, stdin);

	sscanf(buf, "%s %s %s %s %s %s ", flag, user, file, md5, date, size);

	*p_flag = flag;
	*filename = file;
	*username = user;
	*p_md5 = md5;
	*p_size = size;
	*p_date = date;
	
	free(buf);

	return 0;
}



// 通过文件名查询要操作文件,通过flag判断删除还是分享
int delete_file(char *username, char *filename, char *md5, char *size)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char *num;		//记录是否共享文件，共享文件不能删除
	char mysqlquery[1024];
	int n;

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		//printf("Error connecting to databases:%s\n", mysql_error(&mysql));
		return -1;
	}


	// 查询是否为共享文件
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where user='%s' and filename='%s' and md5='%s';", username, filename, md5);
	
	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		//printf("execute mysql query error:%s\n", mysql_error(&mysql));
		return -1;
	}
	
	// 检索完整结果
	result = mysql_store_result(&mysql);
	
	if((row = mysql_fetch_row(result)) != NULL)
	{
		for(n = 0; n < mysql_num_fields(result); n++)
		{
			if(n == 2)
			{
				num = row[n];
				break;
			}
		}
	}

	mysql_free_result(result);

	// 数据库删除文件记录
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "delete from fileInfo where user='%s' and filename='%s' and md5='%s';", username, filename, md5);
	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");

	// 查询是否还有其他用户享有文件,及能否删除文件
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where md5='%s';", md5);
	mysql_query(&mysql, mysqlquery);
	result = mysql_store_result(&mysql);
	if(result)
	{
		if(mysql_fetch_row(result) == NULL)
		{
			// 没有其他用户持有此文件
			// 通过num判断是否为共享文件，能否从磁盘上删除
			int number = strtol(num, NULL, 10);
			if(number == 0)
			{
				remove(filename);
			}
		}
	}

	mysql_free_result(result);

	mysql_close(&mysql);

	return 0;
}

// 客户端用户分享文件进入数据库
int share_file(char *username, char *filename, char *md5, char *size, char *date)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char *num;		//记录是否共享文件，共享文件不能删除
	char mysqlquery[1024];
	int n;

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		return -1;
	}

	// 取出用户文件下的num，先判断是否分享过，再更改分享状态
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where user='%s' and md5='%s';", username, md5);
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
			// 取出分享状态
			if(n == 2)
			{
				num = row[n];
			}
		}
	}

	mysql_free_result(result);

	int number = strtol(num, NULL, 10);
	if(number == 1)		// 已经被分享过，返回
	{
		return -2;
	}

	// 文件未被分享，分享文件
	// 更新状态
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "update fileInfo set num='1' where user='%s' and md5='%s';", username, md5);

	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");

	// 添加记录到数据库
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "insert into sharedList(filename,md5,size,downloadCount,username,sharetime) values('%s','%s','%s',0,'%s','%s');", 
			filename, md5, size, username, date);

	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");

	mysql_close(&mysql);

	return 0;

}

// 客户端取消分享文件操作
int cancel_share_file(char *username, char *filename, char *md5, char *size)
{
	MYSQL mysql;
	char mysqlquery[1024];
	int n;

	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		return -1;
	}

	// 数据库删除分享数据
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "delete from sharedList where filename='%s' and md5='%s';", filename, md5);
	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}

	// 更改用户文件的分享状态为 0 未被分享
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "update fileInfo set num='0' where user='%s' and md5='%s';", username, md5);
	n = mysql_query(&mysql, mysqlquery);
	if(n)
	{
		return -1;
	}

	mysql_query(&mysql, "flush privileges;");

	mysql_close(&mysql);

	return 0;
}

void determine_operation(char *flag, char *username, char *filename, char *md5, char *size, char *date)
{
	int res;

	if(strcmp("del", flag) == 0)
	{
		res = delete_file(username, filename, md5, size);
	}
	else if(strcmp("share", flag) == 0)
	{
		res = share_file(username, filename, md5, size, date);
	}
	else if(strcmp("cancelshare", flag) == 0)
	{
		res = cancel_share_file(username, filename, md5, size);
	}


	switch(res)
	{
	case 0:
		printf("{\"code\":\"001\"}");
		break;
	case -1:
		printf("{\"code\":\"002\"}");
		break;
	case -2:
		printf("{\"code\":\"003\"}");		//文件已经被分享
		break;
	default:
		printf("{\"code\":\"002\"}");
		break;
	}
}

int main()
{
	while(FCGI_Accept() >= 0)
	{
		char *sign, *username, *filename, *md5, *date;
		char *size;

		printf("Content-type: application/json\r\n\r\n");

		//int get_name(char **p_flag, char **username, char **filename, char **p_md5, char **p_size)
		int flag = get_name(&sign, &username, &filename, &md5, &size, &date);

		// 通过sign选择删除文件还是共享文件
		//void determine_operation(char *flag, char *username, char *filename, char *md5, char *size)
		if(flag == 0)
		{
			determine_operation(sign, username, filename, md5, size, date);
		}


		if(sign != NULL)
			free(sign);
		if(username != NULL)
			free(username);
		if(filename != NULL)
			free(filename);
		if(md5 != NULL)
			free(md5);
		if(size != NULL)
			free(size);
		if(date != NULL)
			free(date);
	}


	return 0;

}
