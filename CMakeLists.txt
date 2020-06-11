cmake_minimum_required(VERSION 3.16)
set(ProjectMainName Socks5BalancerAsio)
project(${ProjectMainName})

set(CMAKE_CXX_STANDARD 20)
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DBOOST_ALL_NO_LIB)
else ()
    add_definitions(-Wall -Wextra)
endif ()

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.70.0 REQUIRED COMPONENTS system filesystem program_options REQUIRED)

message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")

find_package(OpenSSL 1.1.0 REQUIRED)

add_executable(${ProjectMainName}
        src/main.cpp
        src/TcpRelayServer.cpp
        src/TcpRelayServer.h
        src/UpstreamPool.cpp
        src/UpstreamPool.h
        src/StateMonitorServer.cpp
        src/StateMonitorServer.h
        src/ConnectTestHttps.cpp
        src/ConnectTestHttps.h)


include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${ProjectMainName} ${Boost_LIBRARIES})
link_directories(${Boost_LIBRARY_DIRS})

include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(${ProjectMainName} ${OPENSSL_LIBRARIES})
if (OPENSSL_VERSION VERSION_GREATER_EQUAL 1.1.1)
    option(ENABLE_SSL_KEYLOG "Build with SSL KeyLog support" ON)
    if (ENABLE_SSL_KEYLOG)
        add_definitions(-DENABLE_SSL_KEYLOG)
    endif ()

    option(ENABLE_TLS13_CIPHERSUITES "Build with TLS1.3 ciphersuites support" ON)
    if (ENABLE_TLS13_CIPHERSUITES)
        add_definitions(-DENABLE_TLS13_CIPHERSUITES)
    endif ()
endif ()

if (WIN32)
    target_link_libraries(${ProjectMainName} wsock32 ws2_32 crypt32)
else ()
endif ()
