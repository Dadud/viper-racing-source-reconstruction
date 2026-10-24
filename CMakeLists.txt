cmake_minimum_required(VERSION 3.20)

project(viper_racing_reconstruction
    VERSION 0.1.0
    DESCRIPTION "Portable source reconstruction tools for Viper Racing"
    LANGUAGES CXX
)

add_library(viper_asset_catalog src/asset_catalog.cpp)
target_include_directories(viper_asset_catalog PUBLIC include)
target_compile_features(viper_asset_catalog PUBLIC cxx_std_20)

add_library(viper_frontend_model
    src/career_file.cpp
    src/career_progression.cpp
    src/career_season.cpp
    src/config_directory.cpp
    src/game_options.cpp
    src/frontend_initialization.cpp
    src/frontend_dispatch.cpp
    src/replay_load.cpp
    src/replay_format.cpp
    src/replay_save.cpp
    src/replay_screen.cpp
    src/startup_command_line.cpp
)
target_include_directories(viper_frontend_model PUBLIC include)
target_compile_features(viper_frontend_model PUBLIC cxx_std_20)

add_library(viper_startup_gates src/startup_gates.cpp)
target_include_directories(viper_startup_gates PUBLIC include)
target_compile_features(viper_startup_gates PUBLIC cxx_std_20)
target_link_libraries(viper_startup_gates PUBLIC viper_frontend_model)

add_library(viper_race_setup_model
    src/race_setup.cpp
    src/race_participants.cpp
    src/human_car_setup.cpp
    src/single_race_setup_screen.cpp
)
target_include_directories(viper_race_setup_model PUBLIC include)
target_compile_features(viper_race_setup_model PUBLIC cxx_std_20)

add_library(viper_race_loop_model src/race_loop_callbacks.cpp)
target_sources(viper_race_loop_model PRIVATE src/race_camera_control.cpp)
target_include_directories(viper_race_loop_model PUBLIC include)
target_compile_features(viper_race_loop_model PUBLIC cxx_std_20)

add_library(viper_alternate_race_mode src/alternate_race_mode.cpp)
target_include_directories(viper_alternate_race_mode PUBLIC include)
target_compile_features(viper_alternate_race_mode PUBLIC cxx_std_20)

add_library(viper_resource_bundle src/resource_bundle.cpp)
target_include_directories(viper_resource_bundle PUBLIC include)
target_compile_features(viper_resource_bundle PUBLIC cxx_std_20)

add_library(viper_sound_resource
    src/sound_resource.cpp
    src/engine_sound_setup.cpp)
target_include_directories(viper_sound_resource PUBLIC include)
target_compile_features(viper_sound_resource PUBLIC cxx_std_20)

add_library(viper_ai_resources
    src/ai_driver_resource.cpp
    src/ai_notes_resource.cpp
)
target_include_directories(viper_ai_resources PUBLIC include)
target_compile_features(viper_ai_resources PUBLIC cxx_std_20)

add_library(viper_model_resource src/model_resource.cpp)
target_include_directories(viper_model_resource PUBLIC include)
target_compile_features(viper_model_resource PUBLIC cxx_std_20)

add_library(viper_vehicle_model src/vehicle_setup.cpp)
target_include_directories(viper_vehicle_model PUBLIC include)
target_compile_features(viper_vehicle_model PUBLIC cxx_std_20)

add_library(viper_terrain_model src/terrain_surface.cpp)
target_include_directories(viper_terrain_model PUBLIC include)
target_compile_features(viper_terrain_model PUBLIC cxx_std_20)
target_link_libraries(viper_terrain_model PUBLIC viper_resource_bundle)
target_link_libraries(viper_vehicle_model PUBLIC viper_terrain_model)

add_library(viper_scene_model
    src/entity_update_dispatch.cpp
    src/scene_car_update.cpp
    src/scene_renderer_dispatch.cpp
    src/scene_overlay.cpp
    src/world_track_objects.cpp
)
target_include_directories(viper_scene_model PUBLIC include)
target_compile_features(viper_scene_model PUBLIC cxx_std_20)

add_library(viper_race_scene_flow src/race_scene_render_flow.cpp)
target_include_directories(viper_race_scene_flow PUBLIC include)
target_compile_features(viper_race_scene_flow PUBLIC cxx_std_20)
target_link_libraries(viper_race_scene_flow PUBLIC
    viper_race_loop_model
    viper_scene_model
    viper_graphics_startup
)

