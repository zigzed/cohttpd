
include_directories(${COHTTP_ROOT}/src/3rd/libgo)
include_directories(${COHTTP_ROOT}/src/3rd/spdlog/include)
#include_directories(${COHTTP_ROOT}/src/3rd/asio-1.10.8/include)

set(COHTTP_SRC
    cohttp.cpp cohttp.hpp
    errors.hpp errors.cpp
    semops.hpp semops.cpp
    server.hpp server.cpp
    iobufs.hpp iobufs.cpp bitset.hpp bitset.cpp
    codecs.hpp codecs.cpp
    request.hpp request.cpp response.hpp response.cpp
    http_parser.h http_parser.c
    handler.hpp handler.cpp
    modules.hpp modules.cpp
   )
add_executable(cohttp ${COHTTP_SRC})
target_link_libraries(cohttp libgo boost_context boost_system boost_filesystem pthread dl)

add_library(mod_default MODULE
    modules/default.hpp modules/default.cpp
)
target_link_libraries(mod_default libgo pthread dl)

add_library(mod_hello MODULE
    modules/hello.hpp modules/hello.cpp
)
target_link_libraries(mod_hello libgo pthread dl)

add_library(mod_redis MODULE
    modules/redis.hpp modules/redis.cpp
)
target_link_libraries(mod_redis libgo hiredis pthread dl)

add_executable(unittest unittest.cpp 3rd/doctest.h
    codecs.cpp iobufs.cpp request.cpp bitset.cpp
    http_parser.c cohttp.hpp
    )
target_link_libraries(unittest libgo boost_context boost_system pthread dl)
