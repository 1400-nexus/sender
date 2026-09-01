# Lowered from 4.2 so the project still configures inside a Linux container
# (Ubuntu 24.04 ships CMake 3.28). Nothing here needs 4.x.
cmake_minimum_required(VERSION 3.20)
project(sender)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Protobuf REQUIRED)

set(NEXUS_PROTO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/nexus-proto)
set(NEXUS_PROTO_SRC ${NEXUS_PROTO_DIR}/proto)
set(NEXUS_PROTO_GEN ${NEXUS_PROTO_DIR}/generated/cpp)
set(NEXUS_PROTO_NAMES common ipc net)

if(NOT EXISTS ${NEXUS_PROTO_SRC}/net.proto)
    message(FATAL_ERROR
        "nexus-proto not found at ${NEXUS_PROTO_DIR}\n"
        "Run: git submodule update --init --recursive")
endif()

# ---------------------------------------------------------------------------
# Where the C++ bindings come from.
#
# DEFAULT: generate from the .proto sources at build time, into the build
# directory. Each machine generates against its own installed protobuf, so a
# version mismatch is structurally impossible.
#
# The checked-in generated/cpp pins an EXACT runtime (#if PROTOBUF_VERSION !=
# 7036001), which means a 7.36.0 machine cannot build 7.36.1 gencode, and every
# regeneration commit breaks whoever is not on that exact patch version.
# Protobuf itself prints "NO CHECKED-IN PROTOBUF GENCODE" at the top of those
# files for this reason.
#
# Set -DNEXUS_USE_CHECKED_IN_GENCODE=ON to use them anyway (e.g. on a machine
# with no protoc). The exact-version guard below then applies.
# ---------------------------------------------------------------------------
option(NEXUS_USE_CHECKED_IN_GENCODE
       "Use libs/nexus-proto/generated/cpp instead of running protoc" OFF)

set(NEXUS_PROTO_SRCS "")

if(NOT NEXUS_USE_CHECKED_IN_GENCODE AND TARGET protobuf::protoc)
    set(_gen ${CMAKE_CURRENT_BINARY_DIR}/generated/proto)
    file(MAKE_DIRECTORY ${_gen})

    foreach(_n IN LISTS NEXUS_PROTO_NAMES)
        add_custom_command(
            OUTPUT ${_gen}/${_n}.pb.cc ${_gen}/${_n}.pb.h
            COMMAND protobuf::protoc
                    --cpp_out ${_gen}
                    -I ${NEXUS_PROTO_SRC}
                    ${NEXUS_PROTO_SRC}/${_n}.proto
            DEPENDS ${NEXUS_PROTO_SRC}/${_n}.proto protobuf::protoc
            COMMENT "protoc ${_n}.proto"
            VERBATIM)
        list(APPEND NEXUS_PROTO_SRCS ${_gen}/${_n}.pb.cc)
    endforeach()

    set(NEXUS_PROTO_INC ${_gen})
    message(STATUS "nexus-proto: generating C++ with local protoc (${Protobuf_VERSION})")

else()
    # Reads the required version straight out of the header, so regenerating
    # with a different protoc updates this check automatically.
    file(STRINGS ${NEXUS_PROTO_GEN}/net.pb.h _pin REGEX "^#if PROTOBUF_VERSION != ")
    if(_pin)
        list(GET _pin 0 _pin)
        string(REGEX MATCH "[0-9]+" _pin "${_pin}")
        math(EXPR _maj "${_pin} / 1000000")
        math(EXPR _min "(${_pin} / 1000) % 1000")
        math(EXPR _pat "${_pin} % 1000")
        set(_want "${_maj}.${_min}.${_pat}")
        if(Protobuf_VERSION AND NOT Protobuf_VERSION VERSION_EQUAL _want)
            message(FATAL_ERROR
                "protobuf version mismatch.\n"
                "  gencode in nexus-proto was built for : ${_want}\n"
                "  libprotobuf found on this machine    : ${Protobuf_VERSION}\n"
                "Drop -DNEXUS_USE_CHECKED_IN_GENCODE=ON to generate locally "
                "instead, or install protobuf ${_want}.")
        endif()
    endif()

    foreach(_n IN LISTS NEXUS_PROTO_NAMES)
        list(APPEND NEXUS_PROTO_SRCS ${NEXUS_PROTO_GEN}/${_n}.pb.cc)
    endforeach()
    set(NEXUS_PROTO_INC ${NEXUS_PROTO_GEN})
    message(STATUS "nexus-proto: using checked-in gencode")
endif()

# The generated code has to be COMPILED, not just included. Without these .cc
# files you get headers that resolve and undefined symbols at link time.
add_library(nexus_proto STATIC ${NEXUS_PROTO_SRCS})
target_include_directories(nexus_proto PUBLIC ${NEXUS_PROTO_INC})

# protobuf::libprotobuf, NOT ${Protobuf_LIBRARIES}: from protobuf 22 onward the
# runtime depends on Abseil, and only the imported target carries those
# dependencies through.
target_link_libraries(nexus_proto PUBLIC protobuf::libprotobuf)

# ---------------------------------------------------------------------------
# nexus_common - pure bytes. No protobuf, no sockets, no platform assumptions,
# so it unit-tests in milliseconds and can be handed to the receiver verbatim.
# ---------------------------------------------------------------------------
add_library(nexus_common STATIC
        src/common/crc.cpp
        src/common/wire.cpp)

target_include_directories(nexus_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(sender main.cpp)
target_link_libraries(sender PRIVATE nexus_proto nexus_common)

enable_testing()
add_subdirectory(tests)
