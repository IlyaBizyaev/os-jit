cmake_minimum_required(VERSION 3.1)

project(hq9p)

add_executable(hq9p main.cpp)

install(TARGETS hq9p
        RUNTIME DESTINATION bin)
