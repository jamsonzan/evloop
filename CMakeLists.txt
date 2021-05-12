cmake_minimum_required(VERSION 2.8)

###################################################################################
project(libevent2)

add_subdirectory(./example)

add_library(libevent src/libevent.cpp include/libevent.h src/reactor.h src/reactor.cpp src/event.h)

include_directories(./include)

include_directories(src)
