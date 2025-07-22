#ifndef HTTP_H
#define HTTP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>

// 处理HTTP请求（参数：客户端socket）
void handle_http_request(int client_socket);

// 发送HTTP响应
void send_http_response(int client_socket, int status_code, const char* status_text, 
                       const char* content_type, const char* content, long content_length);

// 获取文件MIME类型（辅助函数）
const char* get_mime_type(const char* file_path);

// 新增：URL解码函数
char* url_decode(const char* encoded);

#endif // HTTP_H
