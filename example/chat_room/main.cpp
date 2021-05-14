#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "event.h"

int tcp_server_init(int port, int listen_num);
libevent::EventBase* base = nullptr;

// chat room example
struct client;
std::set<client*> global_room;
struct client {
    client(int cfd) : fd(cfd), rlen(0), wlen(0), w_enable(false) {
        // recv msg from user
        read_ev = new libevent::Event(base, fd, [this](int _, int what, bool deadline) {
            if (deadline) {
                printf("fd %d read access deadline, goaway.\n", fd);
                delete this;
                return;
            }
            int n = read(fd, rbuf, sizeof(rbuf));
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
            rlen = n;
            send_msg_to_room();
            read_ev->SetTimeout(30);
        });
        // send msg to user
        write_ev = new libevent::Event(base, fd, [this](int _, int what, bool deadline) {
            if (deadline) {
                printf("fd %d write access deadline, goaway.\n", fd);
                delete this;
                return;
            }
            int n = write(fd, wbuf, wlen);
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
            wlen -= n;
            assert(wlen >= 0);
            if (wlen > 0) {
                memcpy(wbuf, wbuf+n, wlen);
                write_ev->SetTimeout(30);
            } else {
                disable_write();
            }
        });
        global_room.insert(this);
        read_ev->SetTimeout(30)->EnableIO(libevent::EV_READ);
    }

    void send_msg_to_room() {
        for (auto c : global_room) {
            if (c != this) {
                int send = rlen;
                int remain = sizeof(c->wbuf) - c->wlen;
                if (send > remain) {
                    send = remain;
                }
                printf("send %d bytes\n", send);
                memcpy(c->wbuf, rbuf, send);
                c->wlen += send;
                c->enable_write();
            }
        }
        rlen = 0;
    }

    void disable_write() {
        write_ev->DisableIO();
        w_enable = false;
    }
    void enable_write() {
        if (!w_enable) {
            write_ev->SetTimeout(30)->EnableIO(libevent::EV_WRITE);
            w_enable = true;
        }
    }
    ~client() {
        global_room.erase(this);
        delete read_ev;
        delete write_ev;
        close(fd);
    }
    int     fd;
    int     rlen;
    char    rbuf[1024];
    int     wlen;
    char    wbuf[1024];
    bool    w_enable;
    libevent::Event *read_ev;
    libevent::Event *write_ev;
};

int main()
{
    // init base
    base = new libevent::EventBase();
    int listener = tcp_server_init(9002, 5);
    if (listener < 0) {
        perror("listen");
        delete base;
        exit(1);
    }

    // accept event
    struct sockaddr_in caddr;
    socklen_t len = sizeof(caddr);
    auto listener_ev = new libevent::Event(base, listener, [&](int fd, int what, bool deadline){
        int cfd = accept4(fd, (struct sockaddr*)&caddr, &len,SOCK_CLOEXEC|SOCK_NONBLOCK);
        if (cfd >= 0) {
            auto c = new client(cfd);
        } else {
            perror("accept4");
        }
    });
    listener_ev->SetPriority(0)->EnableIO(libevent::EV_READ);

    // wait ctrl c signal to shutdown
    libevent::Event* sig_ev = new libevent::Event(base, SIGINT, [&](int signum, bool deadline){
        printf("sig_ev get signum %d\n", signum);
        base->Shutdown();
    });

    // event loop until EventBase::Shutdown be called
    if (base->Dispatch() < 0) perror("base->Dispatch");
    printf("base->Dispatch return.\n");
    close(listener);
    delete listener_ev;
    delete sig_ev;
    delete base;
}

typedef struct sockaddr SA;
int tcp_server_init(int port, int listen_num) {
    int errno_save;
    int listener;
    listener = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0);
    if( listener == -1 ) return -1;
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
