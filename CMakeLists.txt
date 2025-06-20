cmake_minimum_required(VERSION 3.16)
set(ProjectMainName Socks5BalancerAsio)
project(${ProjectMainName})

set(CMAKE_CXX_STANDARD 20)
if (MSVC)

    # recommend config
    # cmake.exe -G "Visual Studio 16 2019" -DBOOST_INCLUDEDIR=H:/CodeLib/boost_1_73_0 -DOPENSSL_ROOT_DIR=h:\CodeLib\openssl-1.1.1h\x64 -DUSE_DYNAMIC_BOOST=0
    # https://stackoverflow.com/questions/52206636/force-cmake-to-look-for-x64-bit-libraries-instead-of-x32

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-DBOOST_ALL_NO_LIB)

    add_definitions(-DMSVC)
    if (COMMAND _WIN32_WINNT)
    else ()
        # https://learn.microsoft.com/en-us/cpp/porting/modifying-winver-and-win32-winnt?view=msvc-170
        # if _WIN32_WINNT not set. set the _WIN32_WINNT to 0x0A00 (Windows 10)
        # you can add " /D_WIN32_WINNT=0x0601" in CMAKE_CXX_FLAGS to set it to 0x0601 (win7)
        #add_definitions(-D_WIN32_WINNT=0x0A00)
        add_definitions(-D_WIN32_WINNT=0x0601)
        #add_definitions(-D_WIN32_WINNT=0x0602)

        # if the boost log tell you link error, set the _WIN32_WINNT to right version that you build boost
        # example `boost::log::v2s_mt_nt62` means you should set `_WIN32_WINNT=0x0602`
        # see: https://github.com/boostorg/log/issues/172
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


if (DEBUG_BOOST_ASIO)
    add_definitions(-DBOOST_ASIO_ENABLE_HANDLER_TRACKING)
endif (DEBUG_BOOST_ASIO)
# https://www.boost.org/doc/libs/1_81_0/libs/assert/doc/html/assert.html
add_definitions(-DBOOST_ENABLE_ASSERT_HANDLER)  #


if (USE_DYNAMIC_BOOST)
    message(STATUS "USE DYNAMIC BOOST !!!")
    add_definitions(-DBOOST_ALL_DYN_LINK=1)
else ()
    message(STATUS "USE STATIC BOOST !!!")
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME OFF)
endif ()
set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.81.0 REQUIRED COMPONENTS system filesystem program_options log log_setup url ${BOOST_THEAD_MODULE} REQUIRED)


