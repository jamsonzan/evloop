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
        this->timercb_ = [this](timeval* now) {
            this->is_deadline_ = true;
            this->now = *now;
            this->base_->ActiveInternal(this);
        };
    }
    this->deadline_ = *tv;
    base_->timer_driver->Register(this);
    return this;
}

Event *Event::CancelDeadline() {
    base_->timer_driver->Deregister(this);
    this->timercb_ = nullptr;
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
    base_->RemoveActive(this);
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
    if (e->flags & Event::IN_ACTIVES) {
        return;
    }
    actives[e->priority_].push_front(e);
    e->it = actives[e->priority_].begin();
    e->flags |= Event::IN_ACTIVES;
}

void EventBase::RemoveActive(Event* e) {
    if (e->flags & Event::IN_ACTIVES) {
        actives[e->priority_].erase(e->it);
        e->it = actives[e->priority_].end();
        e->flags &= ~Event::IN_ACTIVES;
    }
}

void EventBase::ExecuteActives() {
    for (int i = 0; i < 10; ++i) {
        auto actives_i = &actives[i];
        for (auto it = actives_i->begin(); it != actives_i->end();) {
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
            RemoveActive((*it));
            it = actives_i->begin();
        }
        assert(actives_i->empty());
    }
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

        int timeout_msec = -1;
        timer_driver->NextDeadline(next_deadline);
        if (next_deadline.tv_sec < 0) {
            // has not deadline event
            timeout_msec = -1;
        } else {
            timersub(&next_deadline, &now, &delta);
            if (delta.tv_sec <= 0) {
                // already access deadline
                timeout_msec = 0;
            } else {
                // round up 3ms, avoid busy polling.
                timeout_msec = delta.tv_sec*1000 + delta.tv_usec/1000 + 3;
            }
        }

        if (io_driver->Dispatch(timeout_msec) < 0) {
            perror("io_driver->Dispatch");
            return -1;
        }
        if (gettimeofday(&now, nullptr) < 0) {
            perror("gettimeofday");
            return -1;
        }
        printf("Dispatch onece\n");
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
