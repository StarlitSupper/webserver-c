#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

// 日志级别
typedef enum {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} LogLevel;

// 初始化日志（创建目录、文件）
void logger_init();

// 记录系统日志
void log_system(LogLevel level, const char* module, const char* format, ...);

// 记录访问日志
void log_access(const char* client_ip, const char* method, const char* path, 
                const char* protocol, int status_code, long response_size);
                
void logger_init_with_config(const char* log_level, const char* log_path);

#endif
