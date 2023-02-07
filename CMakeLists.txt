cmake_minimum_required(VERSION 3.13)

# We prefer to have all linked submodules at toplevel
# set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/tinyusb)

#include(pico-sdk/pico_sdk_init.cmake)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)

set(PICO_TINYUSB_PATH ${PICO_SDK_PATH}/lib/tinyusb)
project(pico_webserver)
pico_sdk_init()

# LWIP
set(LWIP_DIR ${PICO_EXTRAS_PATH}/lib/lwip)

set (LWIP_INCLUDE_DIRS
    "${LWIP_DIR}/src/include"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
include(${LWIP_DIR}/src/Filelists.cmake)

# Extra stuff from TinyUSB, that is not part of tinyusb_device library
set(TINYUSB_LIBNETWORKING_SOURCES
    ${PICO_TINYUSB_PATH}/lib/networking/dhserver.c
    ${PICO_TINYUSB_PATH}/lib/networking/rndis_reports.c
)

add_executable(${PROJECT_NAME} webserver.cpp tusb_lwip_glue.c usb_descriptors.c ${TINYUSB_LIBNETWORKING_SOURCES})

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 1)

add_subdirectory(W25Q64)
add_subdirectory(include)

add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )
target_link_libraries(${PROJECT_NAME} pico_stdlib 
                                      pico_unique_id 
                                      tinyusb_device 
                                      lwipallapps 
                                      lwipcore  
                                      hardware_adc 
                                      hardware_spi
                                      W25Q64 
                                      filesys)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${LWIP_INCLUDE_DIRS} ${PICO_TINYUSB_PATH}/src ${PICO_TINYUSB_PATH}/lib/networking)


pico_add_extra_outputs(${PROJECT_NAME})
# target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_ENTER_USB_BOOT_ON_EXIT=1)
