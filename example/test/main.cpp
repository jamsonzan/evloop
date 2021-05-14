//
// Created by jamsonzan on 2021/5/13.
//

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/time.h>
#include "reactor.h"
#include "driver_io.h"
#include "driver_signal.h"
#include "driver_timer.h"
#include "event.h"

int tcp_server_init(int port, int listen_num);
void TestReactorStdin();
void TestTimerDriver();
void TestSignalDriver();

[[noreturn]] void TestReactorEcho() {
    struct client {
        int fd;
        int len;
        char buf[1024];
    };
    std::vector<client*> clients;
    libevent::epoll_reactor reactor;

    int listener = tcp_server_init(9002, 5);
    if (listener < 0) {
        perror("listen");
        exit(1);
    }
    reactor.add(listener, libevent::EV_READ);
    while (1) {
        reactor.dispatch(-1, [&](int fd, int what) {
            // accept
            if (fd == listener) {
                struct sockaddr_in caddr;
                socklen_t len = sizeof(caddr);
                int cfd = accept4(fd, (struct sockaddr *) &caddr, &len,
                        SOCK_CLOEXEC | SOCK_NONBLOCK);
                if (cfd < 0) {
                    perror("accept4");
                    return ;
                }
                if (clients.size() <= cfd) {
                    clients.resize(cfd+1, nullptr);
                }
                printf("fd %d accepted\n", cfd);
                assert(clients[cfd] == nullptr);
                clients[cfd] = new client();
                clients[cfd]->fd = cfd;
                reactor.add(cfd, libevent::EV_READ);
                return;
            }
            // read
            if (what & libevent::EV_READ) {
                client* c = clients[fd];
                assert(c != nullptr);
                int n = read(fd, c->buf, sizeof(c->buf));
                if (n <= 0) {
                    if (errno == EWOULDBLOCK) {
                        return;
                    } else if (n < 0) {
                        perror("read");
                    }
                    // go away
                    printf("fd %d go away\n", fd);
                    clients[fd] = nullptr;
                    delete c;
                    reactor.del(fd);
                    close(fd);
                    return;
                }
                c->len = n;
                reactor.mod(fd, libevent::EV_WRITE);
                return;
            }
            // write
            if (what & libevent::EV_WRITE) {
                client* c = clients[fd];
                assert(c != nullptr);
                int n = write(fd, c->buf, c->len);
                if (n <= 0) {
                    if (errno == EWOULDBLOCK) {
                        return;
                    } else if (n < 0) {
                        perror("write");
                    }
                    // go away
                    printf("fd %d go away\n", fd);
                    clients[fd] = nullptr;
                    delete c;
                    reactor.del(fd);
                    close(fd);
                    return;
                }
                c->len -= n;
                assert(c->len >= 0);
                if (c->len > 0) {
                    memcpy(c->buf, c->buf+n, c->len);
                } else if (c->len == 0) {
                    reactor.mod(fd, libevent::EV_READ);
                }
                return;
            }
        });
    }
    reactor.del(listener);
    close(listener);
}

libevent::EventBase* base = nullptr;

void TestEventBaseEcho() {
    base = new libevent::EventBase();
    int listener = tcp_server_init(9001, 5);
    if (listener < 0) {
        perror("listen");
        delete base;
        exit(1);
    }
    struct client {
        client(int cfd) : fd(cfd) {
            read_ev = new libevent::Event(base, fd, [this](int _, int what, bool deadline) {
                if (deadline) {
                    printf("fd %d read access deadline, goaway.\n", fd);
                    delete this;
                    return;
                }
                int n = read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    if (errno == EWOULDBLOCK) {
                        return;
                    } else if (n < 0) {
                        perror("read");
                    }
                    // go away
                    printf("fd %d go away\n", fd);
                    delete this;
                    return;
                }
                len = n;
                switch_to_write();
            });
            write_ev = new libevent::Event(base, fd, [this](int _, int what, bool deadline) {
                if (deadline) {
                    printf("fd %d write access deadline, goaway.\n", fd);
                    delete this;
                    return;
                }
                int n = write(fd, buf, len);
                if (n <= 0) {
                    if (errno == EWOULDBLOCK) {
                        return;
                    } else if (n < 0) {
                        perror("write");
                    }
                    // go away
                    printf("fd %d go away\n", fd);
                    delete this;
                    return;
                }
                len -= n;
                assert(len >= 0);
                if (len > 0) {
                    memcpy(buf, buf+n, len);
                } else if (len == 0) {
                    switch_to_read();
                }
            });
            switch_to_read();
        }
        void switch_to_read() {
            write_ev->DisableIO();
            read_ev->SetTimeout(10)->EnableIO(libevent::EV_READ);
        }
        void switch_to_write() {
            read_ev->DisableIO();
            write_ev->SetTimeout(10)->EnableIO(libevent::EV_WRITE);
        }
        ~client() {
            delete read_ev;
            delete write_ev;
            close(fd);
        }
        int fd;
        int len;
        char buf[1024];
        libevent::Event *read_ev;
        libevent::Event *write_ev;
    };

    auto listener_ev = new libevent::Event(base, listener, [](int fd, int what, bool deadline){
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        int cfd = accept4(fd, (struct sockaddr *) &caddr, &len,
                          SOCK_CLOEXEC | SOCK_NONBLOCK);
        if (cfd < 0) {
            perror("accept4");
            return ;
        }
        new client(cfd);
    });
    listener_ev->SetPriority(0)->EnableIO(libevent::EV_READ);

    libevent::Event* sig_ev2 = new libevent::Event(base, SIGINT, [&](int signum, bool deadline){
        printf("sig_ev2 get signum %d\n", signum);
        sig_ev2->DisableSignal();
        base->Shutdown();
    });
    if (base->Dispatch() < 0) {
        perror("base->Dispatch");
    }
    printf("base->Dispatch return.\n");
    close(listener);
    delete listener_ev;
    delete base;
}


