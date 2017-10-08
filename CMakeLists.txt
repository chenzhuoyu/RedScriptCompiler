cmake_minimum_required(VERSION 3.6)
project(RedScriptCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_definitions(-D_BSD_SOURCE)
include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCE_FILES
        include/compiler/Parser.h
        include/compiler/Tokenizer.h
        include/runtime/Object.h
        include/runtime/SyntaxError.h
        include/utils/Pointers.h
        include/utils/Strings.h
        src/compiler/Parser.cpp
        src/compiler/Tokenizer.cpp
        src/runtime/Object.cpp
        src/utils/Strings.cpp
        src/main.cpp)

add_executable(rsc ${SOURCE_FILES})
target_link_libraries(rsc -lfmt -ljemalloc)
