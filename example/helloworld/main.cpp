#include <bits/stdc++.h>
#include "libevent.h"
#include "event.h"

int main()
{
    libevent::a++;
    libevent::print_hello_world();
    libevent::a++;
    libevent::print_hello_world();
    printf("main a is %d\n", libevent::a);

    libevent::print_fn_addr();
    printf("fn addr is %d\n", libevent::print_my_a);
    libevent::print_my_a();
    libevent::print_my_a();

    libevent::print_hello_fn_addr();
    libevent::print_hello_fn_addr();
    printf("main hello a is %d\n", libevent::Event::a);

    libevent::print_hello_fn_addr_static();
    libevent::print_hello_fn_addr_static();
    libevent::Event::event_fn();

    libevent::print_objs();
    libevent::print_objs();
    libevent::ee.obj_print();
}