int main() {
    timeval tv = {0, 0};
    gettimeofday(&tv, nullptr);
    printf("start test time sec %ld usec %ld\n", tv.tv_sec, tv.tv_usec);

    // TestReactorStdin();
    // TestReactorEcho();
     TestEventBaseEcho();
    // TestTimerDriver();
    // TestSignalDriver();
}

void TestSignalDriver() {
    base = new libevent::EventBase();

    int count = 0;
    libevent::Event* sig_ev1 = new libevent::Event(base, SIGINT, [&](int signum, bool deadline){
        sig_ev1->SetTimeout(5);
        if (deadline) {
            printf("sig_ev1 deadline once.\n");
            count++;
            if (count >= 5) {
                printf("cancel sig_ev1.\n");
                sig_ev1->DisableSignal();
                base->Shutdown();
            }
            return ;
        }
        printf("sig_ev1 get signum %d\n", signum);
    });
    sig_ev1->SetTimeout(5);

    libevent::Event* sig_ev2 = new libevent::Event(base, SIGINT, [&](int signum, bool deadline){
        printf("sig_ev2 get signum %d\n", signum);
        sig_ev2->CancelTimeout();
    });

    if (base->Dispatch() < 0) {
        perror("base->Dispatch");
    }
    printf("base->Dispatch return\n");

    delete sig_ev1;
    delete sig_ev2;
    delete base;
}

void TestTimerDriver() {
    base = new libevent::EventBase();
    timeval tv = {0, 0};
    gettimeofday(&tv, nullptr);

    libevent::Event* timer_ev0 = new libevent::Event(base, &tv, [&](timeval* now){
        printf("ev0 access deadline, now is %ld\n", now->tv_sec);
        timer_ev0->CancelDeadline();
    });

    tv.tv_sec += 10;
    libevent::Event* timer_ev1 = new libevent::Event(base, &tv, [&](timeval* now){
        printf("ev1 access deadline, now is %ld\n", now->tv_sec);
        timer_ev1->CancelDeadline();
    });
    libevent::Event* timer_ev2 = new libevent::Event(base, &tv, [&](timeval* now){
        printf("ev2 access deadline, now is %ld\n", now->tv_sec);
        timer_ev2->CancelDeadline();
    });

    int count = 0;
    libevent::Event* timer_ev3 = new libevent::Event(base, &tv, [&](timeval* now){
        printf("ev3 access deadline, now is %ld\n", now->tv_sec);
        count++;
        if (count < 5) {
            // has not canceled, will busy polling.
        } else {
            timer_ev3->CancelDeadline();
        }
    });
    tv.tv_sec += 10;
    timer_ev3->SetDeadline(&tv); // reset deadline

    libevent::Event* timer_ev4 = new libevent::Event(base, &tv, [](timeval* now){
        printf("ev4 access deadline, now is %ld\n", now->tv_sec);
    });
    timer_ev4->CancelDeadline(); // cancel

    libevent::Event* timer_ev5 = new libevent::Event(base, &tv, [](timeval* now){
        printf("ev5 access deadline, now is %ld\n", now->tv_sec);
    });
    delete timer_ev5;

    if (base->Dispatch() < 0) {
        perror("base->Dispatch");
    }

    delete timer_ev1;
    delete timer_ev2;
    delete timer_ev3;
    delete timer_ev4;
    delete base;
}

void TestReactorStdin() {
    libevent::epoll_reactor reactor;
    assert(reactor.add(STDIN_FILENO, libevent::EV_READ) >= 0);
    int res = reactor.dispatch(-1, [&](int fd, int what){
        assert(fd == STDIN_FILENO);
        assert(what & libevent::EV_READ);
        assert(reactor.mod(STDIN_FILENO, libevent::EV_WRITE) >= 0);
        assert(reactor.del(STDIN_FILENO) >= 0);
        printf("done\n");
    });
    assert(res >= 0);
}

typedef struct sockaddr SA;
int tcp_server_init(int port, int listen_num) {
    int errno_save;
    int listener;
    listener = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC |
                               SOCK_NONBLOCK, 0);
    if( listener == -1 )
        return -1;
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    if( bind(listener, (SA*)&sin, sizeof(sin)) < 0 ) goto error;
    if( listen(listener, listen_num) < 0 ) goto error;
    return listener;
    error:
    errno_save = errno;
    close(listener);
    errno = errno_save;
    return -1;
}
