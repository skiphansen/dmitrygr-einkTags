cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set project name and language
project(uartTool C)
set(CMAKE_BUILD_TYPE "Debug")

# Compilation options
set(CMAKE_C_FLAGS "-std=gnu99 -Wall")

# Sources for this executable
set (SOURCES 
   main.c
   transport.c
   transportSerial.c
)

add_executable(uartTool ${SOURCES})

