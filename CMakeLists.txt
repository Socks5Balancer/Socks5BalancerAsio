cmake_minimum_required(VERSION 3.16)
set(ProjectMainName Socks5BalancerAsio)
project(${ProjectMainName})

set(CMAKE_CXX_STANDARD 17)
if (MSVC)

    # recommend config
    # cmake.exe -G "Visual Studio 16 2019" -DBOOST_INCLUDEDIR=H:/CodeLib/boost_1_73_0 -DOPENSSL_ROOT_DIR=h:\CodeLib\openssl-1.1.1h\x64 -DUSE_DYNAMIC_BOOST=0
    # https://stackoverflow.com/questions/52206636/force-cmake-to-look-for-x64-bit-libraries-instead-of-x32

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DBOOST_ALL_NO_LIB)

    add_definitions(-DMSVC)
    if (COMMAND _WIN32_WINNT)
    else ()
        # if _WIN32_WINNT not set. set the _WIN32_WINNT to 0x0A00 (Windows 10)
        # you can add " /D_WIN32_WINNT=0x0601" in CMAKE_CXX_FLAGS to set it to 0x0601 (win7)
        add_definitions(-D_WIN32_WINNT=0x0A00)
    endif ()
    add_compile_options(/bigobj)

    add_definitions(/W4)

else ()
    add_definitions(-Wall -Wextra)
endif ()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

option(MINI_BUILD_MODE "Config Enable MINI BUILD Mode")

if (MINI_BUILD_MODE)
    set(NOT_USE_BOOST_THEAD 1)
    set(NOT_USE_EmbedWebServer 1)
    set(FORCE_disableConnectTest 1)
    set(FORCE_disableConnectionTracker 1)
    set(FORCE_traditionTcpRelay 1)
    message(STATUS "Use MINI_BUILD_MODE !!!")
else ()
    option(NOT_USE_BOOST_THEAD "Config NOT_USE_BOOST_THEAD")
    option(NOT_USE_EmbedWebServer "Config NOT_USE_EmbedWebServer")
    option(FORCE_disableConnectTest "Config FORCE_disableConnectTest")
    option(FORCE_disableConnectionTracker "Config FORCE_disableConnectionTracker")
    option(FORCE_traditionTcpRelay "Config FORCE_traditionTcpRelay")
endif ()

if (NOT_USE_EmbedWebServer)
    add_definitions(-DDISABLE_EmbedWebServer)
    message(STATUS "NOT_USE_EmbedWebServer !!!")
endif ()

if (FORCE_disableConnectTest)
    add_definitions(-DFORCE_disableConnectTest)
    message(STATUS "FORCE_disableConnectTest !!!")
endif ()
if (FORCE_disableConnectionTracker)
    add_definitions(-DFORCE_disableConnectionTracker)
    message(STATUS "FORCE_disableConnectionTracker !!!")
endif ()
if (FORCE_traditionTcpRelay)
    add_definitions(-DFORCE_traditionTcpRelay)
    message(STATUS "FORCE_traditionTcpRelay !!!")
endif ()

if (NOT_USE_BOOST_THEAD)
    set(BOOST_THEAD_MODULE "")
    message(STATUS "NOT_USE_BOOST_THEAD !!!")
else ()
    set(BOOST_THEAD_MODULE "thread")
    add_definitions(-DUSE_BOOST_THEAD)
endif ()

option(USE_DYNAMIC_BOOST "Config USE_DYNAMIC_BOOST")

if (USE_DYNAMIC_BOOST)
    message(STATUS "USE DYNAMIC BOOST !!!")
    add_definitions(-DBOOST_ALL_DYN_LINK=1)
else ()
    message(STATUS "USE STATIC BOOST !!!")
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
endif ()
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.81.0 REQUIRED COMPONENTS system filesystem program_options ${BOOST_THEAD_MODULE} REQUIRED)


message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")

if (MSVC)
else ()
    #    if (${Boost_VERSION} VERSION_LESS_EQUAL 1.72.0)
    #        # https://github.com/boostorg/asio/issues/312
    #        # https://github.com/microsoft/vcpkg/issues/9731
    #        # https://github.com/boostorg/beast/issues/1884
    #        add_definitions(-DBOOST_ASIO_DISABLE_CONCEPTS)
    #        message(STATUS "Boost_VERSION <= 1.72")
    #    endif ()
endif ()

option(Need_ProxyHandshakeAuth "Enable ProxyHandshakeAuth" OFF)
if (Need_ProxyHandshakeAuth)
    set(ProxyHandshakeAuthHeader
            src/ProxyHandshakeUtils/HttpServerImpl.cpp
            src/ProxyHandshakeUtils/HttpServerImpl.h
            src/ProxyHandshakeUtils/HttpClientImpl.cpp
            src/ProxyHandshakeUtils/HttpClientImpl.h
            src/ProxyHandshakeUtils/Socks5ServerImpl.cpp
            src/ProxyHandshakeUtils/Socks5ServerImpl.h
            src/ProxyHandshakeUtils/Socks5ClientImpl.cpp
            src/ProxyHandshakeUtils/Socks5ClientImpl.h
            src/ProxyHandshakeAuth.cpp
            src/ProxyHandshakeAuth.h)
else ()
    set(ProxyHandshakeAuthHeader "")
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
        src/TcpTest.h
        src/FirstPackAnalyzer.cpp
        src/FirstPackAnalyzer.h
        src/TcpRelayStatisticsInfo.cpp
        src/TcpRelayStatisticsInfo.h
        src/ConnectionTracker.cpp
        src/ConnectionTracker.h
        src/ConnectType.h
        src/EmbedWebServer.cpp
        src/EmbedWebServer.h
        src/ConfigRuleEnum.h
        src/AsyncDelay.cpp
        src/AsyncDelay.h
        src/UtilTools.cpp
        src/UtilTools.h
        ${ProxyHandshakeAuth})


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

if (Need_ProxyHandshakeAuth)
    add_definitions(-DNeed_ProxyHandshakeAuth)
endif ()

if (WIN32)
    target_link_libraries(${ProjectMainName} wsock32 ws2_32 crypt32)
else ()
endif ()
