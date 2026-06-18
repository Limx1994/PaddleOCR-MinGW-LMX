add_definitions(-DIR_LIBRARY)
set_property(GLOBAL PROPERTY IR_TARGETS "")

file(GLOB_RECURSE PIR_CPP_SOURCES "*.cc")

if(WIN32)
  if(MINGW)
    set(IR_NAME
        libpir.dll
        CACHE INTERNAL "" FORCE)
  elseif(WITH_SHARED_IR)
    set(IR_NAME
        pir.dll
        CACHE INTERNAL "" FORCE)
  else()
    set(IR_NAME
        pir.lib
        CACHE INTERNAL "" FORCE)
  endif()
elseif(APPLE)
  if(WITH_SHARED_IR)
    set(IR_NAME
        libpir.dylib
        CACHE INTERNAL "" FORCE)
  else()
    set(IR_NAME
        libpir.a
        CACHE INTERNAL "" FORCE)
  endif()
else()
  if(WITH_SHARED_IR)
    set(IR_NAME
        libpir.so
        CACHE INTERNAL "" FORCE)
  else()
    set(IR_NAME
        libpir.a
        CACHE INTERNAL "" FORCE)
  endif()
endif()

set(IR_LIB
    "${CMAKE_CURRENT_BINARY_DIR}/${IR_NAME}"
    CACHE FILEPATH "IR Library" FORCE)

if(MINGW OR WITH_SHARED_IR)
  add_library(pir SHARED ${PIR_CPP_SOURCES})
  target_link_libraries(pir common)
  if(MINGW)
    # MinGW: use --export-all-symbols to export all symbols from pir DLL
    # WINDOWS_EXPORT_ALL_SYMBOLS only works for MSVC, not MinGW
    target_link_options(pir PRIVATE -Wl,--export-all-symbols)
    # Link Windows system libraries needed by glog
    target_link_libraries(pir dbghelp ws2_32 shlwapi)
  endif()
else()
  cc_library(
    pir
    SRCS ${PIR_CPP_SOURCES}
    DEPS common)
  get_property(ir_targets GLOBAL PROPERTY IR_TARGETS)
  set(ir_targets pir)
  set_property(GLOBAL PROPERTY IR_TARGETS "${ir_targets}")
endif()

if((CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  set_target_properties(pir PROPERTIES COMPILE_FLAGS "-Wno-maybe-uninitialized")
endif()
