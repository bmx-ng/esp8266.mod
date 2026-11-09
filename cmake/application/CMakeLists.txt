cmake_minimum_required(VERSION 3.5)

foreach(required_variable BLITZMAX_ESP8266_SDK BLITZMAX_APPLICATION_ROOT BLITZMAX_GENERATED_C BLITZMAX_OUTPUT_NAME BLITZMAX_ESP8266_ARENA_SIZE)
    if (NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} must be supplied by bmk")
    endif()
endforeach()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(${BLITZMAX_OUTPUT_NAME})
