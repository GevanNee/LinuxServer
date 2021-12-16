#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include "ngx_global.h"
#include "ngx_func.h"
#include "ngx_c_conf.h"
#include "ngx_macro.h"


/*
��ӡ�ַ����кܶ෽ʽ��һ���ַ�����ӡ���ַ�������������ַ�����Ŀ�����
���в�����ͬ����Щ��va_list����Щ����������(...)��Ϊ����
*/

static u_char error_levels[][20]
{
	{"stderr"},
	{"emergency"},
	{"alert"},
	{"critical"},
	{"error"},
	{"warn"},
	{"notice"},
	{"infomation"},
	{"debug"},
};

ngx_log_t ngx_log;

void ngx_log_init()
{
	u_char* log_name = nullptr;
	ngx_c_conf* p_config = ngx_c_conf::getInstance();

	log_name = (u_char*)p_config->getString("log");
	if (log_name == nullptr)
	{
		log_name = (u_char*)NGX_ERROR_LOG_PATH;
	}

	ngx_log.log_level = p_config->getInt("loglevel", NGX_LOG_NOTICE);
	if (ngx_log.log_level > 8 || ngx_log.log_level < 0)
	{
		ngx_log.log_level = NGX_LOG_NOTICE;
	}

	ngx_log.fd = open((const char*)log_name, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (ngx_log.fd == -1)
	{
		ngx_log_stderr(errno, "log��ʼ��ʧ�ܣ�fd = -1");
		ngx_log.fd = STDERR_FILENO;
	}
}

/*��errno����Ϣ���浽һ��ָ�����ڴ��*/
/*����ֵ����־�ַ�����ĩβ*/
u_char* ngx_log_errno(u_char* buf, u_char* last, int err)
{
	/*���ַ���*/
	char leftString[10]{ 0 };
	sprintf(leftString, "(%d: ", err);
	size_t leftLenth = strlen(leftString);

	/*����Ϣ*/
	char* errorInfo = strerror(err);
	size_t infoLenth = strlen(errorInfo);
	
	/*���ַ���*/
	char rightString[] = " ) ";
	size_t rightLenth = strlen(rightString);

	if (buf + leftLenth + infoLenth + rightLenth < last)
	{
		buf = (u_char*)memcpy(buf, leftString, leftLenth) + leftLenth;
		buf = (u_char*)memcpy(buf, errorInfo, infoLenth) + infoLenth;
		buf = (u_char*)memcpy(buf, rightString, rightLenth) + rightLenth;
	}

	return buf;
}

/*����־��ӡ��stderr��ǳ���Ҫ����Ϣ����Ҫ�����������*/
/* */
void ngx_log_stderr(int err, const char * fmt, ...)
{
	va_list args;

	u_char errorString[NGX_MAX_ERROR_STR + 1]; /*+1���ܸ�����һ��*/
	memset(errorString, 0, sizeof(errorString));

	u_char* p = (u_char*)memcpy(errorString, "nginx: ", 7) + 7;
	u_char* last = errorString + NGX_MAX_ERROR_STR;

	va_start(args, fmt);
	p = ngx_vslprintf(p, last, fmt, args);
	va_end(args);
	
	if (err != 0)
	{
		p = ngx_log_errno(p, last, err);
	}

	/*λ�ò���ҲҪǿ�в��뻻�з�*/
	if (p >= (last - 1))
	{
		p = last - 2;
	}
	*p++ = '\n';

	write(STDERR_FILENO, errorString, p - errorString);

	if (ngx_log.fd > STDERR_FILENO)
	{
		/*����־�ļ���ͬ����Ϣ*/
	}

	return;
}

/*����Ч����*/
void ngx_log_core(int level, int err, const char* fmt, ...)
{
	va_list args;
	
	u_char errstr[NGX_MAX_ERROR_STR + 1];

	memset(errstr, 0, sizeof(errstr));
	u_char*			 p;
	u_char*			 last;

	last = errstr + NGX_MAX_ERROR_STR;

	/*ʱ�����*/
	struct timeval   timeValue;
	struct tm		 tm;
	time_t			 second;

	memset(&timeValue, 0, sizeof(struct timeval));
	memset(&tm, 0, sizeof(struct tm));

	gettimeofday(&timeValue, nullptr);

	second = timeValue.tv_sec;
	localtime_r(&second, &tm); /*��_r�����̰߳�ȫ���������������⣬������������*/
	/*�Ż������� https://blog.csdn.net/pengzhouzhou/article/details/87095635 */
	tm.tm_year += 1900;
	tm.tm_mon++;

	/*��ϳ�һ��ʱ���ַ��������磺2021/11/08 05:30:00*/
	u_char currentTimestr[40] = {0};
	ngx_slprintf(currentTimestr, currentTimestr + 40, "%4d/%02d/%02d %02d:%02d:%02d", 
				tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	
	p = (u_char*)memcpy(errstr, currentTimestr, strlen((const char*)currentTimestr)) + strlen((const char*)currentTimestr);
	p = ngx_slprintf(p, last, " [%s] ", error_levels[level]);                //��־�������ӽ������õ����磺  2019/01/08 20:26:07 [crit] 
	p = ngx_slprintf(p, last, "%P: ", ngx_pid);                             //֧��%P��ʽ������id���ӽ������õ����磺   2019/01/08 20:50:15 [crit] 2037:

	va_start(args, fmt);
	p = ngx_vslprintf(p, last, fmt, args);
	va_end(args);

	if (err > 0)
	{
		p = ngx_log_errno(p, last, err);
	}

	if (p >= last - 1)
	{
		p = last - 1 - 1;
	}
	*p++ = '\n';

	size_t n;
	while (1)
	{
		if (level > ngx_log.log_level)
		{
			break;
		}

		n = write(ngx_log.fd, errstr, p - errstr);
		if (n == -1)
		{
			if (errno == ENOSPC) //дʧ�ܣ���ԭ���Ǵ���û�ռ���
			{
				n = write(STDERR_FILENO, "disk was full\n", 15);
				n = write(STDERR_FILENO, errstr, p - errstr);
			}
			else
			{
				//����������������ô�ҿ��ǰ����������ʾ����׼�����豸�ɣ�
				if (ngx_log.fd != STDERR_FILENO) //��ǰ�Ƕ�λ���ļ��ģ�����������
				{
					n = write(STDERR_FILENO, "write log file error\n", 21);
					n = write(STDERR_FILENO, errstr, p - errstr);
				}
			}
		}

		break;
	}
}
