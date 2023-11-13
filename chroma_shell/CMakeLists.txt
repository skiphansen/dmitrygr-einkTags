cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set project name and language
project(chroma_shell C)
set(CMAKE_BUILD_TYPE "Debug")

# Compilation options
set(CMAKE_C_FLAGS "-std=gnu99 -Wall")
add_compile_options(-DDARWIN_DEBUG -DUDS_TP=UDS_TP_ISOTP_C)

set(COMMON ../common CACHE STRING "Path to shared code")

# Sources for this executable
set (SOURCES 
   serial_shell.c 
   chroma_cmds.c
   ${COMMON}/SerialFraming.c
   linenoise/linenoise.c
   logging.c
)

add_executable(chroma_shell ${SOURCES})

include_directories(${COMMON} linenoise)

install(TARGETS chroma_shell DESTINATION sbin/)


