cmake_minimum_required(VERSION 3.10)

project(client_app)

add_executable(client client.cpp ../package.cpp)

target_compile_options(client PRIVATE -Wall -Wextra -O2)