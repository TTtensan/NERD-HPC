set(TARGET_NAME NERD_HPC)
add_executable(${TARGET_NAME}
        main.c
        pin.c
        lcd.c
        speaker.c
        sd.cpp
        com_func.c
        draw.c
        font.c
        io.c
        ioexp.c
        ir.c
        mcore.c
        usb_descriptors.c
        usb.c
        basic.cpp
        )

# pull in common dependencies
target_link_libraries(${TARGET_NAME}
    pico_stdlib
    pico_multicore
    hardware_pwm
    hardware_spi
    hardware_i2c
    FatFs_SPI
    tinyusb_device
    tinyusb_board
    )

# Make sure TinyUSB can find tusb_config.h
target_include_directories(${TARGET_NAME} PUBLIC
        ${CMAKE_CURRENT_LIST_DIR})

# create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)
