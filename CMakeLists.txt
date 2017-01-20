cmake_minimum_required(VERSION 3.6)
project(RedScriptCompiler)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_definitions(-D_BSD_SOURCE)
include_directories(
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/compiler
        ${CMAKE_SOURCE_DIR}/include/utils)

set(SOURCE_FILES
        include/compiler/CompilationError.h
        include/compiler/Tokenizer.h
        include/utils/Strings.h
        src/compiler/Strings.cpp
        src/compiler/Tokenizer.cpp
        src/main.cpp)

add_executable(rsc ${SOURCE_FILES})
target_link_libraries(rsc /usr/local/lib/libfmt.a)