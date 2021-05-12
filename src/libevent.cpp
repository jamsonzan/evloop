#include <bits/stdc++.h>
#include "libevent.h"

namespace libevent {
    void print_hello_world() {
        printf("libevent, hello world! %d\n", a);
    }
    void print_fn_addr() {
        printf("fn addr is %d\n", print_my_a);
        print_my_a();
        print_my_a();
    }

    int Event::a = 199;
    Event ee(999);

    void print_hello_fn_addr() {
        Event::a++;
        printf("hello a is %d\n", Event::a);
    }

    void print_hello_fn_addr_static() {
        Event::event_fn();
    }

    void print_objs() {
        ee.obj_print();
    }
}
