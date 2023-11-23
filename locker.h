#ifndef LOCKER_H
#define LOCKER_H

/*
    线程同步机制类：
    互斥锁类locker
    条件变量类cond
    信号量类sem
*/
#include <exception>
#include <pthread.h>
#include <semaphore.h>

// 互斥锁类
class locker
{
public:
    locker() // 构造
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0) // 初始化互斥锁，失败抛出异常
        {
            throw std::exception();
        }
    }

    ~locker() // 析构
    {
        pthread_mutex_destroy(&m_mutex); // 释放互斥锁
    }

    bool lock() // 上锁
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }

    bool unlock() // 释放锁
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }

    pthread_mutex_t *get() // 获取锁对象
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex; // 互斥锁
};

// 条件变量类
class cond
{
public:
    cond() // 构造
    {
        if (pthread_cond_init(&m_cond, NULL) != 0) // 初始化，失败抛出异常
        {
            throw std::exception();
        }
    }
    ~cond() // 析构
    {
        pthread_cond_destroy(&m_cond);
    }

    bool wait(pthread_mutex_t *m_mutex) // wait，传递互斥锁对象
    {
        int ret = 0;
        ret = pthread_cond_wait(&m_cond, m_mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *m_mutex, struct timespec t) // 超时等待，传入等待时间
    {
        int ret = 0;
        ret = pthread_cond_timedwait(&m_cond, m_mutex, &t);
        return ret == 0;
    }
    bool signal() // signal
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    bool broadcast() // 唤醒所有线程
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond; // 条件变量
};

// 信号量类
class sem
{
public:
    sem() // 构造
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    sem(int num) // 构造，初始化为num值
    {
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~sem() // 析构
    {
        sem_destroy(&m_sem);
    }
    // 等待信号量
    bool wait() // wait
    {
        return sem_wait(&m_sem) == 0;
    }
    // 增加信号量
    bool post() // post
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem; // 信号量
};

#endif