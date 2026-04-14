cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(test_state_machine)

target_include_directories(app PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(app PRIVATE
    test_session_fsm.c
    test_conn_fsm.c
    mocks/hal_mock.c
    mocks/mqtt_mock.c
    mocks/network_mock.c
    mocks/ble_prov_mock.c
    mocks/sensor_mock.c
    mocks/sensor_registry_mock.c
    ../../src/fsm/session_fsm.c
    ../../src/fsm/conn_fsm.c
    mocks/temp_mock.c
)
