
#
# Copyright 2018, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#
include(${SEL4_TUTORIALS_DIR}/settings.cmake)
sel4_tutorials_regenerate_tutorial(${CMAKE_CURRENT_SOURCE_DIR})

cmake_minimum_required(VERSION 3.7.2)
project(ipc C ASM)

sel4_tutorials_setup_capdl_tutorial_environment()


cdl_pp(${CMAKE_CURRENT_SOURCE_DIR}/.manifest.obj cdl_pp_target
	
    ELF "client_1"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_client_1.c"
    
    ELF "client_2"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_client_2.c"
    
    ELF "server"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_server.c"
    
)   


add_executable(client_1 EXCLUDE_FROM_ALL client_1.c cspace_client_1.c)
add_dependencies(client_1 cdl_pp_target)
target_link_libraries(client_1 sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:client_1>")
list(APPEND elf_targets "client_1")


add_executable(client_2 EXCLUDE_FROM_ALL client_2.c cspace_client_2.c)
add_dependencies(client_2 cdl_pp_target)
target_link_libraries(client_2 sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:client_2>")
list(APPEND elf_targets "client_2")


add_executable(server EXCLUDE_FROM_ALL server.c cspace_server.c)
add_dependencies(server cdl_pp_target)
target_link_libraries(server sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:server>")
list(APPEND elf_targets "server")




cdl_ld("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec 
    MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/.allocator.obj
    ELF ${elf_files}
    KEYS ${elf_targets}
    DEPENDS ${elf_targets})

DeclareCDLRootImage("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec ELF ${elf_files} ELF_DEPENDS ${elf_targets})


set(FINISH_COMPLETION_TEXT "Assertion failed")
set(START_COMPLETION_TEXT "Assertion failed")
configure_file(${SEL4_TUTORIALS_DIR}/tools/expect.py ${CMAKE_BINARY_DIR}/check @ONLY)
include(simulation)
GenerateSimulateScript()
