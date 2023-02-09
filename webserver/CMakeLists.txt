add_library(webserver
webserver.cpp tusb_lwip_glue.c usb_descriptors.c ${TINYUSB_LIBNETWORKING_SOURCES}
)

target_include_directories(webserver PUBLIC ${CMAKE_CURRENT_LIST_DIR} PRIVATE ${LWIP_INCLUDE_DIRS} ${PICO_TINYUSB_PATH}/src ${PICO_TINYUSB_PATH}/lib/networking
)


add_compile_options(-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        -Wno-maybe-uninitialized
        )

target_link_libraries(webserver pico_stdlib 
                                      pico_unique_id 
                                      tinyusb_device 
                                      lwipallapps 
                                      lwipcore  
                                      hardware_adc 
                                      hardware_spi
                                      W25Q64 
                                      filesys
                                      )