
add_executable(helloworld helloworld/main.cpp)

include_directories(../include)

include_directories(../src)

target_link_libraries(helloworld libevent pthread)
