#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mysql.h"
#include "fcgi_stdio.h"

char *get_value(char *src, char *key)
{
        char *value;

        char *begin = strstr(src, key);
        if(begin == NULL)
        {   
                return NULL;
        }   
        begin += strlen(key);

        // 找到第一个是字母数字的位置
        while(!isalnum(src[begin - src]))
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

	memset(mysqlquery, 0, sizeof(mysqlquery));
	sprintf(mysqlquery, "select * from fileInfo where md5='%s';", md5);

	mysql_query(&mysql, mysqlquery);
	result = mysql_store_result(&mysql);

	// 先判断是否MD5重复，为真则继续查验MD5和用户名
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
				char *new_filename = (char *)realloc(filename, strlen(tempname));
				if(new_filename != NULL)
				{
					filename = new_filename;
					memset(filename, 0, strlen(filename));
					strcpy(filename, tempname);
				}
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
int upload_data(char *username, char *filename, char *md5, char *size, int content_len)
{
	// 查询文件是否存在数据库中
	int flag = file_is_repeat(username, filename, md5);
	if(flag == -1)
		return -1;	//文件存在
	else if(flag == 1)
		return 0;	//文件已经上传过，无需再次上传, 添加进自己文件列表


	// 设置最大读取4096
	int max_read = content_len > 4096 ? 4096 : content_len;
	long long file_size = strtoll(size, NULL, 10);

	char *buf = (char *)malloc(max_read);
	if(buf == NULL)
	{
		printf("开辟内存错误\n");
		return -1;
	}

	memset(buf, 0, max_read);
	int len = fread(buf, 1, max_read, stdin);
	if(len <= 0)
	{
		printf("读 body 失败\n");
		free(buf);
		return -1;
	}

	char *file_buf = strstr(buf, "\r\n\r\n");
	if(file_buf == NULL)
	{
		return -1;
	}
	file_buf += strlen("\r\n\r\n");


	// 写入文件进入磁盘
	int fd = open(filename, O_CREAT | O_WRONLY, 0777);
	max_read -= (file_buf - buf);	// 最大读取 - 已经跳过的前缀长度

	write(fd, file_buf, max_read);

	// 循环读写文件
	if(file_size > max_read)
	{
		max_read = 4096;

		memset(buf, 0, max_read);
		while((max_read = fread(buf, 1, max_read, stdin)) > 0)
		{
			write(fd, buf, max_read);

			memset(buf, 0, max_read);
		}
	}

	// 截断文件
	ftruncate(fd, file_size);

	close(fd);

	if(buf != NULL)
		free(buf);

	return 0;

}


// 数据库验证文件是否存在
int verify_upload_data(const char *username, const char *filename, const char *md5, const char *size, const char *date)
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
	sprintf(mysqlquery, "insert into fileInfo(filename,md5,num,user,size,uploadtime) values('%s','%s',%d,'%s','%s','%s');"
				, filename, md5, num, username, size, date);

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
		char *disposition = getenv("HTTP_CONTENT_DISPOSITION");
		int len = 0;

		printf("Content-type: text/plain; charset=utf-8\r\n\r\n");
		if(contentlength != NULL)
		{
			len = strtol(contentlength, NULL, 10);
		}

		if(len <= 0)
		{
			printf("没有文件可以上传\n");
		}
		else
		{
			char *username = get_value(disposition, "name");
			char *filename = get_value(disposition, "filename");
			char *md5 = get_value(disposition, "md5");
			char *size = get_value(disposition, "size");
			char *date = get_value(disposition, "date");
			int ok = -1;

			int flag = upload_data(username, filename, md5, size, len);
			if(flag == -1)
	                {
	                        // 文件存在数据库
	                        printf("文件名：%s，MD5：%s\n", filename, md5);
	                        printf("您上传的文件已经存在，请重新选择上传文件\n");   //验证失败
	                }
			else
			{
				// 继续验证数据，合格后写入数据库
				//int verify_upload_data(const char *username, const char *filename, const char *md5, long *size)
				ok = verify_upload_data(username, filename, md5, size, date);
			}
			
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

	}

	return 0;
}
