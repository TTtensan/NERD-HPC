set(TARGET_NAME NERD_BOY)
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
        basic.cpp
        )

# pull in common dependencies
target_link_libraries(${TARGET_NAME}
    pico_stdlib
    hardware_pwm
    hardware_spi
    FatFs_SPI
    )

# create map/bin/hex file etc.
pico_add_extra_outputs(${TARGET_NAME})

pico_enable_stdio_usb(${TARGET_NAME} 1)
pico_enable_stdio_uart(${TARGET_NAME} 0)
