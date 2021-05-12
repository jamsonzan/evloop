//
// Created by jamsonzan on 2021/5/13.
//

#include <bits/stdc++.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "event.h"
#include "reactor.h"

int tcp_server_init(int port, int listen_num);

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
        return;
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

typedef struct sockaddr SA;
int tcp_server_init(int port, int listen_num)
{
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

    if( bind(listener, (SA*)&sin, sizeof(sin)) < 0 )
        goto error;

    if( listen(listener, listen_num) < 0 )
        goto error;

    return listener;

    error:
    errno_save = errno;
    close(listener);
    errno = errno_save;

    return -1;
}

int main() {
    // TestReactorStdin();
    TestReactorEcho();
}
