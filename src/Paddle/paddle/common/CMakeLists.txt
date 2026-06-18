file(GLOB common_srcs "*.cc")

if(WIN32)
  if(MINGW)
    # MinGW: use SHARED with __declspec(dllexport) macros
    set(COMMON_NAME
        libcommon.dll
        CACHE INTERNAL "" FORCE)
  else()
    set(COMMON_NAME
        common.dll
        CACHE INTERNAL "" FORCE)
  endif()
elseif(APPLE)
  set(COMMON_NAME
      libcommon.dylib
      CACHE INTERNAL "" FORCE)
else()
  set(COMMON_NAME
      libcommon.so
      CACHE INTERNAL "" FORCE)
endif()

set(COMMON_LIB
    "${CMAKE_CURRENT_BINARY_DIR}/${COMMON_NAME}"
    CACHE FILEPATH "COMMON Library" FORCE)
if(WIN32 AND NOT MINGW)
  set(COMMON_LINK
      "${CMAKE_CURRENT_BINARY_DIR}/common.lib"
      CACHE FILEPATH "COMMON Import Library" FORCE)
endif()

# All platforms use SHARED for common library
set(COMMON_BUILD_TYPE
    SHARED
    CACHE INTERNAL "" FORCE)

cc_library(common ${COMMON_BUILD_TYPE} SRCS ${common_srcs})
if(WIN32)
  if(MINGW)
    # MinGW: use --export-all-symbols to export all symbols from common DLL
    target_link_options(common PRIVATE -Wl,--export-all-symbols)
  else()
    set_property(TARGET common PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
  endif()
endif()
# only used for libpaddle_inference.a
cc_library(common_static STATIC SRCS ${common_srcs})
