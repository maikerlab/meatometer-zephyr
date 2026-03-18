cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_state_machine)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(app PRIVATE
    test_state_machine.c
    mocks/hal_mock.c
    ../../src/app/state_machine.c
    ../../src/app/measure_temp.c
    # measure_thread.c bewusst NICHT inkludiert — wird gemockt
)
