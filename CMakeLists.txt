cmake_minimum_required(VERSION 3.16)
set(ProjectMainName Socks5BalancerAsio)
project(${ProjectMainName})

set(CMAKE_CXX_STANDARD 17)
if (MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DBOOST_ALL_NO_LIB)

    add_definitions(-DMSVC)
    add_definitions(-D_WIN32_WINNT=0x0A00)
    add_compile_options(/bigobj)
else ()
    add_definitions(-Wall -Wextra)
endif ()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost 1.70.0 REQUIRED COMPONENTS system filesystem program_options REQUIRED)

message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")

if (MSVC)
else ()
    if (${Boost_VERSION} VERSION_LESS_EQUAL 1.72.0)
        # https://github.com/boostorg/asio/issues/312
        # https://github.com/microsoft/vcpkg/issues/9731
        # https://github.com/boostorg/beast/issues/1884
        add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)
        message(STATUS "Boost_VERSION <= 1.72")
    endif ()
endif ()

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
        src/ConnectTestHttps.h
        src/ConfigLoader.cpp
        src/ConfigLoader.h
        src/TcpTest.cpp
        src/TcpTest.h)


target_link_libraries(${ProjectMainName} ${CMAKE_THREAD_LIBS_INIT})

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
