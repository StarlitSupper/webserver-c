#include "http.h"
#include "logger.h"
#include <sys/time.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// URL解码实现
char* url_decode(const char* encoded) {
    int len = strlen(encoded);
    char* decoded = (char*)malloc(len + 1);
    int i, j = 0;
    
    for (i = 0; i < len; i++) {
        if (encoded[i] == '%' && i + 2 < len) {
            char hex[3] = {encoded[i+1], encoded[i+2], '\0'};
            decoded[j++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else if (encoded[i] == '+') {
            decoded[j++] = ' ';
        } else {
            decoded[j++] = encoded[i];
        }
    }
    decoded[j] = '\0';
    return decoded;
}

// 定义MIME类型映射表结构体
typedef struct {
    const char* extension;  // 文件扩展名（如".js"）
    const char* mime_type;  // 对应的MIME类型
} MimeTypeMap;

// MIME类型映射表（按常见类型排序，默认项放最后）
static const MimeTypeMap mime_types[] = {
    {".html", "text/html; charset=utf-8"},
    {".htm",  "text/html; charset=utf-8"},
    {".css",  "text/css; charset=utf-8"},
    {".js",   "text/javascript; charset=utf-8"},  // JS文件的正确MIME类型
    {".png",  "image/png"},
    {".jpg",  "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".gif",  "image/gif"},
    {".json", "application/json; charset=utf-8"},
    {".txt",  "text/plain; charset=utf-8"},
    {NULL,    "application/octet-stream"}  // 默认类型
};

// 获取文件MIME类型（使用映射表查找）
const char* get_mime_type(const char* file_path) {
    // 查找文件扩展名（从最后一个'.'开始）
    const char* ext = strrchr(file_path, '.');
    if (!ext) {
        return mime_types[sizeof(mime_types)/sizeof(mime_types[0]) - 1].mime_type;  // 返回默认类型
    }

    // 遍历映射表，匹配扩展名
    for (int i = 0; mime_types[i].extension != NULL; i++) {
        if (strcmp(ext, mime_types[i].extension) == 0) {
            return mime_types[i].mime_type;  // 找到匹配的MIME类型
        }
    }

    // 未找到匹配项，返回默认类型
    return mime_types[sizeof(mime_types)/sizeof(mime_types[0]) - 1].mime_type;
}

// 读取文件内容（带错误处理）
static char* read_file_content(const char* file_path, long* out_size) {
    int fd = open(file_path, O_RDONLY);
    if (fd == -1) {
        *out_size = -1;
        log_system(LOG_WARNING, "FileHandler", "无法打开文件: %s", file_path);
        return NULL;
    }

    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1) {
        close(fd);
        *out_size = -2;
        log_system(LOG_ERROR, "FileHandler", "获取文件信息失败: %s", file_path);
        return NULL;
    }
    *out_size = file_stat.st_size;

    char* content = (char*)malloc(*out_size);
    if (!content) {
        close(fd);
        *out_size = -3;
        log_system(LOG_ERROR, "FileHandler", "内存分配失败: %s", file_path);
        return NULL;
    }

    ssize_t bytes_read = read(fd, content, *out_size);
    close(fd);

    if (bytes_read != *out_size) {
        free(content);
        *out_size = -4;
        log_system(LOG_ERROR, "FileHandler", "读取文件失败: %s", file_path);
        return NULL;
    }

    log_system(LOG_DEBUG, "FileHandler", "成功读取文件: %s (大小: %ld 字节)", file_path, *out_size);
    return content;
}

// 解析GET请求参数
static void parse_get_params(const char* path, char* key1, char* key2) {
    const char* query = strchr(path, '?');
    if (!query) return;
    
    char params[512];
    strncpy(params, query + 1, sizeof(params) - 1);
    
    char* token = strtok(params, "&");
    while (token) {
        if (strstr(token, "q=")) {
            char* val = url_decode(token + 2);
            strncpy(key1, val, 127);
            free(val);
        } else if (strstr(token, "name=")) {
            char* val = url_decode(token + 5);
            strncpy(key2, val, 127);
            free(val);
        }
        token = strtok(NULL, "&");
    }
}

// 解析POST表单数据
static void parse_post_data(const char* data, char* key1, char* key2) {
    char* copy = strdup(data);
    char* token = strtok(copy, "&");
    
    while (token) {
        if (strstr(token, "key1=")) {
            char* val = url_decode(token + 5);
            strncpy(key1, val, 127);
            free(val);
        } else if (strstr(token, "key2=")) {
            char* val = url_decode(token + 5);
            strncpy(key2, val, 127);
            free(val);
        }
        token = strtok(NULL, "&");
    }
    free(copy);
}

// 查询文件内容
static char* query_file(const char* filename, const char* keyword, long* out_len) {
    char file_path[256];
    snprintf(file_path, sizeof(file_path), "../blog/%s.txt", filename);
    
    long file_size;
    char* content = read_file_content(file_path, &file_size);
    if (!content) {
        *out_len = strlen("文件不存在");
        return strdup("文件不存在");
    }
    
    char* result = (char*)malloc(file_size + 1024);
    result[0] = '\0';
    char* line = strtok(content, "\n");
    
    while (line) {
        if (strstr(line, keyword)) {
            strcat(result, line);
            strcat(result, "\n");
        }
        line = strtok(NULL, "\n");
    }
    
    *out_len = strlen(result);
    free(content);
    return result;
}

// 处理HTTP请求
void handle_http_request(int client_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    if (getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len) == -1) {
        log_system(LOG_ERROR, "ConnectionHandler", "无法获取客户端地址");
        close(client_socket);
        return;
    }
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(client_addr.sin_port);

    log_system(LOG_INFO, "ConnectionHandler", "接受来自 %s:%d 的连接", client_ip, client_port);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    char request_buf[4096] = {0};
    ssize_t bytes_read = read(client_socket, request_buf, sizeof(request_buf) - 1);
    if (bytes_read <= 0) {
        log_system(LOG_WARNING, "RequestHandler", "读取请求失败 (客户端: %s:%d)", client_ip, client_port);
        close(client_socket);
        return;
    }

    log_system(LOG_DEBUG, "RequestHandler", "收到请求: %s", request_buf);

    char method[16] = {0}, path[512] = {0}, version[16] = {0};
    if (sscanf(request_buf, "%s %s %s", method, path, version) != 3) {
        log_access(client_ip, "INVALID", path, "HTTP/0.9", 400, 0);
        const char* content = "<html><body><h1>400 Bad Request</h1></body></html>";
        send_http_response(client_socket, 400, "Bad Request", "text/html", content, strlen(content));
        close(client_socket);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        // 处理搜索页面请求
        if (strcmp(path, "/pages/search.html") == 0) {
            char file_path[1024];
            snprintf(file_path, sizeof(file_path), "../blog/pages/search.html");
            
            long file_size;
            char* file_content = read_file_content(file_path, &file_size);
            
            if (!file_content) {
                const char* content = "<html><body><h1>404 Not Found</h1></body></html>";
                log_access(client_ip, method, path, version, 404, strlen(content));
                send_http_response(client_socket, 404, "Not Found", "text/html", content, strlen(content));
            } else {
                const char* mime_type = get_mime_type(file_path);
                log_access(client_ip, method, path, version, 200, file_size);
                send_http_response(client_socket, 200, "OK", mime_type, file_content, file_size);
                free(file_content);
            }
        }
        // 处理搜索功能请求（/search路由，用于AJAX和表单提交）
        else if ((strcmp(path, "/search") == 0) || (strcmp(path, "/search/") == 0)) {
    char key1[128] = {0}, key2[128] = {0};
    parse_get_params(path, key1, key2);
    
    long content_len;
    char* content;
    
    if (strlen(key1) > 0 && strlen(key2) > 0) {
        // 有参数时正常查询
        content = query_file(key1, key2, &content_len);
    } else {
        // 无参数时重定向到你的静态查询页面（关键修改）
        const char* redirect_html = "<html><head><meta http-equiv='refresh' content='0;url=/pages/search.html'></head></html>";
        content = strdup(redirect_html);
        content_len = strlen(content);
    }
    
    log_access(client_ip, method, path, version, 200, content_len);
    send_http_response(client_socket, 200, "OK", "text/html; charset=utf-8", content, content_len);
    free(content);
}
        // 处理其他普通文件请求（首页、博客文章等）
        else {
            char file_path[1024];
            if (strcmp(path, "/") == 0) {
                snprintf(file_path, sizeof(file_path), "../blog/index.html"); // 首页映射
            } else {
                snprintf(file_path, sizeof(file_path), "../blog%s", path); // 其他路径映射到../blog
            }

            long file_size;
            char* file_content = read_file_content(file_path, &file_size);

            if (!file_content) {
                const char* content = "<html><body><h1>404 Not Found</h1></body></html>";
                log_access(client_ip, method, path, version, 404, strlen(content));
                send_http_response(client_socket, 404, "Not Found", "text/html", content, strlen(content));
            } else {
                const char* mime_type = get_mime_type(file_path);
                log_access(client_ip, method, path, version, 200, file_size);
                send_http_response(client_socket, 200, "OK", mime_type, file_content, file_size);
                free(file_content);
            }
        }
    }
    // 处理POST请求
    else if (strcmp(method, "POST") == 0) {
        // 模糊匹配/search路径
        if (strstr(path, "/search") != NULL) { 
            // 解析Content-Length
            int content_length = 0;
            char* cl_header = strstr(request_buf, "Content-Length:");
            if (cl_header) {
                sscanf(cl_header, "Content-Length: %d", &content_length);
            }

            // 提取POST数据
            char* body = strstr(request_buf, "\r\n\r\n");
            if (body) {
                body += 4;
                char key1[128] = {0}, key2[128] = {0};
                parse_post_data(body, key1, key2);
                
                long content_len;
                char* content = query_file(key1, key2, &content_len);
                
                log_access(client_ip, method, path, version, 200, content_len);
                send_http_response(client_socket, 200, "OK", "text/plain; charset=utf-8", content, content_len);
                free(content);
            } else {
                const char* content = "无效的POST数据";
                log_access(client_ip, method, path, version, 400, strlen(content));
                send_http_response(client_socket, 400, "Bad Request", "text/plain", content, strlen(content));
            }
        } else {
            const char* content = "<html><body><h1>404 Not Found</h1></body></html>";
            log_access(client_ip, method, path, version, 404, strlen(content));
            send_http_response(client_socket, 404, "Not Found", "text/html", content, strlen(content));
        }
    }
    // 不支持的方法
    else {
        log_access(client_ip, method, path, version, 501, 0);
        const char* content = "<html><body><h1>501 Not Implemented</h1></body></html>";
        send_http_response(client_socket, 501, "Not Implemented", "text/html", content, strlen(content));
    }

    gettimeofday(&end_time, NULL);
    long response_time = (end_time.tv_sec - start_time.tv_sec) * 1000 + 
                         (end_time.tv_usec - start_time.tv_usec) / 1000;
    log_system(LOG_INFO, "ResponseHandler", "响应时间: %ld ms", response_time);

    close(client_socket);
}

// 发送HTTP响应
void send_http_response(int client_socket, int status_code, const char* status_text, 
                       const char* content_type, const char* content, long content_length) {
    char header[1024] = {0};
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "Server: MiniHTTP/1.0\r\n"
        "\r\n",
        status_code, status_text, content_type, content_length);

    if (write(client_socket, header, header_len) != header_len) {
        log_system(LOG_ERROR, "ResponseHandler", "发送响应头失败");
        return;
    }

    if (content && content_length > 0) {
        if (write(client_socket, content, content_length) != content_length) {
            log_system(LOG_ERROR, "ResponseHandler", "发送响应体失败");
        }
    }
}
