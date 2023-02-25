cmake_policy(SET CMP0048 NEW)

project(Lab1-myFTP)
cmake_minimum_required(VERSION 3.10)
enable_testing()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

aux_source_directory(./src SRC_LIST)

message(${SRC_LIST})

add_executable(ftp_server ftp_server.cpp ${SRC_LIST})
target_compile_options(ftp_server PUBLIC -Ofast)
target_include_directories(ftp_server PUBLIC ./src)

add_executable(ftp_client ftp_client.cpp ${SRC_LIST})
target_compile_options(ftp_client PUBLIC -Ofast)
target_include_directories(ftp_client PUBLIC ./src)
