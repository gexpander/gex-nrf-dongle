cmake_minimum_required(VERSION 3.9)
project(GEX_NRF)

set(CMAKE_CXX_STANDARD 11)

add_definitions(
        -DUSE_HAL_DRIVER=1
        -DSTM32F103xB
        -D__weak=__attribute__\(\(weak\)\)
        -D__packed=__attribute__\(\(__packed__\)\)
        -D__COUNTER__=__LINE__
        -DUSBD_SUPPORT_USER_STRING=1
        -DUSE_FULL_ASSERT=1
        -DUSE_FULL_LL_DRIVER=1
)

FILE(GLOB_RECURSE SOURCE_FILES
        Inc/*.h
        Drivers/*.c
        Drivers/*.h
        Middlewares/*.c
        Middlewares/*.h
        Src/*.c
        Src/*.h
        )

include_directories(
        # System includes folder
        /usr/arm-none-eabi/include/

        # CMSIS + HAL
        Drivers/CMSIS/Include
        Drivers/CMSIS/Device/ST/STM32F1xx/Include
        Drivers/STM32F1xx_HAL_Driver/Inc
        Drivers/STM32F1xx_HAL_Driver/Inc/Legacy

        # USB Library
        Middlewares/ST/STM32_USB_Device_Library/Class/CDC/Inc
        Middlewares/ST/STM32_USB_Device_Library/Core/Inc

        Inc
)

add_executable(main ${SOURCE_FILES})
