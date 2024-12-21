cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)

project(morningtown C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(DS3231 "Enable DS3231 RTC" OFF)
option(USB_SERIAL "Enable serial console over USB (otherwise via UART)" ON)

# Initialize the SDK
pico_sdk_init()

add_executable(morningtown morningtown.c terminal.c)

if (USB_SERIAL)
  pico_enable_stdio_usb(morningtown ENABLED)
else()
  pico_enable_stdio_uart(morningtown ENABLED)
endif()

if (DS3231)
  target_compile_definitions(morningtown PRIVATE HAVE_DS3231)
  target_link_libraries(morningtown hardware_i2c)
  target_sources(morningtown PRIVATE ds3231.c)
endif()

target_link_libraries(morningtown
                      pico_stdlib
                      hardware_rtc
                      hardware_pwm)

string(COMPARE EQUAL ${PICO_BOARD} "pico_w" _cmp)
if (_cmp)
  target_sources(morningtown PRIVATE ntp_client.c)
  target_link_libraries(morningtown pico_cyw43_arch_lwip_poll)
  target_compile_definitions(morningtown PRIVATE
                             WIFI_SSID=\"${WIFI_SSID}\"
                             WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
                             CYW43_HOST_NAME=\"morningtown\"
                             PICO_W=1)
else()
  target_sources(morningtown PRIVATE ntp_dummy.c)
endif()

pico_add_extra_outputs(morningtown)
target_include_directories(morningtown PRIVATE ${CMAKE_CURRENT_LIST_DIR})
