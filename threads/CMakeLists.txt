
include(${SEL4_TUTORIALS_DIR}/settings.cmake)
sel4_tutorials_regenerate_tutorial(${CMAKE_CURRENT_SOURCE_DIR})

cmake_minimum_required(VERSION 3.7.2)
project(threads C ASM)

sel4_tutorials_setup_capdl_tutorial_environment()


cdl_pp(${CMAKE_CURRENT_SOURCE_DIR}/.manifest.obj cdl_pp_target
    
    ELF "threads"
    CFILE "${CMAKE_CURRENT_BINARY_DIR}/cspace_threads.c"
    
)


add_executable(threads EXCLUDE_FROM_ALL threads.c cspace_threads.c)
add_dependencies(threads cdl_pp_target)
target_link_libraries(threads sel4tutorials)

list(APPEND elf_files "$<TARGET_FILE:threads>")
list(APPEND elf_targets "threads")




cdl_ld("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec
    MANIFESTS ${CMAKE_CURRENT_SOURCE_DIR}/.allocator.obj
    ELF ${elf_files}
    KEYS ${elf_targets}
    DEPENDS ${elf_targets})

DeclareCDLRootImage("${CMAKE_CURRENT_BINARY_DIR}/spec.cdl" capdl_spec ELF ${elf_files} ELF_DEPENDS ${elf_targets})

set(FINISH_COMPLETION_TEXT "
Failed to retype thread: 2
")
set(START_COMPLETION_TEXT "
Failed to retype thread: 2
")
configure_file(${SEL4_TUTORIALS_DIR}/tools/expect.py ${CMAKE_BINARY_DIR}/check @ONLY)
include(simulation)
GenerateSimulateScript()

