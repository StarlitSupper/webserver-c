#include <errno.h>
#include "logger.h"

#define SYS_LOG_PATH "../logs/catalina.log"          // 系统日志路径
#define ACCESS_LOG_PATH "../logs/localhost_access_log.txt"  // 访问日志路径

// 创建日志目录（../logs）
static void create_log_dir() {
    struct stat st;
    if (stat("../logs", &st) == -1) {
        if (mkdir("../logs", 0755) == -1) {
            fprintf(stderr, "创建日志目录失败: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

// 初始化日志
void logger_init() {
    create_log_dir();
    
    // 测试文件可写性（非必须，调试用）
    FILE* sys_log = fopen(SYS_LOG_PATH, "a");
    FILE* access_log = fopen(ACCESS_LOG_PATH, "a");
    
    if (!sys_log || !access_log) {
        fprintf(stderr, "日志文件无法写入: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fclose(sys_log);
    fclose(access_log);
    
    log_system(LOG_INFO, "Logger", "日志系统初始化完成");
}

// 格式化 Tomcat 系统日志时间
static void format_tomcat_system_time(char* buf, size_t buf_size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buf, buf_size, "%d-%b-%Y %H:%M:%S", tm_info);
    
    // 补充毫秒（简陋实现）
    struct timeval tv;
    gettimeofday(&tv, NULL);
    snprintf(buf + strlen(buf), buf_size - strlen(buf), ".%03ld", tv.tv_usec / 1000);
}

// 格式化 Tomcat 访问日志时间
static void format_tomcat_access_time(char* buf, size_t buf_size) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buf, buf_size, "%d/%b/%Y:%H:%M:%S %z", tm_info);
}

// 记录系统日志（Tomcat 风格）
void log_system(LogLevel level, const char* module, const char* format, ...) {
    FILE* log_file = fopen(SYS_LOG_PATH, "a");
    if (!log_file) return;
    
    char time_buf[64];
    format_tomcat_system_time(time_buf, sizeof(time_buf));
    
    const char* level_str = "";
    switch (level) {
        case LOG_DEBUG:   level_str = "DEBUG"; break;
        case LOG_INFO:    level_str = "INFO";  break;
        case LOG_WARNING: level_str = "WARNING"; break;
        case LOG_ERROR:   level_str = "ERROR"; break;
    }
    
    fprintf(log_file, "%s %s [%s] %s. - ", 
            time_buf, level_str, "main", module);
    
    va_list args;
    va_start(args, format);
    vfprintf(log_file, format, args);
    va_end(args);
    
    fprintf(log_file, "\n");
    fclose(log_file);
}

// 记录访问日志（Tomcat 风格）
void log_access(const char* client_ip, const char* method, const char* path, 
                const char* protocol, int status_code, long response_size) {
    FILE* log_file = fopen(ACCESS_LOG_PATH, "a");
    if (!log_file) return;
    
    char time_buf[64];
    format_tomcat_access_time(time_buf, sizeof(time_buf));
    
    fprintf(log_file, "%s - - [%s] \"%s %s %s\" %d %ld\n", 
            client_ip, time_buf, method, path, protocol, status_code, response_size);
    
    fclose(log_file);
}
