cmake_minimum_required(VERSION 3.20)

project(mutator_sdk)
set(CMAKE_CXX_STANDARD 20)

option(USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)

if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    option(BUILD_TESTING "set ON to build library tests" ON)
else()
    option(BUILD_TESTING "set ON to build library tests" OFF)
endif()

option(BUILD_FUZZING "set ON to build library fuzzers" OFF)
option(USE_OPENSSL "set OFF to build without OpenSSL" ON)

add_library(mutator_sdk INTERFACE)

target_include_directories(mutator_sdk INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Threads REQUIRED)
target_link_libraries(mutator_sdk INTERFACE ${CMAKE_THREAD_LIBS_INIT})

set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)

target_link_libraries(mutator_sdk INTERFACE ${CURL_LIBRARIES})

find_package(Protobuf REQUIRED)
target_link_libraries(mutator_sdk INTERFACE ${Protobuf_LIBRARIES})
target_include_directories(mutator_sdk INTERFACE ${Protobuf_INCLUDE_DIRS})

if(USE_STANDALONE_ASIO)
    target_compile_definitions(mutator_sdk INTERFACE ASIO_STANDALONE)
    find_path(ASIO_PATH asio.hpp)

    if(NOT ASIO_PATH)
        message(FATAL_ERROR "Standalone Asio not found")
    else()
        target_include_directories(mutator_sdk INTERFACE ${ASIO_PATH})
    endif()
else()
    find_package(Boost 1.53.0 COMPONENTS system thread REQUIRED)
    target_link_libraries(mutator_sdk INTERFACE ${Boost_LIBRARIES})
    target_include_directories(mutator_sdk INTERFACE ${Boost_INCLUDE_DIR})

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(mutator_sdk INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.53.0 COMPONENTS regex REQUIRED)
        target_link_libraries(mutator_sdk INTERFACE ${Boost_LIBRARIES})
        target_include_directories(mutator_sdk INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()

if(WIN32)
    target_link_libraries(mutator_sdk INTERFACE ws2_32 wsock32)
endif()

if(APPLE)
    if(EXISTS /usr/local/opt/openssl)
        set(OPENSSL_ROOT_DIR /usr/local/opt/openssl)
    elseif(EXISTS /opt/homebrew/opt/openssl)
        set(OPENSSL_ROOT_DIR /opt/homebrew/opt/openssl)
    endif()
endif()

if(USE_OPENSSL)
    find_package(OpenSSL)
endif()

if(OPENSSL_FOUND)
    target_compile_definitions(mutator_sdk INTERFACE HAVE_OPENSSL)
    target_link_libraries(mutator_sdk INTERFACE ${OPENSSL_LIBRARIES})
    target_include_directories(mutator_sdk INTERFACE ${OPENSSL_INCLUDE_DIR})
endif()

set(Protobuf_USE_STATIC_LIBS ON)

if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT MSVC)
        add_compile_options(-std=c++11 -Wall -Wextra)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-Wthread-safety)
        endif()
    else()
        add_compile_options(/W1)
    endif()

    find_package(Boost 1.53.0 COMPONENTS system thread filesystem)

    if(Boost_FOUND)
        add_executable(sample_executable main.cpp structs/mutator.pb.cc structs/mutator.pb.h)
        target_link_libraries(sample_executable mutator_sdk)

        target_link_libraries(sample_executable ${Boost_LIBRARIES})
        target_include_directories(sample_executable PRIVATE ${Boost_INCLUDE_DIR})
     endif()

    install(FILES asio_compatibility.hpp server_ws.hpp client_ws.hpp crypto.hpp utility.hpp
            status_code.hpp mutex.hpp instance.hpp DESTINATION include/webserver)
endif()