add_library(viper_matrix_quaternion src/matrix_quaternion.cpp)
target_include_directories(viper_matrix_quaternion PUBLIC include)
target_compile_features(viper_matrix_quaternion PUBLIC cxx_std_20)

add_library(viper_pcar_model src/pcar_control_update.cpp)
target_include_directories(viper_pcar_model PUBLIC include)
target_compile_features(viper_pcar_model PUBLIC cxx_std_20)

add_library(viper_update_curve src/update_curve.cpp)
target_include_directories(viper_update_curve PUBLIC include)
target_compile_features(viper_update_curve PUBLIC cxx_std_20)

add_library(viper_language_resource src/language_resource.cpp)
target_include_directories(viper_language_resource PUBLIC include)
target_compile_features(viper_language_resource PUBLIC cxx_std_20)
target_link_libraries(viper_frontend_model PUBLIC viper_language_resource)
target_link_libraries(viper_scene_model PUBLIC viper_language_resource)

add_library(viper_ncar_model src/ncar_state.cpp)
target_include_directories(viper_ncar_model PUBLIC include)
target_compile_features(viper_ncar_model PUBLIC cxx_std_20)
target_link_libraries(viper_vehicle_model PUBLIC viper_ncar_model)

add_library(viper_wheel_body_coupling src/wheel_body_coupling.cpp)
target_include_directories(viper_wheel_body_coupling PUBLIC include)
target_compile_features(viper_wheel_body_coupling PUBLIC cxx_std_20)
target_link_libraries(viper_wheel_body_coupling PUBLIC
    viper_vehicle_model
    viper_ncar_model
)

add_library(viper_legacy_allocator src/legacy_allocator.cpp)
target_include_directories(viper_legacy_allocator PUBLIC include)
target_compile_features(viper_legacy_allocator PUBLIC cxx_std_20)

add_library(viper_legacy_timer src/legacy_timer.cpp)
target_include_directories(viper_legacy_timer PUBLIC include)
target_compile_features(viper_legacy_timer PUBLIC cxx_std_20)

add_library(viper_legacy_fdiv src/legacy_fdiv.cpp)
target_include_directories(viper_legacy_fdiv PUBLIC include)
target_compile_features(viper_legacy_fdiv PUBLIC cxx_std_20)
target_link_libraries(viper_update_curve PUBLIC viper_legacy_fdiv)
target_link_libraries(viper_terrain_model PUBLIC viper_legacy_fdiv)
target_link_libraries(viper_race_setup_model PUBLIC viper_legacy_fdiv viper_ai_resources)
target_link_libraries(viper_scene_model PUBLIC viper_legacy_fdiv)
target_link_libraries(viper_ncar_model PUBLIC viper_legacy_fdiv)
target_link_libraries(viper_race_loop_model PUBLIC viper_legacy_fdiv)

add_library(viper_render_context src/render_context.cpp)
target_include_directories(viper_render_context PUBLIC include)
target_compile_features(viper_render_context PUBLIC cxx_std_20)

add_library(viper_graphics_startup src/graphics_startup.cpp)
target_include_directories(viper_graphics_startup PUBLIC include)
target_compile_features(viper_graphics_startup PUBLIC cxx_std_20)
target_link_libraries(viper_graphics_startup PUBLIC viper_render_context)
target_link_libraries(viper_race_loop_model PUBLIC viper_graphics_startup)
target_link_libraries(viper_scene_model PUBLIC viper_render_context)
target_link_libraries(viper_race_setup_model PUBLIC viper_scene_model)

add_library(viper_physics_scheduler src/physics_scheduler.cpp)
target_sources(viper_physics_scheduler PRIVATE
    src/physics_task_factory.cpp
    src/acar_curve_tracker.cpp
    src/acar_driver_ai.cpp
)
target_include_directories(viper_physics_scheduler PUBLIC include)
target_compile_features(viper_physics_scheduler PUBLIC cxx_std_20)
target_link_libraries(viper_physics_scheduler PUBLIC viper_update_curve)

add_library(viper_timed_event_queue src/timed_event_queue.cpp)
target_include_directories(viper_timed_event_queue PUBLIC include)
target_compile_features(viper_timed_event_queue PUBLIC cxx_std_20)
target_link_libraries(viper_ncar_model PUBLIC
    viper_timed_event_queue
    viper_matrix_quaternion
)

