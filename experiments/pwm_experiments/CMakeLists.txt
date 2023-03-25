# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/enno/Dokumente/pico/pico-sdk")

set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(pwm_experiments C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pwm_experiments pwm_experiments.c )

pico_set_program_name(pwm_experiments "pwm_experiments")
pico_set_program_version(pwm_experiments "0.1")

pico_enable_stdio_uart(pwm_experiments 0)
pico_enable_stdio_usb(pwm_experiments 1)

# Add the standard library to the build
target_link_libraries(pwm_experiments
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pwm_experiments PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(pwm_experiments
        hardware_pwm
        )

pico_add_extra_outputs(pwm_experiments)

