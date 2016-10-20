project(cohttp CXX C)
cmake_minimum_required(VERSION 2.8)

if(COMMAND cmake_policy)
    cmake_policy(SET CMP0015 NEW)
endif(COMMAND cmake_policy)

#set(CMAKE_C_COMPILER	"/usr/bin/clang")
#set(CMAKE_CXX_COMPILER	"/usr/bin/clang++")
#set(CMAKE_AR			"/usr/bin/llvm-ar")
#set(CMAKE_LINKER		"/usr/bin/llvm-ld")
#set(CMAKE_NM			"/usr/bin/llvm-nm")
#set(CMAKE_OBJDUMP		"/usr/bin/llvm-objdump")
#set(CMAKE_RANLIB		"/usr/bin/llvm-ranlib")

set(COHTTP_ROOT ${CMAKE_SOURCE_DIR})
set(LIBRARY_OUTPUT_PATH ${COHTTP_ROOT}/bin/${CMAKE_BUILD_TYPE})
set(EXECUTABLE_OUTPUT_PATH ${COHTTP_ROOT}/bin/${CMAKE_BUILD_TYPE})
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
link_directories(${LIBRARY_OUTPUT_PATH})

#find_package(Boost)
#MESSAGE(STATUS "boost found ... ${Boost_FOUND}")
#if(${Boost_FOUND})
#	include_directories(${Boost_INCLUDE_DIRS})
#	link_directories(${Boost_LIBRARY_DIRS})
#	set(BOOST boost)
#endif()

add_definitions(--std=c++11)

option(USE_BOOST_COROUTINE  "use boost coroutine" ON)

if(${CMAKE_BUILD_TYPE} MATCHES Debug)
    add_definitions(-DDEBUG)
endif(${CMAKE_BUILD_TYPE} MATCHES Debug)

message(STATUS "COHTTP VERSION=${BUILD_VERSION}")
add_definitions(-DBUILD_VERSION=${BUILD_VERSION})

add_subdirectory(src)