add_executable(viper_reconstruction src/main.cpp)
target_link_libraries(viper_reconstruction PRIVATE
    viper_race_scene_flow
    viper_asset_catalog
    viper_frontend_model
    viper_resource_bundle
    viper_pcar_model
    viper_scene_model
    viper_graphics_startup
    viper_matrix_quaternion
    viper_update_curve
    viper_language_resource
    viper_vehicle_model
    viper_terrain_model
)
target_compile_features(viper_reconstruction PRIVATE cxx_std_20)

add_library(viper_race_progression src/race_progression.cpp)
target_include_directories(viper_race_progression PUBLIC include)
target_compile_features(viper_race_progression PUBLIC cxx_std_20)

if(WIN32)
    add_executable(viper_game src/game_main.cpp src/game_assets.cpp)
    target_compile_features(viper_game PRIVATE cxx_std_20)
    target_compile_definitions(viper_game PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    if(MSVC)
        target_compile_options(viper_game PRIVATE /W4 /permissive- /utf-8)
    endif()
    target_link_libraries(viper_game PRIVATE
        viper_race_progression
        viper_ai_resources viper_resource_bundle viper_model_resource
        viper_sound_resource
        viper_language_resource viper_terrain_model
        viper_vehicle_model viper_frontend_model
        viper_scene_model viper_race_setup_model d3d11 dxgi dsound user32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(viper_race_progression_regression
        tests/race_progression_regression.cpp)
    target_link_libraries(viper_race_progression_regression PRIVATE
        viper_race_progression)
    add_test(NAME race_progression_regression
        COMMAND viper_race_progression_regression)
    add_executable(viper_game_options_regression tests/game_options_regression.cpp)
    target_link_libraries(viper_game_options_regression PRIVATE viper_frontend_model)
    add_test(NAME game_options_regression COMMAND viper_game_options_regression)
    add_executable(viper_renderer_regression tests/renderer_regression.cpp)
    target_link_libraries(viper_renderer_regression PRIVATE viper_scene_model)
    add_test(NAME renderer_regression COMMAND viper_renderer_regression)
    add_executable(viper_track_table_regression tests/track_table_regression.cpp)
    target_link_libraries(viper_track_table_regression PRIVATE
        viper_scene_model viper_resource_bundle)
    add_test(NAME track_table_regression COMMAND viper_track_table_regression)
    add_test(NAME inspection_help COMMAND viper_reconstruction --help)
    if(WIN32)
        add_test(NAME game_help COMMAND viper_game --help)
        set(VIPER_EXTERNAL_DATA_DIR "${CMAKE_SOURCE_DIR}/../game_install/GameData")
        if(EXISTS "${VIPER_EXTERNAL_DATA_DIR}/common.res")
            add_test(NAME game_original_asset_validation
                COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}" --validate-assets)
            add_test(NAME game_warp_render_smoke
                COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}" --software --smoke-frames 3)
            add_test(NAME game_trace_replay_regression
                COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
                    "${CMAKE_SOURCE_DIR}/tests/trace_replay_regression.ps1"
                    -GameExe "$<TARGET_FILE:viper_game>"
                    -DataDir "${VIPER_EXTERNAL_DATA_DIR}")
            set_tests_properties(game_trace_replay_regression PROPERTIES TIMEOUT 120)
            set(VIPER_SHIPPED_TRACKS
                bemidji dundas hastings heaven kenyon limbo nfield uptown)
            set(VIPER_SHIPPED_CARS viper exotic plane sedan sports)
            foreach(track IN LISTS VIPER_SHIPPED_TRACKS)
                add_test(NAME game_track_${track}_asset_validation
                    COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}"
                        --track "${track}" --car viper --validate-assets)
                add_test(NAME game_track_${track}_warp_smoke
                    COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}"
                        --track "${track}" --car viper --software --smoke-frames 3)
                set_tests_properties(game_track_${track}_asset_validation
                    game_track_${track}_warp_smoke PROPERTIES TIMEOUT 30)
            endforeach()
            foreach(car IN LISTS VIPER_SHIPPED_CARS)
                add_test(NAME game_car_${car}_asset_validation
                    COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}"
                        --track bemidji --car "${car}" --validate-assets)
                add_test(NAME game_car_${car}_warp_smoke
                    COMMAND viper_game --data-dir "${VIPER_EXTERNAL_DATA_DIR}"
                        --track bemidji --car "${car}" --software --smoke-frames 3)
                set_tests_properties(game_car_${car}_asset_validation
                    game_car_${car}_warp_smoke PROPERTIES TIMEOUT 30)
            endforeach()
            set_tests_properties(game_original_asset_validation game_warp_render_smoke
                PROPERTIES TIMEOUT 30)
        endif()
    endif()
    add_executable(viper_model_resource_regression tests/model_resource_regression.cpp)
    target_link_libraries(viper_model_resource_regression PRIVATE viper_model_resource)
    add_test(NAME model_resource_regression COMMAND viper_model_resource_regression)
    add_executable(viper_ai_resources_regression tests/ai_resources_regression.cpp)
    target_link_libraries(viper_ai_resources_regression PRIVATE viper_ai_resources)
    add_test(NAME ai_resources_regression COMMAND viper_ai_resources_regression)
    add_executable(viper_race_participants_regression
        tests/race_participants_regression.cpp)
    target_link_libraries(viper_race_participants_regression PRIVATE
        viper_race_setup_model)
    add_test(NAME race_participants_regression
        COMMAND viper_race_participants_regression)
    add_executable(viper_car_runtime_setup_regression
        tests/car_runtime_setup_regression.cpp)
    target_link_libraries(viper_car_runtime_setup_regression PRIVATE
        viper_scene_model viper_resource_bundle)
    add_test(NAME car_runtime_setup_regression
        COMMAND viper_car_runtime_setup_regression)
    add_executable(viper_human_car_setup_regression
        tests/human_car_setup_regression.cpp)
    target_link_libraries(viper_human_car_setup_regression PRIVATE
        viper_vehicle_model)
    add_test(NAME human_car_setup_regression
        COMMAND viper_human_car_setup_regression)
    add_executable(viper_sound_resource_regression
        tests/sound_resource_regression.cpp)
    target_link_libraries(viper_sound_resource_regression PRIVATE
        viper_sound_resource viper_resource_bundle)
    add_test(NAME sound_resource_regression COMMAND viper_sound_resource_regression)
    if(WIN32 AND EXISTS "${VIPER_EXTERNAL_DATA_DIR}/common.res")
        add_test(NAME sound_resource_shipped_data
            COMMAND viper_sound_resource_regression "${VIPER_EXTERNAL_DATA_DIR}")
    endif()
