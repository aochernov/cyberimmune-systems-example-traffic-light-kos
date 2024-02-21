project (einit)

# Tools for using NK parser.
include (platform/image)

# Set compile flags.
project_header_default ("STANDARD_GNU_11:YES" "STRICT_WARNINGS:NO")

# Define the ENTITIES variable with a list of entities.
set (ENTITIES ControlSystem LightsGPIO Connector ${precompiled_vfsVfsNet} ${rump_DHCPCD_ENTITY})

set (ROOTFS_ENTITY "kl.VfsRamFs")
set (ROOTFS_DEVICE "ramfs")
set (FS_BACKEND ${precompiled_vfsVfsRamFs})
set (FS_IMAGE ramdisk0.img)
set (FS_IMG_FORMAT ext4)
set (FS_IMG_SIZE 2)
set (FILES ${CMAKE_BINARY_DIR}/einit/${FS_IMAGE})

list (APPEND ENTITIES ${FS_BACKEND})

set_target_properties(${FS_BACKEND} PROPERTIES
    EXTRA_ARGS "\
    - -l
    - devfs /dev devfs 0")

set(DHCPCD_CONFIG_FILE "/etc/dhcpcd.conf")
message ("Used default dynamic network configuration with fallback to static configuration by ${ROOTFS_DEVICE}: ${DHCPCD_CONFIG_FILE}")

set_target_properties (${rump_DHCPCD_ENTITY} PROPERTIES
    DEPENDS_ON_ENTITY ""
    EXTRA_CONNECTIONS "\
  - target: kl.VfsNet
    id: kl.VfsNet
  - target: ${ROOTFS_ENTITY}
    id: ${ROOTFS_ENTITY}"
    EXTRA_ENV "\
    VFS_FILESYSTEM_BACKEND: client{fs->net}:${ROOTFS_ENTITY}
    VFS_NETWORK_BACKEND: client:kl.VfsNet"
    EXTRA_ARGS "\
    - '-4'
    - -f
    - ${DHCPCD_CONFIG_FILE}")

add_custom_target (${FS_IMAGE}
    COMMAND  ${KL_SDK_ROOT_PATH}/common/prepare_hdd_img.sh
        -d   ${CMAKE_SOURCE_DIR}/resources/hdd
        -s   ${FS_IMG_SIZE}
        -f   ${FS_IMG_FORMAT}
        -img ${FS_IMAGE})

# KasperskyOS image for target hardware platform.
build_kos_hw_image (kos-image
        EINIT_ENTITY EinitHw
        CONNECTIONS_CFG "src/init.yaml.in"
        SECURITY_PSL "src/security.psl"
        IMAGE_FILES ${ENTITIES} ${FILES})

add_dependencies (kos-image ${FS_IMAGE})

# KasperskyOS image for QEMU with simulation targets (sim, gdbsim, gdb).
build_kos_qemu_image (kos-qemu-image
        EINIT_ENTITY EinitQemu
        CONNECTIONS_CFG "src/init.yaml.in"
        SECURITY_PSL "src/security.psl"
        IMAGE_FILES ${ENTITIES} ${FILES})

add_dependencies (kos-qemu-image ${FS_IMAGE})