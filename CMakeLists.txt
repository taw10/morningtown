cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(morningtown C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(morningtown morningtown.c ntp_client.c)

# For verbose debugging, uncomment this line and see ntp_client.h
#pico_enable_stdio_usb(morningtown ENABLED)

target_link_libraries(morningtown
                      pico_stdlib
                      hardware_rtc
                      pico_cyw43_arch_lwip_threadsafe_background)

pico_add_extra_outputs(morningtown)

target_compile_definitions(morningtown PRIVATE
                           WIFI_SSID=\"${WIFI_SSID}\"
                           WIFI_PASSWORD=\"${WIFI_PASSWORD}\")

target_include_directories(morningtown PRIVATE ${CMAKE_CURRENT_LIST_DIR})
