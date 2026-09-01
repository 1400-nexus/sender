add_library(nexus_test_main INTERFACE)
target_include_directories(nexus_test_main INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

function(nexus_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE nexus_common nexus_test_main)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

nexus_add_test(test_crc)
nexus_add_test(test_wire)
