cmake_minimum_required(VERSION 3.18)
project(bear LANGUAGES C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

include(CheckLanguage)

set(default_build_type "Release")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "Assert")
endif()
string(TOUPPER ${CMAKE_BUILD_TYPE} BUILD_TYPE)


function(fix_nvidia_binary_blob nv_driver_src name)
    if(NOT EXISTS "${nv_driver_src}/${name}.o_binary")
        message(STATUS "Cannot find NVIDIA binary blob for ${name}. Attempting to corrective actions")
        set(nv_driver_arch ${CMAKE_HOST_SYSTEM_PROCESSOR})
        if("${nv_driver_arch}" STREQUAL "x86_64")
            set(nv_driver_arch "amd64")
        endif()
        if(EXISTS "${NV_DRIVER_SRC}/nvidia/nv-kernel-${nv_driver_arch}.o_binary")
            file(CREATE_LINK "${nv_driver_src}/${name}-${nv_driver_arch}.o_binary"
                             "${nv_driver_src}/${name}.o_binary")
        else()
            message(FATAL_ERROR "Failed to correct missing NVIDIA driver binary blob ${name}.")
        endif()
    endif()
endfunction()

file(GLOB_RECURSE nv_driver_paths "/usr/src/*/nv-p2p.h")
string(REPLACE "nv-p2p.h" "" nv_driver_paths "${nv_driver_paths}")
if(nv_driver_paths)
    list(REVERSE nv_driver_paths)
endif()

find_path(nv_driver_include NAMES "nv-p2p.h" PATHS "${NVIDIA_DRIVER}" ${nv_driver_paths})
set(nv_driver_path "${nv_driver_include}")

if(NOT EXISTS "${nv_driver_path}/Module.symvers")
    get_filename_component(nv_driver_path "${nv_driver_path}" DIRECTORY)
endif()

if("${nv_driver_path}" STREQUAL "")
    message(STATUS "NVIDIA Driver not found. Compiling CPU only kernel modules")
    set(NV_DRIVER_CONFIG "n" CACHE STRING INTERNAL)
else()
    message(STATUS "Using NVIDIA Driver at ${nv_driver_path} to compile kernel modules")
    set(NV_DRIVER_CONFIG "m" CACHE STRING INTERNAL)
    set(NV_DRIVER_PATH ${nv_driver_path} CACHE PATH INTERNAL)

    if(NOT EXISTS "${nv_driver_path}/Module.symvers")
        message(STATUS "Cannot find Module Symbols in NVIDIA tree. Generating it at compile-time")
        get_filename_component(nv_local_dir "${nv_driver_path}" NAME)
        file(COPY ${nv_driver_path} DESTINATION "${PROJECT_BINARY_DIR}/nvidia/")
        set(NV_DRIVER_SRC "${PROJECT_BINARY_DIR}/nvidia/${nv_local_dir}")
        set(NV_DRIVER_SYMVERS "${NV_DRIVER_SRC}/Module.symvers")
        fix_nvidia_binary_blob("${NV_DRIVER_SRC}/nvidia" "nv-kernel")
        fix_nvidia_binary_blob("${NV_DRIVER_SRC}/nvidia-modeset" "nv-modeset-kernel")

        add_custom_target(nvidia-symvers ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} make module WORKING_DIRECTORY "${PROJECT_BINARY_DIR}/nvidia/${nv_local_dir}")
    else()
        set(NV_DRIVER_SYMVERS "${nv_driver_path}/Module.symvers")
    endif()
endif()

# Linux kernel modules
set(KERNEL "/lib/modules/${CMAKE_SYSTEM_VERSION}/build" CACHE PATH "Path to kernel source, or module directory")
set(module_bin "${PROJECT_BINARY_DIR}/module" CACHE PATH INTERNAL)
set(module_src "${PROJECT_SOURCE_DIR}/module" CACHE PATH INTERNAL)
set(module_ccflags_DEBUG "-DCONFIG_BAFS_DEBUG")
set(module_ccflags "${module_ccflags_${BUILD_TYPE}}")
if(NV_DRIVER_PATH)
    set(module_ccflags "${module_ccflags} -I${NV_DRIVER_PATH}/nvidia")
    set(module_extra_symbols ${NV_DRIVER_SYMVERS} CACHE STRING INTERNAL)
endif()
configure_file("${module_src}/Makefile.in" "${module_bin}/Makefile" @ONLY)
file(MAKE_DIRECTORY ${module_bin}/bafs)
add_custom_target(module ALL "${CMAKE_MAKE_PROGRAM}" WORKING_DIRECTORY "${module_bin}")
if(TARGET nvidia-symvers)
    add_dependencies(module nvidia-symvers)
endif()

add_library(kernel-module INTERFACE)
target_include_directories(kernel-module INTERFACE
    $<BUILD_INTERFACE:${module_src}/include>
    $<INSTALL_INTERFACE:include>
)

