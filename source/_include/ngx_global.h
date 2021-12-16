#pragma once

typedef struct 
{
	int log_level;
	int fd;
}ngx_log_t;

extern ngx_log_t ngx_log;
extern pid_t ngx_pid;
extern pid_t ngx_parent;