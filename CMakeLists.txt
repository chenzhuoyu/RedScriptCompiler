cmake_minimum_required(VERSION 3.8)
project(RedScriptCompiler C CXX)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_definitions(-D_GLIBCXX_DEBUG)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_C_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_ASM_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++-5.0)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_compile_options(-g3)
add_compile_options(-pthread)

add_definitions(-D_BSD_SOURCE)
add_definitions(-DONE_SOURCE=0)
add_definitions(-DTCC_TARGET_X86_64)
add_definitions(-DCONFIG_TRIPLET="x86_64-linux-gnu")

link_directories(/usr/local/lib)
include_directories(/usr/local/include ${CMAKE_SOURCE_DIR}/libtcc-mt ${CMAKE_SOURCE_DIR}/include)

set(LIBTCC_MT
        libtcc-mt/tccelf.h
        libtcc-mt/i386-asm.c
        libtcc-mt/i386-asm.h
        libtcc-mt/i386-tok.h
        libtcc-mt/libtcc.c
        libtcc-mt/libtcc.h
        libtcc-mt/tccasm.c
        libtcc-mt/tccelf.c
        libtcc-mt/tccgen.c
        libtcc-mt/tcc.h
        libtcc-mt/tccpp.c
        libtcc-mt/tccrun.c
        libtcc-mt/tcctok.h
        libtcc-mt/x86_64-asm.h
        libtcc-mt/x86_64-defs.h
        libtcc-mt/x86_64-gen.c
        libtcc-mt/x86_64-link.c)

set(SOURCE_FILES
        ${LIBTCC_MT}
        include/compiler/AST.h
        include/compiler/CodeGenerator.h
        include/compiler/Parser.h
        include/compiler/Tokenizer.h
        include/engine/Bytecode.h
        include/engine/GarbageCollector.h
        include/engine/Memory.h
        include/engine/Thread.h
        include/lockfree/DoublyLinkedList.h
        include/lockfree/HazardMemory.h
        include/lockfree/IndexGenerator.h
        include/runtime/BoolObject.h
        include/runtime/CodeObject.h
        include/runtime/DecimalObject.h
        include/runtime/ExceptionBlockObject.h
        include/runtime/InternalError.h
        include/runtime/IntObject.h
        include/runtime/NativeClassObject.h
        include/runtime/NullObject.h
        include/runtime/Object.h
        include/runtime/ReferenceCounted.h
        include/runtime/RuntimeError.h
        include/runtime/StringObject.h
        include/runtime/SyntaxError.h
        include/runtime/TypeError.h
        include/utils/Immovable.h
        include/utils/NonCopyable.h
        include/utils/Preprocessor.h
        include/utils/Strings.h
        include/RedScript.h
        src/compiler/AST.cpp
        src/compiler/CodeGenerator.cpp
        src/compiler/Parser.cpp
        src/compiler/Tokenizer.cpp
        src/engine/GarbageCollector.cpp
        src/engine/Memory.cpp
        src/engine/Thread.cpp
        src/runtime/BoolObject.cpp
        src/runtime/CodeObject.cpp
        src/runtime/DecimalObject.cpp
        src/runtime/ExceptionBlockObject.cpp
        src/runtime/IntObject.cpp
        src/runtime/NativeClassObject.cpp
        src/runtime/NullObject.cpp
        src/runtime/Object.cpp
        src/runtime/ReferenceCounted.cpp
        src/runtime/StringObject.cpp
        src/utils/Strings.cpp
        src/RedScript.cpp
        src/main.cpp src/engine/Interpreter.cpp include/engine/Interpreter.h)

add_executable(rsc ${SOURCE_FILES})
target_link_libraries(rsc -ldl -lfmt -ljemalloc)
