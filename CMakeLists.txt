cmake_minimum_required(VERSION 3.21)

project(MercuryChat
    VERSION 0.1.0
    DESCRIPTION "UTF-8 chat client for the Mercury HF modem"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

option(MERCURYCHAT_USE_HAMLIB "Build hamlib CAT support" ON)

if(MERCURYCHAT_USE_HAMLIB)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(HAMLIB QUIET IMPORTED_TARGET hamlib)
    endif()

    if(HAMLIB_FOUND)
        set(MERCURYCHAT_HAMLIB_TARGET PkgConfig::HAMLIB)
    else()
        find_path(HAMLIB_INCLUDE_DIR NAMES hamlib/rig.h)
        find_library(HAMLIB_LIBRARY NAMES hamlib libhamlib)
        if(NOT HAMLIB_INCLUDE_DIR OR NOT HAMLIB_LIBRARY)
            message(FATAL_ERROR "hamlib was requested but not found. Install hamlib or configure with -DMERCURYCHAT_USE_HAMLIB=OFF.")
        endif()
        add_library(Hamlib::Hamlib UNKNOWN IMPORTED)
        set_target_properties(Hamlib::Hamlib PROPERTIES
            IMPORTED_LOCATION "${HAMLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${HAMLIB_INCLUDE_DIR}")
        set(MERCURYCHAT_HAMLIB_TARGET Hamlib::Hamlib)
    endif()
endif()

add_library(mercurychat_core STATIC
    src/CatController.cpp
    src/CatController.hpp
    src/ChatProtocol.cpp
    src/ChatProtocol.hpp
    src/TncClient.cpp
    src/TncClient.hpp)

target_include_directories(mercurychat_core PUBLIC src)
target_link_libraries(mercurychat_core PUBLIC Qt6::Core Qt6::Network)

if(MERCURYCHAT_USE_HAMLIB)
    target_compile_definitions(mercurychat_core PUBLIC MERCURYCHAT_WITH_HAMLIB=1)
    target_link_libraries(mercurychat_core PUBLIC "${MERCURYCHAT_HAMLIB_TARGET}")
else()
    target_compile_definitions(mercurychat_core PUBLIC MERCURYCHAT_WITH_HAMLIB=0)
endif()

add_executable(mercury-chat
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.hpp)

target_link_libraries(mercury-chat PRIVATE
    mercurychat_core
    Qt6::Widgets)

enable_testing()

add_executable(mercurychat_tests
    tests/test_chat_protocol.cpp)

target_link_libraries(mercurychat_tests PRIVATE
    mercurychat_core
    Qt6::Core)

add_test(NAME chat_protocol COMMAND mercurychat_tests)

