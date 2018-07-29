cmake_minimum_required(VERSION 3.8)
project(RedScriptCompiler C CXX)

if (NOT (CMAKE_SIZEOF_VOID_P EQUAL 8))
    message(ERROR "RedScript only support 64-bit processors.")
endif ()

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_C_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_ASM_COMPILER /usr/bin/clang-5.0)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++-5.0)
endif ()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (${CMAKE_BUILD_TYPE} MATCHES "Debug")
    add_definitions(-D_GLIBCXX_DEBUG)
else ()
    add_compile_options(-O3)
    add_compile_options(-flto=full)
endif ()

add_compile_options(-g3)
add_compile_options(-fPIC)
add_compile_options(-pthread)

add_definitions(-D_BSD_SOURCE)
add_definitions(-DONE_SOURCE=0)
add_definitions(-DTCC_TARGET_X86_64)
add_definitions(-DCONFIG_TRIPLET="x86_64-linux-gnu")

link_directories(
        /usr/local/lib
        ${CMAKE_SOURCE_DIR}/libbid/lib
        ${CMAKE_SOURCE_DIR}/libmpir/lib)

include_directories(
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/libbid/include
        ${CMAKE_SOURCE_DIR}/libmpir/include
        ${CMAKE_SOURCE_DIR}/libtcc-mt
        ${CMAKE_SOURCE_DIR}/include)

set(LIBBID
        libbid/include/bid_conf.h
        libbid/include/bid_dfp.h)

set(LIBMPIR
        libmpir/include/mpir.h)

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
        ${LIBBID}
        ${LIBMPIR}
        ${LIBTCC_MT}
        include/compiler/AST.h
        include/compiler/CodeGenerator.h
        include/compiler/Parser.h
        include/compiler/Tokenizer.h
        include/engine/Builtins.h
        include/engine/Bytecode.h
        include/engine/Closure.h
        include/engine/GarbageCollector.h
        include/engine/Interpreter.h
        include/engine/Memory.h
        include/engine/Thread.h
        include/runtime/ArrayObject.h
        include/runtime/BoolObject.h
        include/runtime/BoundMethodObject.h
        include/runtime/CodeObject.h
        include/runtime/DecimalObject.h
        include/runtime/ExceptionObject.h
        include/runtime/FunctionObject.h
        include/runtime/IntObject.h
        include/runtime/MapObject.h
        include/runtime/NativeClassObject.h
        include/runtime/NativeFunctionObject.h
        include/runtime/NullObject.h
        include/runtime/Object.h
        include/runtime/ProxyObject.h
        include/runtime/ReferenceCounted.h
        include/runtime/SliceObject.h
        include/runtime/StringObject.h
        include/runtime/TupleObject.h
        include/runtime/UnboundMethodObject.h
        include/utils/Decimal.h
        include/utils/FreeList.h
        include/utils/Immovable.h
        include/utils/Integer.h
        include/utils/Lists.h
        include/utils/NFI.h
        include/utils/NonCopyable.h
        include/utils/Preprocessor.h
        include/utils/RWLock.h
        include/utils/SpinLock.h
        include/utils/Strings.h
        include/RedScript.h
        src/compiler/AST.cpp
        src/compiler/CodeGenerator.cpp
        src/compiler/Parser.cpp
        src/compiler/Tokenizer.cpp
        src/engine/Builtins.cpp
        src/engine/GarbageCollector.cpp
        src/engine/Interpreter.cpp
        src/engine/Memory.cpp
        src/runtime/ArrayObject.cpp
        src/runtime/BoolObject.cpp
        src/runtime/BoundMethodObject.cpp
        src/runtime/CodeObject.cpp
        src/runtime/DecimalObject.cpp
        src/runtime/ExceptionObject.cpp
        src/runtime/FunctionObject.cpp
        src/runtime/IntObject.cpp
        src/runtime/MapObject.cpp
        src/runtime/NativeClassObject.cpp
        src/runtime/NativeFunctionObject.cpp
        src/runtime/NullObject.cpp
        src/runtime/Object.cpp
        src/runtime/ProxyObject.cpp
        src/runtime/ReferenceCounted.cpp
        src/runtime/SliceObject.cpp
        src/runtime/StringObject.cpp
        src/runtime/TupleObject.cpp
        src/runtime/UnboundMethodObject.cpp
        src/utils/Decimal.cpp
        src/utils/Integer.cpp
        src/utils/Strings.cpp
        src/RedScript.cpp
        src/main.cpp)

add_executable(rsc ${SOURCE_FILES})
target_link_libraries(rsc bid_${CMAKE_SYSTEM_NAME} mpir_${CMAKE_SYSTEM_NAME} dl fmt jemalloc)
