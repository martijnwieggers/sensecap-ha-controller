cmake_minimum_required(VERSION 3.20)
project(sensecap_ha_simulator C)
set(CMAKE_C_STANDARD 11)

# ─── LVGL 8.3.3 via FetchContent ──────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v8.3.3
    GIT_SHALLOW    TRUE
)
FetchContent_Declare(lv_drivers
    GIT_REPOSITORY https://github.com/lvgl/lv_drivers.git
    GIT_TAG        v8.3.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(lvgl lv_drivers)

# ─── SDL2 ─────────────────────────────────────────────────────────────────────
# Windows via vcpkg: cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
# Linux/macOS: sudo apt install libsdl2-dev  /  brew install sdl2
find_package(SDL2 REQUIRED)

# ─── lv_conf.h locatie doorgeven ──────────────────────────────────────────────
target_compile_definitions(lvgl PUBLIC
    LV_CONF_PATH=${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
)
target_compile_definitions(lv_drivers PUBLIC
    LV_CONF_PATH=${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
    LV_DRV_CONF_PATH=${CMAKE_CURRENT_SOURCE_DIR}/lv_drv_conf.h
    USE_SDL=1
)
# lv_drivers moet lv_drv_conf.h en SDL2 kunnen vinden
target_include_directories(lv_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(lv_drivers PUBLIC SDL2::SDL2)

# ─── Gedeelde firmware bronbestanden (app/ en ui/) ────────────────────────────
set(FIRMWARE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)

set(SHARED_SOURCES
    ${FIRMWARE_SRC}/app/app_state.c
    ${FIRMWARE_SRC}/app/app_entities.c
    ${FIRMWARE_SRC}/app/app_events.c
    ${FIRMWARE_SRC}/ui/ui_manager.c
    ${FIRMWARE_SRC}/ui/ui_setup.c
    ${FIRMWARE_SRC}/ui/ui_view_menu.c
    ${FIRMWARE_SRC}/ui/ui_view_settings.c
    ${FIRMWARE_SRC}/ui/ui_entities.c
    ${FIRMWARE_SRC}/ui/ui_widgets.c
    ${FIRMWARE_SRC}/ui/ui_status.c
)

# ─── Simulator-specifieke bronnen ─────────────────────────────────────────────
set(SIM_SOURCES
    main.c
    mock/mock_storage.c
    mock/mock_system.c
    mock/mock_ha.c
    mock/mock_wifi.c
    mock/mock_web_config.c
)

add_executable(sensecap_sim ${SHARED_SOURCES} ${SIM_SOURCES})

target_compile_definitions(sensecap_sim PRIVATE
    APP_VERSION="0.1.0-sim"
    SIMULATOR=1
)

target_include_directories(sensecap_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}            # lv_conf.h
    ${FIRMWARE_SRC}                        # gedeelde headers
    ${CMAKE_CURRENT_SOURCE_DIR}/mock       # mock headers
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs      # ESP-IDF / FreeRTOS stubs
    ${lv_drivers_SOURCE_DIR}               # sdl/sdl.h
)

target_link_libraries(sensecap_sim
    lvgl
    lv_drivers
    SDL2::SDL2
    SDL2::SDL2main
    $<$<PLATFORM_ID:Windows>:ws2_32>
)

# Windows: kopieer SDL2.dll naast het executable
if(WIN32)
    add_custom_command(TARGET sensecap_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:sensecap_sim>
    )
endif()
