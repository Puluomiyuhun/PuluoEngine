cmake_minimum_required(VERSION 3.20)
project(PuluoEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Global options
option(PULUO_BUILD_EDITOR   "Build editor application"  OFF)
option(PULUO_BUILD_SANDBOX  "Build sandbox application"  ON)
option(PULUO_BUILD_TESTS    "Build unit tests"           OFF)

# Dependencies
include(cmake/Dependencies.cmake)

# Engine modules
add_subdirectory(engine)

# Applications
if(PULUO_BUILD_SANDBOX)
    add_subdirectory(sandbox)
endif()

if(PULUO_BUILD_EDITOR)
    add_subdirectory(editor)
endif()

if(PULUO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
