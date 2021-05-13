//
// Created by jamsonzan on 2021/5/13.
//

#ifndef LIBEVENT2_DRIVER_SIGNAL_H
#define LIBEVENT2_DRIVER_SIGNAL_H

#include <bits/stdc++.h>
#include <signal.h>
#include <fcntl.h>
#include "event.h"
#include "driver_io.h"

namespace libevent {

class SignalDriver {
public:
    SignalDriver(EventBase* base) : base_(base) {
        if (pipe2(pipes, O_NONBLOCK | O_CLOEXEC) < 0) {
            throw "pipe";
        }
        sig_handler_fd = pipes[1];
        pipe_event = new Event(base_, pipes[0], [this](int fd, int events, bool deadline){
            int i;
            int ncaught[NSIG];
            static char signals[1024];
            memset(&ncaught, 0, sizeof(ncaught));
            while (1) {
                int n = read(fd, signals, sizeof(signals));
                if (n <= 0) {
                    if (errno != EWOULDBLOCK) {
                        perror("read pipe[0]");
                    }
                    break;
                }
                for (i = 0; i < n; ++i) {
                    int sig = signals[i];
                    if (sig < NSIG)
                        ncaught[sig]++;
                }
            }
            for (i = 0; i < NSIG; ++i) {
                if (ncaught[i]) {
                    this->Dispatch(i, ncaught[i]);
                }
            }
        });
        pipe_event->SetPriority(0)->EnableIO(EV_READ)->SetPriority(0);
    }

    ~SignalDriver() {
        delete pipe_event;
        for (int i = 0; i < 2; ++i) {
            close(pipes[i]);
        }
        for (int i = 0; i < NSIG; ++i) {
            if (!signal_events[i].empty()) {
                for (auto it = signal_events[i].begin(); it != signal_events[i].end(); ++it) {
                    Deregister((*it));
                }
            }
        }
    };

    void Register(Event* e) {
        if (e->flags & Event::SIGNAL_WAIT) {
            return;
        }
        int signum = e->signum_;
        std::vector<Event*>& events = signal_events[signum];
        if (events.empty()) {
            SetSigaction(signum);
        }
        events.push_back(e);
        e->flags |= Event::SIGNAL_WAIT;
    }

    void Deregister(Event* e) {
        if (!(e->flags & Event::SIGNAL_WAIT)) {
            return;
        }
        int signum = e->signum_;
        std::vector<Event*>& events = signal_events[signum];
        for (auto it = events.begin(); it != events.end(); ++it) {
            if ((*it) == e) {
                events.erase(it);
                (*it)->flags &= ~Event::SIGNAL_WAIT;
                break;
            }
        }
        if (events.empty()) {
            ResetSigaction(signum);
        }
    }

    void Dispatch(int signum, int ncaught) {
        std::vector<Event*>& events = signal_events[signum];
        if (!events.empty()) {
            for (auto it = events.begin(); it != events.end(); ++it) {
                (*it)->signum_ = signum;
                (*it)->ncaught_ = ncaught;
                if ((*it)->type_ == Event::SIGNAL) {
                    base_->ActiveInternal((*it));
                } else {
                    // 内部注册的事件需要立即执行
                    (*it)->ExecuteSignalCB();
                }
            }
        }
    }

private:
    EventBase* base_;

    Event *pipe_event;
    int pipes[2];
    std::vector<Event*> signal_events[NSIG];

    static int sig_handler_fd;
    static void sig_handler(int signum) {
        uint8_t msg = signum;
        if (write(sig_handler_fd, (char*)&msg, 1) <= 0) {
            perror("write sig_handler_fd");
        }
    }

    void SetSigaction(int signum) {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig_handler;
        sa.sa_flags |= SA_RESTART;
        sigfillset(&sa.sa_mask);
        if (sigaction(signum, &sa, &olds[signum]) < 0) {
            perror("SetSigaction");
        }
    }

    void ResetSigaction(int signum) {
        if (sigaction(signum, &olds[signum], nullptr) < 0) {
            perror("ResetSigaction");
        }
    }

    struct sigaction olds[NSIG];
};

}

#endif //LIBEVENT2_DRIVER_SIGNAL_H
