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
    USE_MONITOR=1
    USE_MOUSE=1
)

# ─── Gedeelde firmware bronbestanden (app/ en ui/) ────────────────────────────
set(FIRMWARE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/../firmware/src)

set(SHARED_SOURCES
    ${FIRMWARE_SRC}/app/app_state.c
    ${FIRMWARE_SRC}/app/app_entities.c
    ${FIRMWARE_SRC}/app/app_events.c
    ${FIRMWARE_SRC}/ui/ui_manager.c
    ${FIRMWARE_SRC}/ui/ui_setup.c
    ${FIRMWARE_SRC}/ui/ui_view_menu.c
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
)

add_executable(sensecap_sim ${SHARED_SOURCES} ${SIM_SOURCES})

target_include_directories(sensecap_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}            # lv_conf.h
    ${FIRMWARE_SRC}                        # gedeelde headers
    ${CMAKE_CURRENT_SOURCE_DIR}/mock       # mock headers
)

target_link_libraries(sensecap_sim
    lvgl
    lv_drivers
    SDL2::SDL2
    SDL2::SDL2main
)

# Windows: kopieer SDL2.dll naast het executable
if(WIN32)
    add_custom_command(TARGET sensecap_sim POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL2::SDL2>
            $<TARGET_FILE_DIR:sensecap_sim>
    )
endif()
