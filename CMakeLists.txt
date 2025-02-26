# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 1.5.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.0.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(metronomo_de_presets C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_executable(metronomo_de_presets
    metronomo_de_presets.c
    inc/ssd1306.c
    )

# Generate PIO header (corrigido)
pico_generate_pio_header(metronomo_de_presets ${CMAKE_CURRENT_LIST_DIR}/ws2818b.pio)

# Modifique as linhas abaixo para habilitar/desabilitar saída via UART/USB (corrigido)
pico_enable_stdio_uart(metronomo_de_presets 0)
pico_enable_stdio_usb(metronomo_de_presets 1)

# pull in common dependencies
target_link_libraries(metronomo_de_presets pico_stdlib
    hardware_pwm
    hardware_pio
    hardware_clocks
    hardware_irq
    hardware_i2c
    hardware_uart
    hardware_adc
    )

if (PICO_CYW43_SUPPORTED)
    target_link_libraries(metronomo_de_presets pico_cyw43_arch_none)
endif()

# create map/bin/hex file etc.
pico_add_extra_outputs(metronomo_de_presets)

# Adicionar diretórios de inclusão corretamente (corrigido)
target_include_directories(metronomo_de_presets PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
)

# add url via pico_set_program_url
