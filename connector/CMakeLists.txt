project (Connector)

# Tools for using NK parser.
include (platform/nk)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

add_executable (Connector "src/connector.c")
add_dependencies (Connector traffic_light_connector_edl_files)

target_link_libraries (Connector rump::dhcpcd_client ${vfs_CLIENT_LIB})

set_target_properties (Connector PROPERTIES ${vfs_ENTITY}_REPLACEMENT "")
set_target_properties (Connector PROPERTIES LINK_FLAGS "-no-pie -Ttext 0x100000")