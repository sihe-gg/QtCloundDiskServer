#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mysql.h"
#include "fcgi_stdio.h"

// 连接数据库查看文件是否在库中
int file_is_repeat(char *username, char *filename, const char *md5)
{
	MYSQL mysql;
	MYSQL_RES *result;
	char mysqlquery[256];

	// 连接数据库
	mysql_init(&mysql);
	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
	{
		return -1;
	}

	// 准备查询md5和用户名,是否重复上传
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where md5='%s';", md5);

	mysql_query(&mysql, mysqlquery);
	result = mysql_store_result(&mysql);

	if(result)
	{
		if(mysql_fetch_row(result) != NULL)
		{
			// 存在md5
			mysql_free_result(result);

			// 查询用户名
			memset(mysqlquery, 0, sizeof(mysqlquery));
			sprintf(mysqlquery, "select * from fileInfo where md5='%s' and user='%s';", md5,  username);
	
			mysql_query(&mysql, mysqlquery);
			result = mysql_store_result(&mysql);

			if(result)
			{
				if(mysql_fetch_row(result) != NULL)
				{
					// 存在用户名，文件已经被上传
					mysql_free_result(result);
					mysql_close(&mysql);
					return -1;
				}
				else
				{
					// 文件被别人上传，添加进自己的列表
					mysql_free_result(result);
					mysql_close(&mysql);
					return 1;
				}
			}
			else
			{
				return -1;
			}
		}
	}
	else
	{
		return -1;
	}

	// 不存在md5
	mysql_free_result(result);
//---------------------------------------------------------------------

	// 查询文件名是否重复,重复则重命名
	int num = 1;
	char *namebuf, *suffixbuf;
	char *tempname;
	tempname = (char *)malloc(64);
	if(tempname == NULL)
	{
		return -1;
	}

	// 临时赋值验证文件名
	memset(tempname, 0, 64);
	strcpy(tempname, filename);

	namebuf = strtok(filename, ".");
	suffixbuf = strtok(NULL, " ");
	while(1)
	{
		memset(mysqlquery, 0, sizeof(mysqlquery));
		sprintf(mysqlquery, "select * from fileInfo where filename='%s';", tempname);

		mysql_query(&mysql, mysqlquery);
		result = mysql_store_result(&mysql);

		if(result)
		{
			if(mysql_fetch_row(result) == NULL)
			{
				// 不存在文件重名
				// 临时文件名赋值真实文件名
				memset(filename, 0, 64);
				strcpy(filename, tempname);
				break;
			}
			else
			{
				// 存在文件重名, 更改文件名
				memset(tempname, 0, 64);
				sprintf(tempname, "%s%d.%s", namebuf, num, suffixbuf);
				num++;
			}
		}
		else
		{
			mysql_free_result(result);
			mysql_close(&mysql);
			return -1;
		}
	}

	free(tempname);
	mysql_free_result(result);
	mysql_close(&mysql);
	return 0;
}

