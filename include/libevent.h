//
// Created by jamsonzan on 2021/5/1.
//

#ifndef LIBEVENT2_LIBEVENT_H
#define LIBEVENT2_LIBEVENT_H

#include <cstddef>
#include "event.h"

namespace libevent {
    void print_hello_world();
    void print_fn_addr();

    void print_hello_fn_addr();
    void print_hello_fn_addr_static();

    void print_objs();
}

#endif //LIBEVENT2_LIBEVENT_H
