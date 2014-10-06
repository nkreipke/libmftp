project(libmftp)
cmake_minimum_required(VERSION 2.8)
set(CMAKE_C_FLAGS "-std=gnu11 -g")

add_subdirectory(src)
add_subdirectory(test)
