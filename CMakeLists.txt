cmake_minimum_required(VERSION 3.10)
project(实体沙箱 VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

include_directories(${CMAKE_SOURCE_DIR}/include)

file(GLOB SOURCES "src/*.cpp")

add_executable(实体沙箱 ${SOURCES})

target_link_libraries(实体沙箱 PRIVATE OpenGL::GL)