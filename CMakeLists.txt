cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(meatometer)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(app PRIVATE src)

target_sources(app PRIVATE
    src/main.c
    src/fsm/session_fsm.c
    src/fsm/conn_fsm.c
    src/fsm/dispatcher.c
    src/comms/ble_prov.c
    src/comms/wifi_mgr.c
    src/comms/mqtt_mgr.c
    src/hal/hal.c
    src/sensor/dummy.c
    src/sensor/sensor_registry.c
    src/temperature.c
)

zephyr_library_include_directories($<TARGET_PROPERTY:wifi_prov_core,BINARY_DIR>)

target_link_libraries(app PRIVATE wifi_prov_ble)