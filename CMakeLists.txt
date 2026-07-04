cmake_minimum_required(VERSION 3.25...4.1)

project(Engine 
    VERSION 0.1.0 
    DESCRIPTION "a modular engine"
    LANGUAGES CXX)

option(ENGINE_WARNINGS_AS_ERRORS "treat compiler warnings as errors" ON)
option(ENGINE_SANITIZE "enable sanitizers" OFF)
option(ENGINE_SHARED "engine modules as dll" OFF)

option(ENGINE_BUILD_RENDERING "build rendering module" ON)
option(ENGINE_BUILD_SANDBOX "build sandbox" ON)
option(ENGINE_BUILD_TESTS "build tests" ON)


include(cmake/Sanitizers.cmake)
include(cmake/CompilerWarnings.cmake)
include(cmake/EngineModule.cmake)

add_subdirectory(modules/core)

if(ENGINE_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(ENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()