//
// Created by jamsonzan on 2021/5/8.
//

#ifndef LIBEVENT2_EVENT_H
#define LIBEVENT2_EVENT_H

#include <cstdio>

namespace libevent {

enum {
    EV_READ = 0x01,
    EV_WRITE = 0x02,
    EV_CLOSED = 0x04,
    EV_TIMEOUT = 0x08,
    EV_SIGNAL = 0x10
};

static int a = 99;

static int print_my_a() {
    static int b = 99;
    b++;
    printf("my b is %d\n", b);
}

class Event {
public:
    Event(int i):obj(i) {}

    static int a;

    static int event_fn() {
        static int e = 399;
        e++;
        printf("event e is %d, and addr is %d\n", e, event_fn);
    }

    void obj_print() {
        obj++;
        printf("objs is %d\n", obj);
    }

    int obj;
};

extern Event ee;

}

#endif //LIBEVENT2_EVENT_H
