
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
project(notifications C ASM)

sel4_tutorials_setup_capdl_tutorial_environment()


cdl_pp(${CMAKE_CURRENT_SOURCE_DIR}/.manifest.obj cdl_pp_target
	
    ELF "producer_1"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_producer_1.c"
    
    ELF "producer_2"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_producer_2.c"
    
    ELF "consumer"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_consumer.c"
    
)   


add_executable(producer_1 EXCLUDE_FROM_ALL producer_1.c cspace_producer_1.c)
add_dependencies(producer_1 cdl_pp_target)
target_link_libraries(producer_1 sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:producer_1>")
list(APPEND elf_targets "producer_1")


add_executable(producer_2 EXCLUDE_FROM_ALL producer_2.c cspace_producer_2.c)
add_dependencies(producer_2 cdl_pp_target)
target_link_libraries(producer_2 sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:producer_2>")
list(APPEND elf_targets "producer_2")


add_executable(consumer EXCLUDE_FROM_ALL consumer.c cspace_consumer.c)
add_dependencies(consumer cdl_pp_target)
target_link_libraries(consumer sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:consumer>")
list(APPEND elf_targets "consumer")




cdl_ld("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec 
    MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/.allocator.obj
    ELF ${elf_files}
    KEYS ${elf_targets}
    DEPENDS ${elf_targets})

DeclareCDLRootImage("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec ELF ${elf_files} ELF_DEPENDS ${elf_targets})


set(FINISH_COMPLETION_TEXT "Waiting for producer")
set(START_COMPLETION_TEXT "Waiting for producer")
configure_file(${SEL4_TUTORIALS_DIR}/tools/expect.py ${CMAKE_BINARY_DIR}/check @ONLY)
include(simulation)
GenerateSimulateScript()
