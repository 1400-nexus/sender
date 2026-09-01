cmake_minimum_required(VERSION 4.2)
project(sender)

set(CMAKE_CXX_STANDARD 20)

add_executable(sender main.cpp)

find_package(Protobuf REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/libs/nexus-proto/generated/cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE ${Protobuf_LIBRARIES})