# SPDX-License-Identifier: Apache-2.0
# Shared part of the QNX 8.0 toolchain files. The including file sets
# VITALS_QNX_ARCH (x86_64 or aarch64) and VITALS_QNX_VARIANT (gcc_nto...).
#
# Run the SDP environment script first so QNX_HOST and QNX_TARGET are set:
#   Linux:   source ~/qnx805/qnxsdp-env.sh
#   Windows: <SDP>\qnxsdp-env.bat

if(NOT DEFINED ENV{QNX_HOST} OR NOT DEFINED ENV{QNX_TARGET})
    message(FATAL_ERROR "QNX_HOST and QNX_TARGET are not set. Source qnxsdp-env.sh (or run qnxsdp-env.bat) first.")
endif()

file(TO_CMAKE_PATH "$ENV{QNX_HOST}" _qnx_host)
file(TO_CMAKE_PATH "$ENV{QNX_TARGET}" _qnx_target)

set(CMAKE_SYSTEM_NAME QNX)
set(CMAKE_SYSTEM_VERSION 8.0.0)
set(CMAKE_SYSTEM_PROCESSOR ${VITALS_QNX_ARCH})

if(CMAKE_HOST_WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()

set(CMAKE_C_COMPILER   "${_qnx_host}/usr/bin/qcc${_exe}")
set(CMAKE_CXX_COMPILER "${_qnx_host}/usr/bin/q++${_exe}")
set(CMAKE_C_COMPILER_TARGET   ${VITALS_QNX_VARIANT})
set(CMAKE_CXX_COMPILER_TARGET ${VITALS_QNX_VARIANT})

# A strict -std=c++17 hides QNX and POSIX declarations such as CLOCK_MONOTONIC
# and breaks the SDP's libc++ headers. _QNX_SOURCE exposes them again.
set(CMAKE_C_FLAGS_INIT   "-D_QNX_SOURCE")
set(CMAKE_CXX_FLAGS_INIT "-D_QNX_SOURCE")

# qcc finds its own headers and libraries; these settings only steer
# find_package() and friends into the target tree, never the host's.
set(CMAKE_FIND_ROOT_PATH "${_qnx_target}" "${_qnx_target}/${VITALS_QNX_TARGET_DIR}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
