//
// Created by jamsonzan on 2021/5/13.
//

#include <bits/stdc++.h>
#include <sys/time.h>
#include "event.h"
#include "driver_io.h"
#include "driver_signal.h"
#include "driver_timer.h"

namespace libevent {

Event* Event::SetDeadline(timeval *tv) {
    if (this->type_ != TIMER && this->timercb_ == nullptr) {
        this->deadline_ = *tv;
        this->timercb_ = [this](timeval* now) {
            this->is_deadline_ = true;
            this->now = *now;
        };
    }
    base_->timer_driver->Register(this);
    return this;
}

Event *Event::CancelDeadline() {
    base_->timer_driver->Deregister(this);
    return this;
}

Event *Event::EnableIO(int events) {
    if (this->type_ == IO) {
        int old_interest = this->interest;
        this->interest = events;
        base_->io_driver->Register(this, old_interest);
    }
    return this;
}

Event *Event::DisableIO() {
    if (this->type_ == IO) {
        base_->io_driver->Deregister(this);
    }
    return this;
}

Event* Event::EnableSignal() {
    if (this->type_ == SIGNAL) {
        base_->signal_driver->Register(this);
    }
    return this;
}

Event* Event::DisableSignal() {
    if (this->type_ == SIGNAL) {
        base_->signal_driver->Deregister(this);
    }
    return this;
}

Event::~Event() {
    CancelDeadline();
    DisableIO();
    DisableSignal();
}

EventBase::EventBase() : done(false) {
    io_driver = new IODriver(this);
    signal_driver = new SignalDriver(this);
    timer_driver = new TimerDriver(this);
}

void EventBase::Active(Event *e) {
    if (e->type_ == Event::USER) {
        ActiveInternal(e);
    }
}

void EventBase::ActiveInternal(Event* e) {
    assert(!(e->flags & Event::IN_ACTIVES));
    actives.push_back(e);
    e->flags |= Event::IN_ACTIVES;
}

void EventBase::ExecuteActives() {
    for (auto it = actives.begin(); it != actives.end(); ++it) {
        switch ((*it)->type_) {
            case Event::IO:
                (*it)->ExecuteIOCB();
                break;
            case Event::SIGNAL:
                (*it)->ExecuteSignalCB();
                break;
            case Event::TIMER:
                (*it)->ExecuteTimerCB();
                break;
            case Event::USER:
                (*it)->ExecuteUserCB();
                break;
            default:
                assert(0);
        }
        (*it)->flags &= ~Event::IN_ACTIVES;
    }
    actives.clear();
}

int EventBase::Dispatch() {
    if (done) {
        printf("EventBase had done\n");
        return -1;
    }

    timeval zero_time = {0, 0};
    timeval now = zero_time;
    timeval next_deadline = zero_time;

    while (!done) {
        timeval delta = zero_time;
        now = zero_time;
        next_deadline = zero_time;
        if (gettimeofday(&now, nullptr) < 0) {
            perror("gettimeofday");
            return -1;
        }

        int timeout_sec = -1;
        timer_driver->NextDeadline(next_deadline);
        if (next_deadline.tv_sec < 0) {
            // has not deadline event
            timeout_sec = -1;
        } else {
            timersub(&next_deadline, &now, &delta);
            if (delta.tv_sec <= 0) {
                // already access deadline
                timeout_sec = 0;
            } else {
                timeout_sec = delta.tv_sec;
            }
        }

        if (io_driver->Dispatch(timeout_sec) < 0) {
            perror("io_driver->Dispatch");
            return -1;
        }
        if (gettimeofday(&now, nullptr) < 0) {
            perror("gettimeofday");
            return -1;
        }
        timer_driver->Dispatch(&now);
        ExecuteActives();
    }
    return 0;
}

EventBase::~EventBase() {
    delete io_driver;
    delete signal_driver;
    delete timer_driver;
}

}
