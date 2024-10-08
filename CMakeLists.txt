cmake_minimum_required(VERSION 3.10)
set (CMAKE_CXX_STANDARD 17)


project(lifestream_task)

# Включаем подпроекты
add_subdirectory(client)
add_subdirectory(server)

set(TEST_DATA_DIR "${CMAKE_SOURCE_DIR}/test_data")
set(CLIENT_TEST_DATA_DIR "${CMAKE_BINARY_DIR}/client")

file(COPY ${TEST_DATA_DIR} DESTINATION ${CLIENT_TEST_DATA_DIR})