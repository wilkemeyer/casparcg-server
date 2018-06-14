cmake_minimum_required (VERSION 2.6)
project (cgbrowserhost)

set(SOURCES
		cgbrowserhost.cpp
		stdafx.cpp
)
set(HEADERS
		targetver.h
		stdafx.h
)


add_executable(cgbrowserhost WIN32 ${SOURCES} ${HEADERS})
add_precompiled_header(cgbrowserhost stdafx.h FORCEINCLUDE)

target_link_libraries(cgbrowserhost
		libcef
		libcef_dll_wrapper
)

include_directories(..)
include_directories(../..)
include_directories(${BOOST_INCLUDE_PATH})
include_directories(${RXCPP_INCLUDE_PATH})
include_directories(${TBB_INCLUDE_PATH})
include_directories(${CEF_INCLUDE_PATH})
include_directories(${CEF_PATH})


casparcg_add_runtime_dependency("$<TARGET_FILE:cgbrowserhost>")