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
target_link_libraries(viper_race_setup_model PUBLIC viper_legacy_fdiv)
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

if(MSVC)
    target_compile_options(viper_asset_catalog PRIVATE /W4 /permissive-)
    target_compile_options(viper_frontend_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_startup_gates PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_setup_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_loop_model PRIVATE /W4 /permissive-)
    target_compile_options(viper_race_scene_flow PRIVATE /W4 /permissive-)
    target_compile_options(viper_alternate_race_mode PRIVATE /W4 /permissive-)
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
    target_compile_options(viper_reconstruction PRIVATE -Wall -Wextra -Wpedantic)
endif()
