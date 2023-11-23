#ifndef THREADPOOL_H
#define THREADPOOL_H

/*
    线程池类：
*/

#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"

// 线程池模板类，T是任务类
template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool(int thread_number = 8, int max_requests = 10000); // 默认8个线程，10000个请求
    ~threadpool();
    bool append(T *request); // 向请求队列添加请求

private:
    static void *worker(void *arg); // 线程运行函数，静态
    void run();                     // 线程运行函数，动态

private:
    // 线程的数量
    int m_thread_number;

    // 线程数组，大小为m_thread_number
    pthread_t *m_threads;

    // 请求队列中最多等待请求数量
    int m_max_requests;

    // 请求队列
    std::list<T *> m_workqueue;

    // 保护请求队列的互斥锁
    locker m_queuelocker;

    // 任务处理信号量
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;
};

// 构造
template <typename T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests),
                                                                 m_stop(false), m_threads(NULL)
{

    if ((thread_number <= 0) || (max_requests <= 0)) // 线程数与最大请求数应>0
    {
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number]; // 创建线程数组
    if (!m_threads)                             // 失败抛异常
    {
        throw std::exception();
    }

    // 创建thread_number 个线程，并将他们设置为脱离线程（结束后自动释放资源）。
    for (int i = 0; i < thread_number; ++i)
    {
        printf("create the %dth thread\n", i);                      // 输出正在创建第几个线程
        if (pthread_create(m_threads + i, NULL, worker, this) != 0) // 创建线程，失败抛出异常
        {
            delete[] m_threads;
            throw std::exception();
        }

        if (pthread_detach(m_threads[i])) // 线程分离
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

// 析构，delete线程数组，设置线程停止
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

// 添加请求
template <typename T>
bool threadpool<T>::append(T *request)
{
    // 加锁
    m_queuelocker.lock();
    if (m_workqueue.size() > m_max_requests) // 请求数目已到上限
    {
        m_queuelocker.unlock(); // 释放锁
        return false;
    }
    m_workqueue.push_back(request); // 将新请求加入请求队列
    m_queuelocker.unlock();         // 释放锁
    m_queuestat.post();             // 待处理信号量+1
    return true;
}

// worker
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    threadpool *pool = (threadpool *)arg;
    pool->run(); // 调用对象的run函数
    return pool;
}

// run
template <typename T>
void threadpool<T>::run()
{

    while (!m_stop) // 线程停止
    {
        m_queuestat.wait();      // wait请求信号量
        m_queuelocker.lock();    // 加锁
        if (m_workqueue.empty()) // 判断队列为空
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front(); // 取出队列第一个请求
        m_workqueue.pop_front();          // 弹出第一个请求
        m_queuelocker.unlock();           // 解锁
        if (!request)                     // 指针判空
        {
            continue;
        }
        request->process(); // 处理请求
    }
}

#endif
