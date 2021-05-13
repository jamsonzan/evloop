//
// Created by jamsonzan on 2021/5/13.
//

#ifndef LIBEVENT2_DRIVER_IO_H
#define LIBEVENT2_DRIVER_IO_H

#include <bits/stdc++.h>
#include "reactor.h"
#include "event.h"

namespace libevent {

class IODriver {
public:
    IODriver(EventBase* base) :base_(base) {}
    ~IODriver() {}

    void Register(Event* e, int old_interest) {
        if (e->interest == 0) {
            Deregister(e);
            return;
        }

        int fd = e->fd_;
        if (ctxs.size() <= fd) {
            ctxs.resize(fd+1, nullptr);
        }

        ctx* c = ctxs[fd];
        if (c == nullptr) {
            c = new ctx(fd);
            ctxs[fd] = c;
        }
        assert(c->fd == fd);

        int interest = e->interest;
        if (interest & EV_READ) {
            c->nread++;
        }
        if (interest & EV_WRITE) {
            c->nwrite++;
        }
        if (interest & EV_CLOSED) {
            c->nclose++;
        }

        if (old_interest & EV_READ) {
            c->nread--;
        }
        if (old_interest & EV_WRITE) {
            c->nwrite--;
        }
        if (old_interest & EV_CLOSED) {
            c->nclose--;
        }

        int new_events = 0;
        if (c->nread > 0) {
            new_events |= EV_READ;
        }
        if (c->nwrite > 0) {
            new_events |= EV_WRITE;
        }
        if (c->nclose > 0) {
            new_events |= EV_CLOSED;
        }
        c->new_events = new_events;

        if (c->new_events != c->old_events && c->change_idx < 0) {
            change_list.push_back(c);
            c->change_idx = change_list.size() - 1;
        }

        if (!(e->flags & Event::IO_WAIT)) {
            c->events_list.push_back(e);
            e->flags |= Event::IO_WAIT;
        }
    }

    void Deregister(Event* e) {
        if (!(e->flags & Event::IO_WAIT)) {
            return;
        }
        int fd = e->fd_;
        int interest = e->interest;
        ctx* c = ctxs[fd];
        assert(c != nullptr);

        if (interest & EV_READ) {
            c->nread--;
        }
        if (interest & EV_WRITE) {
            c->nwrite--;
        }
        if (interest & EV_CLOSED) {
            c->nclose--;
        }

        int new_events = 0;
        if (c->nread > 0) {
            new_events |= EV_READ;
        }
        if (c->nwrite > 0) {
            new_events |= EV_WRITE;
        }
        if (c->nclose > 0) {
            new_events |= EV_CLOSED;
        }
        c->new_events = new_events;

        for (auto it = c->events_list.begin(); it != c->events_list.end(); ++it) {
            if ((*it) == e) {
                c->events_list.erase(it);
                e->flags &= ~Event::IO_WAIT;
                break;
            }
        }
    }

    int Dispatch(int timeout_sec) {
        if (ApplyChangeList() < 0) {
            return -1;
        }
        return reactor_.dispatch(timeout_sec, [&](int fd, int what){
            ctx* c = ctxs[fd];
            assert(c != nullptr);
            for (auto it = c->events_list.begin(); it != c->events_list.end(); ++it) {
                (*it)->what = (*it)->interest & what;
                if ((*it)->what) {
                    if ((*it)->type_ == Event::IO) {
                        base_->ActiveInternal((*it));
                    } else {
                        // 内部注册的事件需要立即执行
                        (*it)->ExecuteIOCB();
                    }
                }
            }
        });
    }

private:
    struct ctx {
        ctx(int cfd):nread(0), nwrite(0), nclose(0), fd(cfd),
            change_idx(-1), old_events(0), new_events(0){}

        std::list<Event*> events_list;
        int nread;
        int nwrite;
        int nclose;

        int change_idx;
        int fd;
        int old_events;
        int new_events;
    };

    int ApplyChangeList() {
        for (int i = 0; i < change_list.size(); ++i) {
            ctx* c = change_list[i];
            if (c->old_events == 0 && c->new_events != 0) {
                int res = reactor_.add(c->fd, c->new_events);
                if (res < 0) {
                    return res;
                }
            }
            if (c->old_events != 0 && c->new_events != 0 && c->old_events != c->new_events) {
                int res = reactor_.mod(c->fd, c->new_events);
                if (res < 0) {
                    return res;
                }
            }
            if (c->old_events != 0 && c->new_events == 0) {
                int res = reactor_.del(c->fd);
                if (res < 0) {
                    return res;
                }
            }
            c->change_idx = -1;
            c->old_events = c->new_events;
            if (c->events_list.empty()) {
                assert(c->new_events == 0);
                ctxs[c->fd] = nullptr;
                delete c;
            }
        }
        change_list.clear();
        return 1;
    }

    EventBase* base_;
    epoll_reactor reactor_;
    std::vector<ctx*> ctxs;
    std::vector<ctx*> change_list;
};

}

#endif //LIBEVENT2_DRIVER_IO_H
