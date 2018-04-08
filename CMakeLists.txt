cmake_minimum_required(VERSION 3.6)
project(RedScriptCompiler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_definitions(-D_BSD_SOURCE)

link_directories(/usr/local/lib)
include_directories(/usr/local/include ${CMAKE_SOURCE_DIR}/include)

set(SOURCE_FILES
        include/compiler/AST.h
        include/compiler/Parser.h
        include/compiler/Tokenizer.h
        include/engine/GarbageCollector.h
        include/engine/Memory.h
        include/engine/Thread.h
        include/lockfree/DoublyLinkedList.h
        include/lockfree/HazardMemory.h
        include/lockfree/IndexGenerator.h
        include/runtime/Object.h
        include/runtime/ReferenceCounted.h
        include/runtime/SyntaxError.h
        include/utils/Immovable.h
        include/utils/NonCopyable.h
        include/utils/Preprocessor.h
        include/utils/Strings.h
        src/compiler/AST.cpp
        src/compiler/Parser.cpp
        src/compiler/Tokenizer.cpp
        src/engine/GarbageCollector.cpp
        src/engine/Memory.cpp
        src/engine/Thread.cpp
        src/runtime/Object.cpp
        src/runtime/ReferenceCounted.cpp
        src/utils/Strings.cpp
        src/main.cpp)

add_executable(rsc ${SOURCE_FILES})
target_link_libraries(rsc -lfmt -ljemalloc)