message(STATUS "Boost_FOUND: ${Boost_FOUND}")
message(STATUS "BOOST_ROOT: ${BOOST_ROOT}")
message(STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")
message(STATUS "Boost_LIBRARY_DIRS: ${Boost_LIBRARY_DIRS}")
message(STATUS "Boost_LIBRARIES: ${Boost_LIBRARIES}")
message(STATUS "Boost_VERSION: ${Boost_VERSION}")
message(STATUS "Boost_LIB_VERSION: ${Boost_LIB_VERSION}")

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
    message(STATUS "Enable ProxyHandshakeAuth ON.")
    set(ProxyHandshakeAuthHeader
            src/ProxyHandshakeUtils/HttpServerImpl.cpp
            src/ProxyHandshakeUtils/HttpServerImpl.h
            src/ProxyHandshakeUtils/HttpClientImpl.cpp
            src/ProxyHandshakeUtils/HttpClientImpl.h
            src/ProxyHandshakeUtils/Socks5ServerImpl.cpp
            src/ProxyHandshakeUtils/Socks5ServerImpl.h
            src/ProxyHandshakeUtils/Socks5ClientImpl.cpp
            src/ProxyHandshakeUtils/Socks5ClientImpl.h
            src/ProxyHandshakeUtils/Socks4ServerImpl.cpp
            src/ProxyHandshakeUtils/Socks4ServerImpl.h
            src/ProxyHandshakeAuth.cpp
            src/ProxyHandshakeAuth.h
            src/ProxyHandshakeUtils/ProxyRelayMode.h)
else ()
    message(STATUS "Enable ProxyHandshakeAuth OFF.")
    set(ProxyHandshakeAuthHeader "")
endif ()


find_package(OpenSSL 1.1.0 REQUIRED)

message(STATUS "OPENSSL_FOUND: ${OPENSSL_FOUND}")
message(STATUS "OPENSSL_INCLUDE_DIR: ${OPENSSL_INCLUDE_DIR}")
message(STATUS "OPENSSL_CRYPTO_LIBRARY: ${OPENSSL_CRYPTO_LIBRARY}")
message(STATUS "OPENSSL_SSL_LIBRARY: ${OPENSSL_SSL_LIBRARY}")
message(STATUS "OPENSSL_LIBRARIES: ${OPENSSL_LIBRARIES}")
message(STATUS "OPENSSL_VERSION: ${OPENSSL_VERSION}")


# ============================================================================================


# https://www.mattkeeter.com/blog/2018-01-06-versioning/
execute_process(COMMAND git log --pretty=format:'%H' -n 1
        OUTPUT_VARIABLE GIT_REV
        ERROR_QUIET)

# Check whether we got any revision (which isn't
# always the case, e.g. when someone downloaded a zip
# file from Github instead of a checkout)
if ("${GIT_REV}" STREQUAL "")
    set(GIT_REV "N/A")
    set(GIT_DIFF "")
    set(GIT_TAG "N/A")
    set(GIT_BRANCH "N/A")
else ()
    execute_process(
            COMMAND bash -c "git diff --quiet --exit-code || echo +"
            OUTPUT_VARIABLE GIT_DIFF)
    execute_process(
            COMMAND git describe --exact-match --tags
            OUTPUT_VARIABLE GIT_TAG ERROR_QUIET)
    execute_process(
            COMMAND git rev-parse --abbrev-ref HEAD
            OUTPUT_VARIABLE GIT_BRANCH)

    string(STRIP "${GIT_REV}" GIT_REV)
    string(SUBSTRING "${GIT_REV}" 1 8 GIT_REV_F)
    string(STRIP "${GIT_DIFF}" GIT_DIFF)
    string(STRIP "${GIT_TAG}" GIT_TAG)
    string(STRIP "${GIT_BRANCH}" GIT_BRANCH)
endif ()

# https://gitlab.kitware.com/cmake/cmake/-/issues/18431
string(TIMESTAMP currentSeconds "%s")
message(STATUS "currentSeconds = ${currentSeconds}")

string(TIMESTAMP BUILD_DATETIME "%Y-%m-%d %H:%M:%S")
message(STATUS "BUILD_DATETIME = ${BUILD_DATETIME}")

set(VERSION "const char* GIT_REV=\"git-[${GIT_REV}-${GIT_DIFF}]\";
const char* GIT_TAG=\"${GIT_TAG}\";
const char* GIT_BRANCH=\"${GIT_BRANCH}\";
const char* BUILD_DATETIME=\"${BUILD_DATETIME}\";
")

message(STATUS "VERSION: ${VERSION}")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/VERSION/CodeVersion.h.in
        ${CMAKE_CURRENT_SOURCE_DIR}/src/VERSION/CodeVersion.h)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})


# ============================================================================================


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
        src/base64.h
        src/log/Log.cpp
        src/log/Log.h
        src/log/BoostAssertionFailed.cpp
        src/AuthClientManager.cpp
        src/AuthClientManager.h
        src/TcpRelaySession.cpp
        src/TcpRelaySession.h
        src/SessionRelayId.cpp
        src/SessionRelayId.h
        src/DelayCollection.cpp
        src/DelayCollection.h
        ${ProxyHandshakeAuthHeader})


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
    message(STATUS "Enable ProxyHandshakeAuth ON.")
    add_definitions(-DNeed_ProxyHandshakeAuth)
endif ()

if (WIN32)
    target_link_libraries(${ProjectMainName} wsock32 ws2_32 crypt32)
else ()
endif ()
