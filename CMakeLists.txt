project(StandaloneCardEmu CXX)
cmake_minimum_required(VERSION 2.8)

if(NOT((${CMAKE_GENERATOR} STREQUAL "Unix Makefiles") OR (${CMAKE_GENERATOR} STREQUAL "MSYS Makefiles") OR (${CMAKE_GENERATOR} STREQUAL "MinGW Makefiles")))
  message(FATAL_ERROR "Chosen generator is not supported for this project")
endif()

message(STATUS "Building for ${CMAKE_SYSTEM_NAME}. Processor architecture is ${CMAKE_SYSTEM_PROCESSOR}")

#set some custom options and default values
set(ARCHSUFFIX ".${CMAKE_SYSTEM_PROCESSOR}")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

#set defaults
set(EXELINKFLAGS "")
set(OPTFLAGS "")

#setup RPATH and build directories
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_USE_RELATIVE_PATHS TRUE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

set(CMAKE_INSTALL_RPATH "./lib${ARCHSUFFIX}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib${ARCHSUFFIX}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build/lib${ARCHSUFFIX}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/build")

file(MAKE_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
file(MAKE_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

#print status
message(STATUS "Current build configuration:")
message(STATUS "CMAKE_GENERATOR=${CMAKE_GENERATOR}")
message(STATUS "CMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}")
message(STATUS "CMAKE_ARCHIVE_OUTPUT_DIRECTORY=${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_LIBRARY_OUTPUT_DIRECTORY=${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "CMAKE_RUNTIME_OUTPUT_DIRECTORY=${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

include_directories("${CMAKE_BINARY_DIR}")

#set flags for gcc
if(CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
  #platform dependent gcc flags
  set(OPTFLAGS "-pedantic")
  #general compiler flags for gcc compiler
  ##You can enable additional warnings for paranoics: -Wunsuffixed-float-constants -Wpadded
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra -Wshadow -Wstrict-overflow=5 -Wwrite-strings -Winit-self -Wmissing-include-dirs -Wbad-function-cast -Wc++-compat -Wcast-qual -Wcast-align -Wconversion -Wlogical-op -Wstrict-prototypes -Wold-style-definition -Wmissing-prototypes -Wmissing-declarations -Wpacked -Wredundant-decls -Wnested-externs -Winline -Wdisabled-optimization -Woverlength-strings -Wfloat-equal -Wswitch-default ${OPTFLAGS}")
  #set build-type dependent additional flags
  set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
  set(CMAKE_C_FLAGS_DEBUG "-DDEBUG -ggdb -no-pie")
  add_definitions(-D_GNU_SOURCE)
  set(EXELINKFLAGS "-Wl,-rpath-link,'${CMAKE_LIBRARY_OUTPUT_DIRECTORY}'")
endif()

#check include files
include(CheckIncludeFile)
include(CheckSymbolExists)
###check_include_file(inttypes.h HAVE_INTTYPES_H)

#save module configuration to config.h
configure_file(StandaloneAppSrc/config.h.in config.h)

#qt creator trick
if(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)
  include_directories("${CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES}")
endif(CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES)

#emu util
add_executable(cardemu StandaloneAppSrc/main.cpp)
###target_link_libraries(TODO)
install(TARGETS cardemu DESTINATION "cardemu")