endif()

if(MSVC)
    target_compile_options(viper_asset_catalog PRIVATE /W4 /permissive-)
    target_compile_options(viper_frontend_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_startup_gates PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_setup_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_loop_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_scene_flow PRIVATE /W4 /permissive-)
    target_compile_options(viper_alternate_race_mode PRIVATE /W4 /permissive-)
    target_compile_options(viper_ai_resources PRIVATE /W4 /permissive-)
    target_compile_options(viper_vehicle_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_terrain_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_scene_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_matrix_quaternion PRIVATE /W4 /permissive-)
    target_compile_options(viper_pcar_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_update_curve PRIVATE /W4 /permissive-)
    target_compile_options(viper_ncar_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_legacy_allocator PRIVATE /W4 /permissive-)
    target_compile_options(viper_legacy_timer PRIVATE /W4 /permissive-)
    target_compile_options(viper_legacy_fdiv PRIVATE /W4 /permissive-)
    target_compile_options(viper_render_context PRIVATE /W4 /permissive-)
    target_compile_options(viper_graphics_startup PRIVATE /W4 /permissive-)
    target_compile_options(viper_physics_scheduler PRIVATE /W4 /permissive-)
    target_compile_options(viper_timed_event_queue PRIVATE /W4 /permissive-)
    target_compile_options(viper_resource_bundle PRIVATE /W4 /permissive-)
    target_compile_options(viper_model_resource PRIVATE /W4 /permissive-)
    target_compile_options(viper_reconstruction PRIVATE /W4 /permissive-)
else()
    target_compile_options(viper_asset_catalog PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_frontend_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_startup_gates PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_race_setup_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_race_loop_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_race_scene_flow PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_alternate_race_mode PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_vehicle_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_terrain_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_scene_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_matrix_quaternion PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_pcar_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_update_curve PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_ncar_model PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_legacy_allocator PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_legacy_timer PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_render_context PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_graphics_startup PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_physics_scheduler PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_timed_event_queue PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_resource_bundle PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_model_resource PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(viper_reconstruction PRIVATE -Wall -Wextra -Wpedantic)
endif()
