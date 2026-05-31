cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0087 NEW)
cmake_policy(SET CMP0091 NEW) # select MSVC runtime library through `CMAKE_MSVC_RUNTIME_LIBRARY`

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(chatterinotify VERSION 0.1.0)

set(MAJOR_QT_VERSION "6")
find_package(Qt${MAJOR_QT_VERSION} REQUIRED COMPONENTS Core)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
