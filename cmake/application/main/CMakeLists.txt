set(ESP8266_MODULE_ROOT "${BLITZMAX_ESP8266_SDK}/mod/esp8266.mod")
set(EMBEDDED_MODULE_ROOT "${BLITZMAX_ESP8266_SDK}/mod/embedded.mod")
set(BRL_BLITZ_ROOT "${BLITZMAX_ESP8266_SDK}/mod/brl.mod/blitz.mod")
set(GENERATED_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GENERATED_BOOTSTRAP_C "${GENERATED_DIRECTORY}/blitzmax_esp8266_modules.c")
set(GENERATED_ENTRY_C "${GENERATED_DIRECTORY}/blitzmax_esp8266_entry.c")

file(MAKE_DIRECTORY "${GENERATED_DIRECTORY}")
file(WRITE "${GENERATED_BOOTSTRAP_C}" "/* Generated ESP8266 module initialization order. */\n")
foreach(module_initializer IN LISTS BLITZMAX_ESP8266_MODULE_INITIALIZERS)
    file(APPEND "${GENERATED_BOOTSTRAP_C}" "extern int ${module_initializer}(void);\n")
endforeach()
file(APPEND "${GENERATED_BOOTSTRAP_C}" "void bmx_embedded_modules_init(void) {\n")
foreach(module_initializer IN LISTS BLITZMAX_ESP8266_MODULE_INITIALIZERS)
    file(APPEND "${GENERATED_BOOTSTRAP_C}" "    (void)${module_initializer}();\n")
endforeach()
file(APPEND "${GENERATED_BOOTSTRAP_C}" "}\n")
file(WRITE "${GENERATED_ENTRY_C}" "extern int main(void);\nvoid app_main(void) { (void)main(); }\n")

if (DEFINED BLITZMAX_ESP8266_NATIVE_CMAKE)
    include("${BLITZMAX_ESP8266_NATIVE_CMAKE}")
endif()

set(BLITZMAX_ESP8266_SOURCES
    ${BLITZMAX_GENERATED_C}
    ${BLITZMAX_ESP8266_GENERATED_SOURCES}
    ${BLITZMAX_PICO_NATIVE_SOURCES}
    "${GENERATED_BOOTSTRAP_C}"
    "${GENERATED_ENTRY_C}"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_runtime.c"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_string_integer.c"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_string_float.cpp"
    "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/embedded_unicode.c"
    "${ESP8266_MODULE_ROOT}/runtime.mod/native/esp8266_events.c"
    "${ESP8266_MODULE_ROOT}/runtime.mod/native/esp8266_gpio.c"
    "${ESP8266_MODULE_ROOT}/runtime.mod/native/esp8266_runtime.c"
    "${BRL_BLITZ_ROOT}/ryu/ryu/f2s.c"
    "${BRL_BLITZ_ROOT}/ryu/ryu/d2s.c")

idf_component_register(
    SRCS ${BLITZMAX_ESP8266_SOURCES}
    INCLUDE_DIRS
        "${EMBEDDED_MODULE_ROOT}/runtime.mod/native/include"
        "${ESP8266_MODULE_ROOT}/runtime.mod/native/include"
        "${BLITZMAX_ESP8266_SDK}/mod"
        "${BLITZMAX_APPLICATION_ROOT}"
        "${BRL_BLITZ_ROOT}"
        "${BRL_BLITZ_ROOT}/ryu"
    REQUIRES esp8266 freertos newlib pthread)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    BMX_ESP8266=1
    BMX_EMBEDDED_ARENA_SIZE=${BLITZMAX_ESP8266_ARENA_SIZE}
    BMX_EMBEDDED_ARENA_IN_PSRAM=0)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-function -Wno-unused-variable)
