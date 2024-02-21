cmake_minimum_required (VERSION 3.12)

project (traffic_light)

# Initialize CMake library for KasperskyOS SDK.
include (platform)
initialize_platform (FORCE_STATIC)

# Tools for using NK parser.
include (platform/nk)

include (platform/install)
include (platform/tests)
add_project_tests_main_target ()
include (platform/options)
include (platform/install)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package (vfs REQUIRED)
include_directories (${vfs_INCLUDE})
find_package (rump REQUIRED COMPONENTS DHCPCD_ENTITY)
include_directories (${rump_INCLUDE})
find_package (precompiled_vfs REQUIRED)
include_directories (${precompiled_vfs_INCLUDE})

nk_build_idl_files (traffic_light_idl_files NK_MODULE "traffic_light" IDL "resources/IMode.idl")
nk_build_idl_files (traffic_light_idl_connector_files NK_MODULE "traffic_light" IDL "resources/ConfigurationMessage.idl")
nk_build_cdl_files (traffic_light_cdl_files IDL_TARGET traffic_light_idl_files NK_MODULE "traffic_light" CDL "resources/CMode.cdl")
nk_build_edl_files (traffic_light_lights_gpio_edl_files CDL_TARGET traffic_light_cdl_files NK_MODULE "traffic_light" EDL "resources/LightsGPIO.edl")
nk_build_edl_files (traffic_light_control_system_edl_files DEPENDS traffic_light_idl_connector_files NK_MODULE "traffic_light" EDL "resources/ControlSystem.edl")
nk_build_edl_files (traffic_light_connector_edl_files IDL_TARGET traffic_light_idl_connector_files NK_MODULE "traffic_light" EDL "resources/Connector.edl")

add_subdirectory (control_system)
add_subdirectory (lights_gpio)
add_subdirectory (connector)
add_subdirectory (einit)
