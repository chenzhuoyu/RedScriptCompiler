cmake_minimum_required(VERSION 3.8)
project(RedScriptCompiler)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_definitions(-D_GLIBCXX_DEBUG)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_C_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_ASM_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++-5.0)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_compile_options(-g3)
add_compile_options(-pthread)
add_compile_options(-D_BSD_SOURCE)

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
