cmake_minimum_required(VERSION 2.8)

###################################################################################
project(libevent2)

add_subdirectory(./example)

add_library(libevent src/reactor.h src/reactor.cpp include/event.h src/event.cpp src/driver_io.h src/driver_signal.h src/driver_timer.h)

include_directories(./include)

include_directories(src)
