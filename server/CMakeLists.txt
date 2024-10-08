cmake_minimum_required(VERSION 3.10)

project(server_app)

add_executable(server server.cpp ../package.cpp)

target_compile_options(server PRIVATE -Wall -Wextra -O2)