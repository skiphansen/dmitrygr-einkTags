cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_VERBOSE_MAKEFILE ON)

# set project name and language
project(chroma_shell C)
set(CMAKE_BUILD_TYPE "Debug")

option(COBS_TEST "Build for testing COBS library" OFF)

# Compilation options
set(CMAKE_C_FLAGS "-std=gnu99 -Wall -MD")

set(RF_CAT_INCLUDE ../firmware/rfcat/include)
set(COMMON ../common)

# Sources for this executable
if(COBS_TEST)
   set (SOURCES 
     ${COMMON}/CobsFraming.c
     logging.c
   )
   add_compile_options(-DCOBS_TEST)
else()
   set (SOURCES 
      serial_shell.c 
      chroma_cmds.c
      sfdp-parser.c
      ${COMMON}/CobsFraming.c
      linenoise/linenoise.c
      logging.c
      lut_dump.c
   )
endif(COBS_TEST)

add_executable(chroma_shell ${SOURCES})

include_directories(. ${COMMON} ${RF_CAT_INCLUDE} linenoise)

install(TARGETS chroma_shell DESTINATION sbin/)