// 取出上传的文件信息
int get_upload_data(char **username, char **filename, char **md5, long **psize, char **pdate)
{
	char *name, *file, *crypto, *tmp, *date;
	char *p, *t, *initlen;
	char *begin;
	char *buf;
	long *size;

	// 开辟内存空间
	name = (char *)malloc(64);
	file = (char *)malloc(256);
	crypto = (char *)malloc(33);
	size = (long *)malloc(sizeof(long));
	date = (char *)malloc(128);
	if(name == NULL || file == NULL || crypto == NULL || size == NULL || date == NULL)
		return -1;

	tmp = (char *)malloc(32);
	if(tmp == NULL)
		return -1;

	buf = (char *)malloc(4096);
	if(buf == NULL)
		return -1;

	// 从客户端读入数据
	memset(buf, 0, 4096);
	int len = fread(buf, 1, 4096, stdin);
	if(len <= 0)
	{
		free(buf);
		return -1;
	}

	//printf("Content-type: text/plain\r\n\r\n");

	// 上传数据分割
	initlen = buf;
	// 跳过分割符
	begin = buf;
	p = strstr(begin, "\r\n");
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 2;		//跳过\r\n

	// 跳过content-type
	begin = p;
	p = strstr(begin, "\r\n");
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 2;		//跳过\r\n

	//printf("%s\n", p);

	// 取得用户名
	begin = p;
	p = strchr(begin, '=');
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 1;		//跳过=
	t = strchr(p, ';');
	strncpy(name, p, t-p);
	name[t-p] = '\0';

	// 取得文件名
	begin = t;
	p = strchr(begin, '=');
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 1;		//跳过=
	t = strchr(p, ';');
	strncpy(file, p, t-p);
	file[t-p] = '\0';

	// 取得md5
	begin = t;
	p = strchr(begin, '=');
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 1;		//跳过=
	t = strchr(p, ';');
	strncpy(crypto, p, t-p);
	crypto[t-p] = '\0';

	// 取得size
	begin = p;
	p = strchr(begin, '=');
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 1;		//跳过=
	t = strchr(p, ';');
	strncpy(tmp, p, t-p);
	tmp[t-p] = '\0';

	// 取得date
	begin = t;
	p = strchr(begin, '=');
	if(p == NULL)
	{
		free(buf);
		return -1;
	}
	p += 1;		//跳过=
	t = strstr(p, "\r\n");
	strncpy(date, p, t-p);
	date[t-p] = '\0';

	// 注意：添加数据时把 \r\n换成 ';'

	t += 4;		//跳过\r\n\r\n

	// 传出登录信息数据
	*username = name;
	*filename = file;
	*md5 = crypto;
	*psize = size;
	*pdate = date;

	// 剩余部分大小
	p = t;
	len -= (p - initlen);
	*size = strtol(tmp, NULL, 10);

	// 查询文件是否存在数据库中
	int flag = file_is_repeat(name, file, crypto);
	if(flag == -1)
		return -1;		//文件存在
	if(flag == 1)
		return 0;		//文件已经上传过，无需再次上传，返回成功

	// 写入文件进入磁盘
	char *file_buf;
	file_buf = t;
	int fd = open(file, O_CREAT | O_WRONLY, 0777);
	write(fd, file_buf, len);

	// 循环读写文件
	if(*size > len)
	{
		memset(buf, 0, 4096);
		while((len = fread(buf, 1, 4096, stdin)) > 0)
		{
			write(fd, buf, len);

			memset(buf, 0, 4096);
		}
	}

	// 截断文件
	ftruncate(fd, *size);

	close(fd);

	free(buf);
	free(tmp);
	return 0;

}

// 数据库验证文件是否存在
int verify_upload_data(const char *username, const char *filename, const char *md5, long *size, const char *date)
{
	MYSQL mysql;
	MYSQL_RES *result;
	MYSQL_ROW row;
	char mysqlquery[2048];
	int num = 0;	//被分享次数

	// 数据库初始化
	mysql_init(&mysql);

	if(!mysql_real_connect(&mysql, "192.168.50.244", "root", "root", "register_server", 0, NULL, 0))
		return -1;

	// 准备数据
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from registered where username='%s';", username);

	// 查询用户名验证数据
	mysql_query(&mysql, mysqlquery);

	// 将查询结果读取
	result = mysql_store_result(&mysql);
	if(result)
	{
		if((row = mysql_fetch_row(result)) == NULL)
		{
			// 用户名不存在，非法注入
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


	// 文件第一次上传，存入数据库
	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "insert into fileInfo(filename,md5,num,user,size,uploadtime) values('%s','%s',%d,'%s',%ld,'%s');"
				, filename, md5, num, username, *size, date);

	mysql_query(&mysql, mysqlquery);
	mysql_query(&mysql, "flush privileges;");
	mysql_close(&mysql);

	return 0;
}


int main()
{
	while (FCGI_Accept() >= 0) 
	{
		printf("Content-type: text/plain\r\n\r\n");

		char *username, *filename, *md5, *date;
		long *size;

		int flag = get_upload_data(&username, &filename, &md5, &size, &date);

		if(flag == -1)
		{
			// 文件存在数据库
			printf("文件名：%s，MD5：%s\n", filename, md5);
			printf("您上传的文件已经存在，请重新选择上传文件\n");	//验证失败
			continue;
		}

		// 继续验证数据，合格后写入数据库
		//int verify_upload_data(const char *username, const char *filename, const char *md5, long *size)
		int ok = verify_upload_data(username, filename, md5, size, date);

		// 发送回应
		switch(ok)
		{
			case 0:
				printf("成功！\n");
				printf("%s, 您的文件：%s 上传完毕！\n", username, filename);	//验证成功,数据库写入成功
				printf("上传日期：%s\n", date);
				printf("MD5：%s\n", md5);
				break;
			case -1:
				printf("服务器繁忙，请重新上传！\n");				//验证失败,服务器出错
				break;
			case -2:
				printf("非法用户，非法操作！！！请重新登录\n");			//验证失败,登录用户名不在数据库中
				break;
			default:
				printf("服务器繁忙，请重新上传！\n");				//验证失败,服务器出错
				break;
		}

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
