/*
    主函数
*/
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "locker.h"
#include "threadpool.h"
#include "http_conn.h"

#define MAX_FD 65536           // 最大的文件描述符个数
#define MAX_EVENT_NUMBER 10000 // 监听的最大的事件数量

// epoll中添加、移除文件描述符
extern void addfd(int epollfd, int fd, bool one_shot);
extern void removefd(int epollfd, int fd);

void addsig(int sig, void(handler)(int)) // 添加信号捕捉
{
    struct sigaction sa;           // 注册信号
    memset(&sa, '\0', sizeof(sa)); // 清空
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);                 // 临时阻塞信号集合，全部阻塞
    assert(sigaction(sig, &sa, NULL) != -1); // 注册信号
}

int main(int argc, char *argv[])
{
    // 运行程序应指明端口号，如没有则提示
    if (argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));
        return 1;
    }

    int port = atoi(argv[1]); // 端口号
    addsig(SIGPIPE, SIG_IGN); // 处理sigpipe信号为忽略，对方断开连接时不终止程序

    threadpool<http_conn> *pool = NULL; // 创建线程池，任务类为http_conn
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch (...)
    {
        return 1;
    }

    // 创建数组保存所有客户端信息
    http_conn *users = new http_conn[MAX_FD];

    // 创建监听tcpsocket
    int listenfd = socket(PF_INET, SOCK_STREAM, 0);

    // 绑定参数
    int ret = 0;
    struct sockaddr_in address;
    address.sin_addr.s_addr = INADDR_ANY; // 任何地址
    address.sin_family = AF_INET;
    address.sin_port = htons(port); // 端口

    // 端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定地址
    ret = bind(listenfd, (struct sockaddr *)&address, sizeof(address));

    // 监听，等待最大为5
    ret = listen(listenfd, 5);

    // 创建事件数组，添加监听文件描述符
    epoll_event events[MAX_EVENT_NUMBER];

    // 创建epoll对象
    int epollfd = epoll_create(5);

    // 将监听文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);
    http_conn::m_epollfd = epollfd; // 设置http的epoll

    while (true) // 死循环
    {

        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // 检测到几个事件

        if ((number < 0) && (errno != EINTR)) // 调用失败且不是被信号中断
        {
            printf("epoll failure\n");
            break;
        }

        for (int i = 0; i < number; i++) // 遍历事件数组
        {

            int sockfd = events[i].data.fd; // 文件描述符

            if (sockfd == listenfd) // 有新客户端连接
            {

                struct sockaddr_in client_address; // 新连接的socket
                socklen_t client_addrlength = sizeof(client_address);
                int connfd = accept(listenfd, (struct sockaddr *)&client_address, &client_addrlength); // 接收连接

                if (connfd < 0) // 连接失败
                {
                    printf("errno is: %d\n", errno);
                    continue;
                }

                if (http_conn::m_user_count >= MAX_FD) // 当前最大客户端数已满
                {
                    close(connfd); // 关闭连接
                    continue;
                }
                users[connfd].init(connfd, client_address); // 将新客户放入用户数组
            }
            else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) // 对方异常断开
            {

                users[sockfd].close_conn(); // 关闭连接
            }
            else if (events[i].events & EPOLLIN) // 读事件发生
            {

                if (users[sockfd].read()) // 一次性读出数据
                {
                    pool->append(users + sockfd); // 加入请求队列
                }
                else
                {
                    users[sockfd].close_conn(); // 关闭连接
                }
            }
            else if (events[i].events & EPOLLOUT) // 写事件
            {

                if (!users[sockfd].write()) // 一次性写
                {
                    users[sockfd].close_conn(); // 关闭连接
                }
            }
        }
    }

    // 关闭、回收
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}