//
// Created by jamsonzan on 2021/5/8.
//

#ifndef LIBEVENT2_REACTOR_H
#define LIBEVENT2_REACTOR_H

#include <unistd.h>
#include <exception>
#include <sys/epoll.h>
#include <sys/timerfd.h>

namespace libevent {

class epoll_reactor {
public:
    epoll_reactor() :epoll_fd_(-1) {
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ < 0) {
            throw "epoll_create1";
        }
    }
    epoll_reactor(const epoll_reactor&) = delete;
    epoll_reactor& operator=(const epoll_reactor&) = delete;

    int add(int fd, int events);
    int mod(int fd, int events);
    int del(int fd);
    int dispatch(int timeout, void (*handle)(void *arg, int fd, int events), void *arg);

    ~epoll_reactor() {
        if (epoll_fd_ >= 0) {
            close(epoll_fd_);
            epoll_fd_ = -1;
        }
    }

private:
    static const int max_events = 1024;
    static const int max_epoll_timeout_msec = 35 * 60 * 1000;
    struct epoll_event events_[max_events];
    int epoll_fd_;

};

}

#endif //LIBEVENT2_REACTOR_H
