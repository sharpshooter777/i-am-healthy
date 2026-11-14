# SPDX-License-Identifier: Apache-2.0
# The only OS-specific code. One implementation is chosen per platform here,
# so the sources themselves need no #ifdef to pick a platform.
if(CMAKE_SYSTEM_NAME STREQUAL "QNX")
    set(_impl src/qnx/platform_qnx.cpp)
elseif(WIN32)
    set(_impl src/win32/platform_win32.cpp)
else()
    set(_impl src/posix/platform_posix.cpp)
endif()

add_library(vitals_platform STATIC ${_impl})
target_include_directories(vitals_platform PUBLIC include)
vitals_warnings(vitals_platform)
