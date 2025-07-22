#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <pthread.h>
#include <sys/wait.h>
#include <errno.h>
#include "http.h"
#include "logger.h"

#define PORT 9001
#define BACKLOG 10  // 最大等待连接数
#define MAX_THREADS 10  // 最大工作线程数

// 线程池结构体
typedef struct {
    int client_socket;
    struct sockaddr_in client_addr;
} ThreadArgs;

// 线程处理函数
void* handle_client(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    int client_socket = args->client_socket;
    
    // 获取客户端IP和端口
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &args->client_addr.sin_addr, client_ip, sizeof(client_ip));
    int client_port = ntohs(args->client_addr.sin_port);
    
    log_system(LOG_DEBUG, "ThreadPool", "线程 %ld 开始处理 %s:%d", 
               pthread_self(), client_ip, client_port);
    
    // 处理HTTP请求
    handle_http_request(client_socket);
    
    log_system(LOG_DEBUG, "ThreadPool", "线程 %ld 完成处理 %s:%d", 
               pthread_self(), client_ip, client_port);
    
    free(args);  // 释放参数内存
    return NULL;
}

// 清理僵尸进程（信号处理）
static void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main() {
    // 初始化日志系统
    logger_init();
    log_system(LOG_INFO, "Main", "服务器启动中...");
    
    // 创建服务器socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        log_system(LOG_ERROR, "Main", "创建socket失败: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        log_system(LOG_ERROR, "Main", "设置socket选项失败: %s", strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 绑定端口
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PORT)
    };
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        log_system(LOG_ERROR, "Main", "绑定端口 %d 失败: %s", PORT, strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 开始监听
    if (listen(server_fd, BACKLOG) == -1) {
        log_system(LOG_ERROR, "Main", "监听端口 %d 失败: %s", PORT, strerror(errno));
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    log_system(LOG_INFO, "Main", "服务器启动成功，监听端口 %d...", PORT);
    log_system(LOG_INFO, "Main", "博客根目录: %s", "../blog");
    
    // 注册信号处理（清理僵尸进程）
    signal(SIGCHLD, handle_sigchld);
    
    // 主循环：接受并处理连接
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == -1) {
            log_system(LOG_ERROR, "Main", "接受连接失败: %s", strerror(errno));
            continue;
        }
        
        // 获取客户端IP和端口
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        int client_port = ntohs(client_addr.sin_port);
        
        log_system(LOG_INFO, "Main", "接受新连接: %s:%d", client_ip, client_port);
        
        // 创建线程处理请求
        pthread_t thread_id;
        ThreadArgs* args = (ThreadArgs*)malloc(sizeof(ThreadArgs));
        if (!args) {
            log_system(LOG_ERROR, "Main", "内存分配失败，拒绝连接: %s:%d", client_ip, client_port);
            close(client_socket);
            continue;
        }
        
        args->client_socket = client_socket;
        args->client_addr = client_addr;
        
        // 创建线程
        if (pthread_create(&thread_id, NULL, handle_client, (void*)args) != 0) {
            log_system(LOG_ERROR, "Main", "创建线程失败，拒绝连接: %s:%d", client_ip, client_port);
            close(client_socket);
            free(args);
            continue;
        }
        
        // 分离线程，自动回收资源
        pthread_detach(thread_id);
    }
    
    close(server_fd);
    return 0;
}
