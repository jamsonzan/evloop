//
// Created by jamsonzan on 2021/5/8.
//

#ifndef LIBEVENT2_EVENT_H
#define LIBEVENT2_EVENT_H

#include <cstdio>
#include <utility>
#include <list>

namespace libevent {

// for IO
enum {
    EV_READ = 0x01,
    EV_WRITE = 0x02,
    EV_CLOSED = 0x04,
};

class EventBase;
class IODriver;
class SignalDriver;
class TimerDriver;

typedef std::function<void (int fd, int events, bool deadline)> IOCallback;
typedef std::function<void (int signum, bool deadline)> SignalCallback;
typedef std::function<void (timeval* now)> TimerCallback;
typedef std::function<void ()> UserCallback;

class Event {
public:
    // IO Event Construct
    Event(EventBase* base, int fd, IOCallback cb) : priority_(5),
        base_(base), type_(IO), iocb_(std::move(cb)), fd_(fd), interest(0), flags(INIT) {};
    // SIGNAL Event Construct
    Event(EventBase* base, int signum, SignalCallback cb) : priority_(5),
            base_(base), type_(SIGNAL), signalcb_(std::move(cb)), signum_(signum),flags(INIT) {
        EnableSignal();
    };
    // TIMER Event Construct
    Event(EventBase* base, timeval *deadline, TimerCallback cb) : priority_(5),
            base_(base), type_(TIMER), timercb_(std::move(cb)), deadline_(*deadline), flags(INIT) {
        SetDeadline(deadline);
    };
    // USER Event Construct
    Event(EventBase* base, UserCallback cb) : priority_(5),
            base_(base), type_(USER), usercb_(std::move(cb)) {}

    Event(const Event&) = delete;
    Event& operator=(const Event&) = delete;

    ~Event();

    // follow used for IO, SIGNAL, TIMER, USER
    Event* SetPriority(int priority) {
        if (priority < 0) {
            priority = 0;
        } else if (priority >= 10) {
            priority = 9;
        }
        priority_ = priority;
        return this;
    }
    Event* SetDeadline(timeval* tv);
    Event* SetTimeout(int sec);
    Event* CancelDeadline();
    Event* CancelTimeout();

    // follow only used for IO
    Event* EnableIO(int events);
    Event* DisableIO();

    // follow only used for signal
    Event* EnableSignal();
    Event* DisableSignal();

private:
    friend EventBase;
    friend IODriver;
    friend SignalDriver;
    friend TimerDriver;

    EventBase* base_;
    enum Type { IO, SIGNAL, TIMER, USER };
    enum { INIT = 0x01, IO_WAIT = 0x02, TIMER_WAIT = 0x04, SIGNAL_WAIT = 0x08, IN_ACTIVES = 0x10};
    const Type type_;
    int flags;
    int priority_;

    int fd_;
    int interest;
    int what;      // events happened
    IOCallback iocb_;

    int signum_;
    int ncaught_;
    SignalCallback signalcb_;

    timeval deadline_;
    bool is_deadline_;
    timeval now;   // now time while is_deadline_ == true
    TimerCallback timercb_;

    UserCallback usercb_;

    void ExecuteIOCB() {
        iocb_(fd_, what, is_deadline_);
        is_deadline_ = false;
    }
    void ExecuteSignalCB() {
        if (is_deadline_) {
            signalcb_(signum_, is_deadline_);
        } else {
            while (ncaught_-- > 0) {
                signalcb_(signum_, is_deadline_);
            }
        }
        is_deadline_ = false;
    }
    void ExecuteTimerCB() {
        timercb_(&now);
    }
    void ExecuteUserCB() { usercb_(); }

    // EventBase::actives list
    std::list<Event*>::iterator it;
};


class EventBase {
public:
    EventBase();
    ~EventBase();
    EventBase(const EventBase&) = delete;
    EventBase& operator=(const EventBase&) = delete;
    void Active(Event* e);
    int Dispatch();
    void Shutdown() { done = true; }

private:
    friend class Event;
    friend IODriver;
    friend SignalDriver;
    friend TimerDriver;
    void ActiveInternal(Event* e);
    void ExecuteActives();
    void RemoveActive(Event* e);
    bool done;
    IODriver* io_driver;
    SignalDriver* signal_driver;
    TimerDriver* timer_driver;
    std::list<Event*> actives[10];
};

}
#endif //LIBEVENT2_EVENT_H
