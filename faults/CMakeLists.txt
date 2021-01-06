
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
# declare the fault-handlers CMake project and the languages it is written in
project(fault-handlers C ASM)

sel4_tutorials_setup_capdl_tutorial_environment()


cdl_pp(${CMAKE_CURRENT_SOURCE_DIR}/.manifest.obj cdl_pp_target
	
    ELF "faulter"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_faulter.c"
    
    ELF "handler"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_handler.c"
    
)   


add_executable(faulter EXCLUDE_FROM_ALL faulter.c cspace_faulter.c)
add_dependencies(faulter cdl_pp_target)
target_link_libraries(faulter sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:faulter>")
list(APPEND elf_targets "faulter")


add_executable(handler EXCLUDE_FROM_ALL handler.c cspace_handler.c)
add_dependencies(handler cdl_pp_target)
target_link_libraries(handler sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:handler>")
list(APPEND elf_targets "handler")




cdl_ld("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec 
    MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/.allocator.obj
    ELF ${elf_files}
    KEYS ${elf_targets}
    DEPENDS ${elf_targets})

DeclareCDLRootImage("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec ELF ${elf_files} ELF_DEPENDS ${elf_targets})


set(FINISH_COMPLETION_TEXT "Successfully copied")
set(START_COMPLETION_TEXT "Failed to mint ep cap with badge")
configure_file(${SEL4_TUTORIALS_DIR}/tools/expect.py ${CMAKE_BINARY_DIR}/check @ONLY)
include(simulation)
GenerateSimulateScript()
