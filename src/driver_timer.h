//
// Created by jamsonzan on 2021/5/13.
//

#ifndef LIBEVENT2_DRIVER_TIMER_H
#define LIBEVENT2_DRIVER_TIMER_H

#include <bits/stdc++.h>
#include "event.h"

namespace libevent {

class TimerDriver {
public:
    TimerDriver(EventBase* base) : base_(base) {}
    ~TimerDriver() {}

    void Register(Event* e) {
        Deregister(e);
        ordered_set.insert(e);
        e->flags |= Event::TIMER_WAIT;
    }

    void Deregister(Event* e) {
        if (e->flags & Event::TIMER_WAIT) {
            auto s = ordered_set.erase(e);
            e->flags &= ~Event::TIMER_WAIT;
        }
    }

    void NextDeadline(timeval& tv) {
        auto it = ordered_set.begin();
        if (it != ordered_set.end()) {
            tv.tv_sec = (*it)->deadline_.tv_sec;
            tv.tv_usec = (*it)->deadline_.tv_usec;
        } else {
            tv.tv_sec = -1;
            tv.tv_usec =-1;
        }
    }

    void Dispatch(timeval* now) {
        for (auto it = ordered_set.begin(); it != ordered_set.end(); ++it) {
            if (compare_time(now, &(*it)->deadline_) >= 0) {
                (*it)->is_deadline_ = true;
                (*it)->now = *now;
                if ((*it)->type_ == Event::TIMER) {
                    base_->ActiveInternal((*it));
                } else {
                    // 内部注册的事件需要立即执行
                    (*it)->ExecuteTimerCB();
                }
            }
        }
    }

private:
    EventBase* base_;

    static int compare_time(const timeval* tv1, const timeval* tv2) {
        if (tv1->tv_sec > tv2->tv_sec) {
            return 1;
        }
        if (tv1->tv_sec < tv2->tv_sec) {
            return -1;
        }
        if (tv1->tv_usec > tv2->tv_usec) {
            return 1;
        }
        if (tv1->tv_usec < tv2->tv_usec) {
            return -1;
        }
        return 0;
    }

    struct Compare {
        bool operator()(const Event* lhs, const Event* rhs) {
            if (lhs == rhs) {
                return false;
            }
            int res = compare_time(&lhs->deadline_, &rhs->deadline_);
            if (res == 0) {
                return lhs < rhs;
            }
            return res < 0;
        }
    };
    std::set<Event*, Compare> ordered_set;
};

}

#endif //LIBEVENT2_DRIVER_TIMER_H
