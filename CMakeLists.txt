PROJECT(dcc_original)
cmake_minimum_required(VERSION 3.1)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt5Core)

OPTION(dcc_build_tests "Enable unit tests." OFF)
#SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS)
IF("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D__UNIX__ -D_CRT_NONSTDC_NO_DEPRECATE -DNOMINMAX)
    ADD_DEFINITIONS(/W4)
ELSE()
#-D_GLIBCXX_DEBUG
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++11")
    SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} "  ) #--coverage
ENDIF()
SET(CMAKE_CXX_STANDARD 11)
SET(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeScripts;${CMAKE_MODULE_PATH})
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})
include(cotire)
FIND_PACKAGE(Boost)
IF(dcc_build_tests)
enable_testing()
    FIND_PACKAGE(GMock)
ENDIF()


INCLUDE_DIRECTORIES(
    3rd_party/libdisasm
    include
    include/idioms
    common
    ${Boost_INCLUDE_DIRS}
)


ADD_SUBDIRECTORY(3rd_party)
ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(tools)


set(dcc_LIB_SOURCES
    src/CallConvention.cpp
    src/ast.cpp
    src/backend.cpp
    src/bundle.cpp
    src/chklib.cpp
    src/comwrite.cpp
    src/control.cpp
    src/dataflow.cpp
    src/disassem.cpp
    src/DccFrontend.cpp
    src/error.cpp
    src/fixwild.cpp
    src/graph.cpp
    src/hlicode.cpp
    src/hltype.cpp
    src/machine_x86.cpp
    src/icode.cpp
    src/RegisterNode
    src/idioms.cpp
    src/idioms/idiom1.cpp
    src/idioms/arith_idioms.cpp
    src/idioms/call_idioms.cpp
    src/idioms/epilogue_idioms.cpp
    src/idioms/mov_idioms.cpp
    src/idioms/neg_idioms.cpp
    src/idioms/shift_idioms.cpp
    src/idioms/xor_idioms.cpp
    src/locident.cpp
    src/liveness_set.cpp
    src/parser.cpp
    src/procs.cpp
    src/project.cpp
    src/Procedure.cpp
    src/proplong.cpp
    src/reducible.cpp
    src/scanner.cpp
    src/symtab.cpp
    src/udm.cpp
    src/BasicBlock.cpp
    src/dcc_interface.cpp
)
set(dcc_SOURCES
    src/dcc.cpp
)
set(dcc_HEADERS
    include/ast.h
    include/bundle.h
    include/BinaryImage.h
    include/DccFrontend.h
    include/Enums.h
    include/dcc.h
    include/disassem.h
    include/dosdcc.h
    include/error.h
    include/graph.h
    include/hlicode.h
    include/machine_x86.h
    include/icode.h
    include/idioms/idiom.h
    include/idioms/idiom1.h
    include/idioms/arith_idioms.h
    include/idioms/call_idioms.h
    include/idioms/epilogue_idioms.h
    include/idioms/mov_idioms.h
    include/idioms/neg_idioms.h
    include/idioms/shift_idioms.h
    include/idioms/xor_idioms.h
    include/locident.h
    include/CallConvention.h
    include/project.h
    include/scanner.h
    include/state.h
    include/symtab.h
    include/types.h
    include/Procedure.h
    include/StackFrame.h
    include/BasicBlock.h
    include/dcc_interface.h

)

SOURCE_GROUP(Source FILES ${dcc_SOURCES})
SOURCE_GROUP(Headers FILES ${dcc_HEADERS})

ADD_LIBRARY(dcc_lib STATIC ${dcc_LIB_SOURCES} ${dcc_HEADERS})
qt5_use_modules(dcc_lib Core)
#cotire(dcc_lib)

ADD_EXECUTABLE(dcc_original ${dcc_SOURCES} ${dcc_HEADERS})
ADD_DEPENDENCIES(dcc_original dcc_lib)
TARGET_LINK_LIBRARIES(dcc_original dcc_lib dcc_hash disasm_s)
qt5_use_modules(dcc_original Core)
SET_PROPERTY(TARGET dcc_original PROPERTY CXX_STANDARD 11)
SET_PROPERTY(TARGET dcc_original PROPERTY CXX_STANDARD_REQUIRED ON)
#ADD_SUBDIRECTORY(gui)
if(dcc_build_tests)
ADD_SUBDIRECTORY(src)
endif()

