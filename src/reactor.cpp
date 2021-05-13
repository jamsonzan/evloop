//
// Created by jamsonzan on 2021/5/8.
//

#include <cerrno>
#include <functional>
#include "event.h"
#include "reactor.h"

int libevent::epoll_reactor::add(int fd, int events) {
    struct epoll_event event = {0, 0};
    event.data.fd = fd;
    if (events & EV_READ) event.events |= EPOLLIN;
    if (events & EV_WRITE) event.events |= EPOLLOUT;
    if (events & EV_CLOSED) event.events |= EPOLLRDHUP;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) < 0) return -1;
    return 0;
}

int libevent::epoll_reactor::mod(int fd, int events) {
    struct epoll_event event = {0, 0};
    event.data.fd = fd;
    if (events & EV_READ) event.events |= EPOLLIN;
    if (events & EV_WRITE) event.events |= EPOLLOUT;
    if (events & EV_CLOSED) event.events |= EPOLLRDHUP;
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) < 0) return -1;
    return 0;
}

int libevent::epoll_reactor::del(int fd) {
    struct epoll_event event = {0, 0};
    if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &event) < 0) return -1;
    return 0;
}

int libevent::epoll_reactor::dispatch(int timeout_msec, const std::function<void (int fd, int events)>& handle) {
    if (timeout_msec > max_epoll_timeout_msec) {
        timeout_msec = max_epoll_timeout_msec;
    }

    // res==0可能是超时
    int res = epoll_wait(epoll_fd_, &events_[0], max_events, timeout_msec);
    if (res < 0) {
        if (errno != EINTR) {
            return -1;
        }
        return 0;
    }

    for (int i = 0; i < res; ++i) {
        struct epoll_event *event = &events_[i];
        int fd = event->data.fd;
        int what = event->events;
        int events = 0;

        if (what & EPOLLERR) {
            events = EV_READ | EV_WRITE;
        } else if ((what & EPOLLHUP) && !(what & EPOLLRDHUP)) {
            events = EV_READ | EV_WRITE;
        } else {
            if (what & EPOLLIN) {
                events |= EV_READ;
            }
            if (what & EPOLLOUT) {
                events |= EV_WRITE;
            }
            if (what & EPOLLRDHUP) {
                events |= EV_CLOSED;
            }
        }

        if (!events) continue;

        handle(fd, events);
    }

    return 0;
}
