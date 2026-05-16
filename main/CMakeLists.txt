set(EI_SDK_DIR "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk")
set(EI_MODEL_DIR "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/tflite-model")

file(GLOB_RECURSE EI_SDK_CPP_SRCS "${EI_SDK_DIR}/*.cpp")
file(GLOB_RECURSE EI_SDK_CC_SRCS "${EI_SDK_DIR}/*.cc")
file(GLOB_RECURSE EI_SDK_C_SRCS "${EI_SDK_DIR}/*.c")
file(GLOB_RECURSE EI_SDK_ASM_SRCS "${EI_SDK_DIR}/*.S")
file(GLOB_RECURSE EI_SDK_ASM_LOWER_SRCS "${EI_SDK_DIR}/*.s")
file(GLOB_RECURSE EI_MODEL_SRCS "${EI_MODEL_DIR}/*.cpp")

set(EI_EXCLUDE_DIRS "porting/android" "porting/arduino" "porting/brickml"
    "porting/ceva-npn" "porting/clib" "porting/himax-we2"
    "porting/infineon-psoc62" "porting/iar" "porting/mbed"
    "porting/mingw32" "porting/particle" "porting/posix"
    "porting/raspberry" "porting/renesas-ra" "porting/seeed-vision-ai"
    "porting/silabs" "porting/sony" "porting/stm32-cubeai"
    "porting/synaptics" "porting/ti" "porting/zephyr"
    "porting/ambiq" "porting/ethos-core-driver")

set(EI_FILTERED_SRCS "")
foreach(src ${EI_SDK_CPP_SRCS} ${EI_SDK_CC_SRCS} ${EI_SDK_C_SRCS} ${EI_SDK_ASM_SRCS} ${EI_SDK_ASM_LOWER_SRCS})
    set(excluded FALSE)
    foreach(excl_dir ${EI_EXCLUDE_DIRS})
        string(FIND "${src}" "${excl_dir}" pos)
        if(NOT pos EQUAL -1)
            set(excluded TRUE)
            break()
        endif()
    endforeach()
    if(NOT excluded)
        list(APPEND EI_FILTERED_SRCS "${src}")
    endif()
endforeach()

list(APPEND EI_FILTERED_SRCS "${EI_SDK_DIR}/tensorflow/lite/c/common.c")

idf_component_register(
    SRCS
        "main.c"
        "core/app_state.c"
        "core/task_manager.c"
        "core/event_bus.c"
        "driver/driver_as7341.c"
        "driver/driver_scd41.c"
        "driver/driver_button.c"
        "driver/ei_classifier.cpp"
        "driver/driver_i2c_soft.c"
        "driver/driver_ssd1306.c"
        "storage/storage.c"
        "network/wifi_sta.c"
        "network/mqtt_client.c"
        "network/provision/prov_main.c"
        "network/provision/prov_http.c"
        "network/provision/prov_dns.c"
        "service/sensor_service.c"
        "service/ai_service.c"
        "service/ha_integration.c"
        "service/notify.c"
        "service/display_service.c"
        "system/menu_system.c"
        "system/menu_display.c"
        "system/menu_actions.c"
        "system/system_log.c"
        "tasks/task_provision.c"
        "tasks/task_network.c"
        "tasks/task_sensor.c"
        "tasks/task_ai.c"
        "tasks/task_auto_network.c"
        ${EI_FILTERED_SRCS}
        ${EI_MODEL_SRCS}
    INCLUDE_DIRS
        "."
        "./core"
        "./driver"
        "./storage"
        "./network"
        "./network/provision"
        "./service"
        "./system"
        "./tasks"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk/porting"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk/porting/espressif"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk/porting/espressif/ESP-NN/include"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk/porting/espressif/esp-dsp/modules/common/include"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/edge-impulse-sdk/porting/espressif/esp-dsp/modules/fft/include"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/model-parameters"
        "${CMAKE_CURRENT_LIST_DIR}/zsh2451-project-1-cpp-mcu-v3-impulse-#4/tflite-model"
    REQUIRES
        esp_driver_ledc
        esp_driver_gpio
        app_trace
        nvs_flash
        esp_event
        esp_http_client
        esp_http_server
        lwip
        esp_netif
        esp_wifi
        driver
        esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -Wno-maybe-uninitialized
    -Wno-missing-field-initializers
    -Wno-deprecated-declarations
    -Wno-error=format=
)
