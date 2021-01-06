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
project(interrupts C ASM)

sel4_tutorials_setup_capdl_tutorial_environment()


cdl_pp(${CMAKE_CURRENT_SOURCE_DIR}/.manifest.obj cdl_pp_target
	
    ELF "client"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_client.c"
    
    ELF "timer"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_timer.c"
    
)



add_executable(client EXCLUDE_FROM_ALL client.c cspace_client.c)

add_dependencies(client cdl_pp_target)
target_link_libraries(client sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:client>")
list(APPEND elf_targets "client")



add_executable(timer EXCLUDE_FROM_ALL timer.c cspace_timer.c ${SEL4_TUTORIALS_DIR}/zynq_timer_driver/src/driver.c)
target_include_directories(timer PUBLIC ${SEL4_TUTORIALS_DIR}/zynq_timer_driver/include)

add_dependencies(timer cdl_pp_target)
target_link_libraries(timer sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:timer>")
list(APPEND elf_targets "timer")




cdl_ld("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec
    MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/.allocator.obj
    ELF ${elf_files}
    KEYS ${elf_targets}
    DEPENDS ${elf_targets})

DeclareCDLRootImage("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec ELF ${elf_files} ELF_DEPENDS ${elf_targets})


set(FINISH_COMPLETION_TEXT "
	Failed to ack irq")
set(START_COMPLETION_TEXT "
	Failed to ack irq")
configure_file(${SEL4_TUTORIALS_DIR}/tools/expect.py ${CMAKE_BINARY_DIR}/check @ONLY)
include(simulation)
GenerateSimulateScript()
