cmake_minimum_required(VERSION 3.14)
project(ppocr CXX)

set(DEMO_NAME "ppocr")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Output directory will be set after WITH_DLL_LIB option is defined
# Default to static mode output directory

option(WITH_MKL        "Compile demo with MKL/OpenBlas support, default use MKL."       ON)
option(WITH_GPU        "Compile demo with GPU/CPU, default use CPU."                    OFF)
option(WITH_STATIC_LIB "Compile demo with static/shared library, default use static."   ON)
option(USE_FREETYPE "Enable FreeType support" OFF)

SET(PADDLE_LIB "" CACHE PATH "Location of libraries")
SET(OPENCV_DIR "" CACHE PATH "Location of libraries")
SET(CUDA_LIB "" CACHE PATH "Location of libraries")
SET(CUDNN_LIB "" CACHE PATH "Location of libraries")

# CPU feature detection function
function(detect_cpu_features)
    if(MINGW)
        # Try to detect CPU features using cpuid
        include(CheckCXXCompilerFlag)

        # Check for AVX-512 support
        check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512)
        check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
        check_cxx_compiler_flag("-mavx" COMPILER_SUPPORTS_AVX)
        check_cxx_compiler_flag("-msse4.2" COMPILER_SUPPORTS_SSE42)
        check_cxx_compiler_flag("-msse4.1" COMPILER_SUPPORTS_SSE41)
        check_cxx_compiler_flag("-mssse3" COMPILER_SUPPORTS_SSSE3)
        check_cxx_compiler_flag("-mfma" COMPILER_SUPPORTS_FMA)

        # Set SIMD flags based on compiler support
        # Note: This checks compiler support, not CPU support
        # The runtime detection in cpu_features.h handles CPU support
        if(COMPILER_SUPPORTS_AVX512)
            set(CPU_SIMD_FLAGS "-mavx512f -mavx512bw -mavx512dq -mavx512vl" PARENT_SCOPE)
            set(CPU_SIMD_LEVEL "AVX-512" PARENT_SCOPE)
        elseif(COMPILER_SUPPORTS_AVX2)
            set(CPU_SIMD_FLAGS "-mavx2" PARENT_SCOPE)
            set(CPU_SIMD_LEVEL "AVX2" PARENT_SCOPE)
        elseif(COMPILER_SUPPORTS_AVX)
            set(CPU_SIMD_FLAGS "-mavx" PARENT_SCOPE)
            set(CPU_SIMD_LEVEL "AVX" PARENT_SCOPE)
        elseif(COMPILER_SUPPORTS_SSE42)
            set(CPU_SIMD_FLAGS "-msse4.2" PARENT_SCOPE)
            set(CPU_SIMD_LEVEL "SSE4.2" PARENT_SCOPE)
        else()
            set(CPU_SIMD_FLAGS "" PARENT_SCOPE)
            set(CPU_SIMD_LEVEL "Unknown" PARENT_SCOPE)
        endif()

        # Add FMA support if available
        if(COMPILER_SUPPORTS_FMA)
            set(CPU_SIMD_FLAGS "${CPU_SIMD_FLAGS} -mfma" PARENT_SCOPE)
        endif()

        message(STATUS "CPU SIMD Level: ${CPU_SIMD_LEVEL}")
        message(STATUS "CPU SIMD Flags: ${CPU_SIMD_FLAGS}")
    endif()
endfunction()

# Detect CPU features
detect_cpu_features()

macro(safe_set_static_flag)
    foreach(flag_var
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
      if(${${flag_var}} MATCHES "/MD")
        string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
      endif()
    endforeach(flag_var)
endmacro()

if (WITH_MKL)
    ADD_DEFINITIONS(-DUSE_MKL)
endif()

if(NOT DEFINED PADDLE_LIB)
  message(FATAL_ERROR "please set PADDLE_LIB with -DPADDLE_LIB=/path/paddle/lib")
endif()

if(NOT DEFINED OPENCV_DIR)
    message(FATAL_ERROR "please set OPENCV_DIR with -DOPENCV_DIR=/path/opencv")
endif()


if (WIN32)
  if(MINGW)
    # MinGW specific configuration
    include_directories("${PADDLE_LIB}/paddle/include")
    link_directories("${PADDLE_LIB}/paddle/lib")
    set(OpenCV_DIR "${OPENCV_DIR}/x64/mingw/lib")
    find_package(OpenCV REQUIRED)
    if(USE_FREETYPE)
      if(NOT "opencv_freetype" IN_LIST OpenCV_LIBS)
        message(FATAL_ERROR "OpenCV was not compiled with the freetype module (opencv_freetype) !")
      endif()
      add_definitions(-DUSE_FREETYPE)
    endif()
  else()
    # MSVC specific configuration
    include_directories("${PADDLE_LIB}/paddle/include")
    link_directories("${PADDLE_LIB}/paddle/lib")
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
    set(OpenCV_DIR "${OPENCV_DIR}/x64/vc16/lib")
    find_package(OpenCV REQUIRED)
    if(USE_FREETYPE)
      if(NOT "opencv_freetype" IN_LIST OpenCV_LIBS)
        message(FATAL_ERROR "OpenCV was not compiled with the freetype module (opencv_freetype) !")
      endif()
      add_definitions(-DUSE_FREETYPE)
    endif()
  endif()

else ()
  set(OpenCV_DIR "${OPENCV_DIR}/lib64/cmake/opencv4")
  find_package(OpenCV REQUIRED)
  if(USE_FREETYPE)
    if(NOT "opencv_freetype" IN_LIST OpenCV_LIBS)
      message(FATAL_ERROR "OpenCV was not compiled with the freetype module (opencv_freetype) !")
    endif()
    add_definitions(-DUSE_FREETYPE)
  endif()

  include_directories("${PADDLE_LIB}/paddle/include")
  link_directories("${PADDLE_LIB}/paddle/lib")
endif ()
include_directories(${OpenCV_INCLUDE_DIRS})

if(MINGW)
    # MinGW specific flags
    add_definitions(-DGOOGLE_GLOG_DLL_DECL=)
    set(FLAG_OPENMP "-fopenmp")
    # Use detected CPU SIMD flags with optimized settings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g0 -O2 -DNDEBUG -Wa,-mbig-obj -ffunction-sections -fdata-sections ${CPU_SIMD_FLAGS} ${FLAG_OPENMP} -std=c++11")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g0 -O2 -DNDEBUG -Wa,-mbig-obj -ffunction-sections -fdata-sections ${CPU_SIMD_FLAGS} ${FLAG_OPENMP}")
    # Allow multiple definitions for merged static library + strip unused sections
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-multiple-definition -Wl,--gc-sections")
    if (WITH_STATIC_LIB OR WITH_DLL_LIB)
        add_definitions(-DSTATIC_LIB)
        add_definitions(-DYAML_CPP_STATIC_DEFINE)
    endif()
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
    message(STATUS "MinGW CXX flags: ${CMAKE_CXX_FLAGS}")
    message(STATUS "CPU SIMD Level: ${CPU_SIMD_LEVEL}")
elseif (WIN32)
    add_definitions("/DGOOGLE_GLOG_DLL_DECL=")
    if(WITH_MKL)
        set(FLAG_OPENMP "/openmp")
    endif()
    set(CMAKE_C_FLAGS_DEBUG   "${CMAKE_C_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
    set(CMAKE_C_FLAGS_RELEASE  "${CMAKE_C_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
    set(CMAKE_CXX_FLAGS_DEBUG  "${CMAKE_CXX_FLAGS_DEBUG} /bigobj /MTd ${FLAG_OPENMP}")
    set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS_RELEASE} /bigobj /MT ${FLAG_OPENMP}")
    if (WITH_STATIC_LIB)
        safe_set_static_flag()
        add_definitions(-DSTATIC_LIB)
	add_definitions(-DYAML_CPP_STATIC_DEFINE)
    endif()
    message("cmake c debug flags " ${CMAKE_C_FLAGS_DEBUG})
    message("cmake c release flags " ${CMAKE_C_FLAGS_RELEASE})
    message("cmake cxx debug flags " ${CMAKE_CXX_FLAGS_DEBUG})
    message("cmake cxx release flags " ${CMAKE_CXX_FLAGS_RELEASE})
else()
    if(WITH_MKL)
        set(FLAG_OPENMP "-fopenmp")
    endif()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3 ${FLAG_OPENMP} -std=c++11")
    set(CMAKE_STATIC_LIBRARY_PREFIX "")
    message("cmake cxx flags" ${CMAKE_CXX_FLAGS})
endif()

if (WITH_GPU)
    if (NOT DEFINED CUDA_LIB OR ${CUDA_LIB} STREQUAL "")
        message(FATAL_ERROR "please set CUDA_LIB with -DCUDA_LIB=/path/cuda-8.0/lib64")
    endif()
    if (NOT WIN32)
        if (NOT DEFINED CUDNN_LIB)
            message(FATAL_ERROR "please set CUDNN_LIB with -DCUDNN_LIB=/path/cudnn_v7.4/cuda/lib64")
        endif()
    add_definitions(-DWITH_GPU)
    endif(NOT WIN32)
endif()

include_directories("${PADDLE_LIB}/third_party/install/protobuf/include")
include_directories("${PADDLE_LIB}/third_party/install/glog/include")
include_directories("${PADDLE_LIB}/third_party/install/gflags/include")
include_directories("${PADDLE_LIB}/third_party/install/xxhash/include")
include_directories("${PADDLE_LIB}/third_party/install/zlib/include")
include_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/include")
include_directories("${PADDLE_LIB}/third_party/eigen3")
include_directories("${PADDLE_LIB}/paddle/include/")
include_directories("${PADDLE_LIB}/paddle/fluid/inference/api/")
# Also add source directory for headers not in build directory
include_directories("${PADDLE_LIB}/../paddle/fluid/inference/api/")
include_directories("${PADDLE_LIB}/../paddle/fluid/framework/io/")
include_directories("${CMAKE_SOURCE_DIR}/")

# Optional includes - only add if directory exists
foreach(_opt_lib onnxruntime paddle2onnx openvino tbb boost onednn)
  if(EXISTS "${PADDLE_LIB}/third_party/install/${_opt_lib}/include")
    include_directories("${PADDLE_LIB}/third_party/install/${_opt_lib}/include")
  endif()
endforeach()

link_directories("${PADDLE_LIB}/third_party/install/zlib/lib")
link_directories("${PADDLE_LIB}/third_party/install/protobuf/lib")
link_directories("${PADDLE_LIB}/third_party/install/glog/lib")
link_directories("${PADDLE_LIB}/third_party/install/gflags/lib")
link_directories("${PADDLE_LIB}/third_party/install/xxhash/lib")
link_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/lib")
link_directories("${PADDLE_LIB}/paddle/lib")

# Optional link directories
foreach(_opt_lib onnxruntime paddle2onnx openvino tbb)
  if(EXISTS "${PADDLE_LIB}/third_party/install/${_opt_lib}/lib")
    link_directories("${PADDLE_LIB}/third_party/install/${_opt_lib}/lib")
  endif()
endforeach()

if(WITH_MKL)
  include_directories("${PADDLE_LIB}/third_party/install/mklml/include")
  if(MINGW)
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/mklml/lib/libmklml_intel${CMAKE_SHARED_LIBRARY_SUFFIX}
            ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif (WIN32)
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/mklml/lib/mklml.lib
            ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5md.lib)
  else ()
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/mklml/lib/libmklml_intel${CMAKE_SHARED_LIBRARY_SUFFIX}
            ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5${CMAKE_SHARED_LIBRARY_SUFFIX})
    execute_process(COMMAND cp -r ${PADDLE_LIB}/third_party/install/mklml/lib/libmklml_intel${CMAKE_SHARED_LIBRARY_SUFFIX} /usr/lib)
  endif ()
  set(MKLDNN_PATH "${PADDLE_LIB}/third_party/install/onednn")
  if(EXISTS ${MKLDNN_PATH})
    include_directories("${MKLDNN_PATH}/include")
    if(MINGW)
      set(MKLDNN_LIB ${MKLDNN_PATH}/lib/libdnnl${CMAKE_SHARED_LIBRARY_SUFFIX})
    elseif (WIN32)
      set(MKLDNN_LIB ${MKLDNN_PATH}/lib/mkldnn.lib)
    else ()
      set(MKLDNN_LIB ${MKLDNN_PATH}/lib/libdnnl.so.3)
    endif ()
  endif()
else()
  if(MINGW)
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/openblas/lib/libopenblas${CMAKE_STATIC_LIBRARY_SUFFIX})
  elseif (WIN32)
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/openblas/lib/openblas${CMAKE_STATIC_LIBRARY_SUFFIX})
  else ()
    set(MATH_LIB ${PADDLE_LIB}/third_party/install/openblas/lib/libopenblas${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif ()
  # MinGW oneDNN support (独立于 MKL)
  if(MINGW)
    set(MKLDNN_PATH "${PADDLE_LIB}/third_party/install/onednn")
    if(EXISTS ${MKLDNN_PATH})
      include_directories("${MKLDNN_PATH}/include")
      set(MKLDNN_LIB ${MKLDNN_PATH}/lib/libdnnl${CMAKE_STATIC_LIBRARY_SUFFIX})
      message(STATUS "Found oneDNN (MinGW): ${MKLDNN_LIB}")
    else()
      message(STATUS "oneDNN not found at ${MKLDNN_PATH}, MKLDNN disabled")
    endif()
  endif()
endif()

# Note: libpaddle_inference_api.so/a must put before libpaddle_inference.so/a
option(WITH_DLL_LIB "Compile demo with DLL library (6-DLL split), default use static." OFF)

# Set output directory based on build mode
if(WITH_DLL_LIB)
    set(DIST_OUTPUT_DIR "D:/tmp/tmp/dist/ppocr_dll")
else()
    set(DIST_OUTPUT_DIR "D:/tmp/tmp/dist/ppocr")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DIST_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${DIST_OUTPUT_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${DIST_OUTPUT_DIR})
message(STATUS "Output directory: ${DIST_OUTPUT_DIR}")

if(WITH_STATIC_LIB AND NOT WITH_DLL_LIB)
  if(MINGW)
    set(DEPS
        -Wl,--whole-archive
        ${PADDLE_LIB}/paddle/lib/libphi_core${CMAKE_STATIC_LIBRARY_SUFFIX}
        -Wl,--no-whole-archive
        ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_STATIC_LIBRARY_SUFFIX}
        ${PADDLE_LIB}/paddle/lib/libcommon${CMAKE_STATIC_LIBRARY_SUFFIX})
  elseif(WIN32)
    set(DEPS
        ${PADDLE_LIB}/paddle/lib/paddle_inference${CMAKE_STATIC_LIBRARY_SUFFIX})
  else()
    set(DEPS
        ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
elseif(WITH_DLL_LIB)
  # 4-DLL split mode: link against individual DLLs
  # Requires DLLs built with build_paddle_dll.bat
  # DLL import libraries are in different directories
  message(STATUS "Using DLL mode: ${PADDLE_LIB}")
  if(MINGW)
    set(DEPS
        ${PADDLE_LIB}/paddle/fluid/inference/libpaddle_inference.dll.a
        ${PADDLE_LIB}/paddle/phi/libphi_core.dll.a
        ${PADDLE_LIB}/paddle/pir/libpir.dll.a
        ${PADDLE_LIB}/paddle/common/libcommon.dll.a)
  else()
    set(DEPS
        ${PADDLE_LIB}/paddle/fluid/inference/paddle_inference${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()
else()
  if(MINGW)
    set(DEPS
        ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_SHARED_LIBRARY_SUFFIX})
  elseif(WIN32)
    set(DEPS
        ${PADDLE_LIB}/paddle/lib/paddle_inference${CMAKE_SHARED_LIBRARY_SUFFIX})
  else()
    set(DEPS
        ${PADDLE_LIB}/paddle/lib/libpaddle_inference${CMAKE_SHARED_LIBRARY_SUFFIX})
  endif()
endif()

if(MINGW)
    set(DEPS ${DEPS}
        ${MATH_LIB} ${MKLDNN_LIB}
        ${PADDLE_LIB}/third_party/install/glog/lib/libglog.a
        ${PADDLE_LIB}/third_party/install/gflags/lib/libgflags_static.a
        ${PADDLE_LIB}/third_party/install/protobuf/lib/libprotobuf.a
        ${PADDLE_LIB}/third_party/install/zlib/lib/libzlibstatic.a
        ${PADDLE_LIB}/third_party/install/xxhash/lib/libxxhash.a
        shlwapi ws2_32 dbghelp
        )
    # ONNX Runtime for plate detection
    set(ONNXRUNTIME_DIR "D:/tmp/tmp/third_party/onnxruntime")
    if(EXISTS "${ONNXRUNTIME_DIR}/lib/libonnxruntime.dll.a")
        include_directories(${ONNXRUNTIME_DIR}/include)
        link_directories(${ONNXRUNTIME_DIR}/lib)
        set(DEPS ${DEPS} onnxruntime)
        message(STATUS "ONNX Runtime found: ${ONNXRUNTIME_DIR}")
    endif()
    if(EXISTS "${PADDLE_LIB}/third_party/install/yaml-cpp/lib/libyaml-cpp.a")
        set(DEPS ${DEPS} ${PADDLE_LIB}/third_party/install/yaml-cpp/lib/libyaml-cpp.a)
    endif()
    if(EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
        set(DEPS ${DEPS} snappy)
    endif()
elseif (NOT WIN32)
    set(DEPS ${DEPS}
        ${MATH_LIB} ${MKLDNN_LIB}
        glog gflags protobuf z xxhash
        )
    if(EXISTS "${PADDLE_LIB}/third_party/install/snappystream/lib")
        set(DEPS ${DEPS} snappystream)
    endif()
    if (EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
        set(DEPS ${DEPS} snappy)
    endif()
else()
    set(DEPS ${DEPS}
        ${MATH_LIB} ${MKLDNN_LIB}
        glog gflags_static libprotobuf xxhash)
    set(DEPS ${DEPS} libcmt shlwapi)
    if (EXISTS "${PADDLE_LIB}/third_party/install/snappy/lib")
        set(DEPS ${DEPS} snappy)
    endif()
    if(EXISTS "${PADDLE_LIB}/third_party/install/snappystream/lib")
        set(DEPS ${DEPS} snappystream)
    endif()
endif()

if (EXISTS "${PADDLE_LIB}/third_party/install/yaml-cpp/lib")
  set(DEPS ${DEPS} yaml-cpp)
endif()

if(WITH_GPU)
  if(NOT WIN32)
    set(DEPS ${DEPS} ${CUDA_LIB}/libcudart${CMAKE_SHARED_LIBRARY_SUFFIX})
    set(DEPS ${DEPS} ${CUDNN_LIB}/libcudnn${CMAKE_SHARED_LIBRARY_SUFFIX})
  else()
    set(DEPS ${DEPS} ${CUDA_LIB}/cudart${CMAKE_STATIC_LIBRARY_SUFFIX} )
    message($DEPS)
    set(DEPS ${DEPS} ${CUDA_LIB}/cublas${CMAKE_STATIC_LIBRARY_SUFFIX} )
    set(DEPS ${DEPS} ${CUDNN_LIB}/cudnn${CMAKE_STATIC_LIBRARY_SUFFIX})
  endif()
endif()


if (NOT WIN32)
    set(EXTERNAL_LIB "-ldl -lrt -lgomp -lz -lm -lpthread")
    set(DEPS ${DEPS} ${EXTERNAL_LIB})
endif()

set(THIRD_PARTY_PATH ${CMAKE_CURRENT_LIST_DIR}/third_party)
function(download_and_decompress url filename decompress_dir)
  if(NOT EXISTS "${filename}" AND NOT EXISTS "${decompress_dir}")
    message("Downloading file from ${url} to ${filename} ...")
    file(DOWNLOAD ${url} "${filename}.tmp" SHOW_PROGRESS)
    file(RENAME "${filename}.tmp" ${filename})
  endif()
  if(NOT EXISTS ${decompress_dir})
    file(MAKE_DIRECTORY ${decompress_dir})
    message("Decompress file ${filename} ...")
    execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf ${filename} WORKING_DIRECTORY ${decompress_dir})
  endif()
endfunction()


set(PACKAGE_LIST abseil-cpp clipper_ver6.4.2 nlohmann)
foreach(PKG ${PACKAGE_LIST})
    set(PKG_URL "https://paddle-model-ecology.bj.bcebos.com/paddlex/cpp/libs/${PKG}.tgz")
    set(PKG_TGZ_PATH "${CMAKE_CURRENT_BINARY_DIR}/${PKG}.tgz")
    set(PKG_DST_PATH "${THIRD_PARTY_PATH}/${PKG}")
    download_and_decompress(${PKG_URL} ${PKG_TGZ_PATH} ${PKG_DST_PATH})
endforeach()

add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/clipper_ver6.4.2/cpp)
include_directories(${POLYCLIPPING_INCLUDE_DIR})

set(DEPS ${DEPS} ${OpenCV_LIBS})
set(DEPS ${DEPS} absl::statusor)
set(DEPS ${DEPS} polyclipping)

if(UNIX)
  find_package(Iconv REQUIRED) 
endif()

file(GLOB_RECURSE SRC_LIST "./src/*.cc")
set(SRCS cli.cc stubs.cc)
add_executable(${DEMO_NAME} ${SRCS} ${SRC_LIST} )
target_link_libraries(${DEMO_NAME} ${DEPS} )

# Copy DLLs to dist/ppocr directory after build
set(DIST_DIR ${DIST_OUTPUT_DIR})

if (WIN32 AND WITH_MKL)
    add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PADDLE_LIB}/third_party/install/mklml/lib/mklml.dll ${DIST_DIR}/mklml.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PADDLE_LIB}/third_party/install/mklml/lib/libiomp5md.dll ${DIST_DIR}/libiomp5md.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PADDLE_LIB}/third_party/install/onednn/lib/mkldnn.dll ${DIST_DIR}/mkldnn.dll
    )
endif()

# Copy MinGW runtime DLLs to dist directory
if(MINGW)
    # Use CMAKE_C_COMPILER to find MinGW bin directory
    get_filename_component(MINGW_BIN_DIR ${CMAKE_C_COMPILER} DIRECTORY)
    message(STATUS "MinGW bin dir: ${MINGW_BIN_DIR}")
    add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libgcc_s_seh-1.dll ${DIST_DIR}/libgcc_s_seh-1.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libstdc++-6.dll ${DIST_DIR}/libstdc++-6.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libwinpthread-1.dll ${DIST_DIR}/libwinpthread-1.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libgomp-1.dll ${DIST_DIR}/libgomp-1.dll
    )
    # Also copy to build directory for local testing
    add_custom_command(TARGET ${DEMO_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libgcc_s_seh-1.dll ${CMAKE_BINARY_DIR}/libgcc_s_seh-1.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libstdc++-6.dll ${CMAKE_BINARY_DIR}/libstdc++-6.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libwinpthread-1.dll ${CMAKE_BINARY_DIR}/libwinpthread-1.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MINGW_BIN_DIR}/libgomp-1.dll ${CMAKE_BINARY_DIR}/libgomp-1.dll
    )
endif()

# Service target
set(SERVICE_NAME "ppocr_service")
set(SERVICE_SRCS
    service/service_main.cc
    service/service_core.cc
    service/tcp_server.cc
    service/protocol.cc
    service/base64_utils.cc
    service/process_pool.cc
    service/worker_process.cc
    service/plate_detector.cc
)
add_executable(${SERVICE_NAME} ${SERVICE_SRCS} ${SRC_LIST})
target_include_directories(${SERVICE_NAME} PRIVATE ${THIRD_PARTY_PATH})
target_link_libraries(${SERVICE_NAME} ${DEPS})
if(WIN32)
    target_link_libraries(${SERVICE_NAME} bcrypt)
endif()

# Worker target (child process for OCR)
set(WORKER_NAME "ppocr_worker")
set(WORKER_SRCS
    service/worker_main.cc
)
add_executable(${WORKER_NAME} ${WORKER_SRCS} ${SRC_LIST})
target_include_directories(${WORKER_NAME} PRIVATE ${THIRD_PARTY_PATH})
target_link_libraries(${WORKER_NAME} ${DEPS})

# Client target (standalone, no Paddle dependency)
set(CLIENT_NAME "ppocr_client")
set(CLIENT_SRCS
    service/client_example.cc
    service/tcp_client.cc
    service/protocol.cc
    service/base64_utils.cc
)
add_executable(${CLIENT_NAME} ${CLIENT_SRCS})
target_include_directories(${CLIENT_NAME} PRIVATE ${THIRD_PARTY_PATH})
target_link_libraries(${CLIENT_NAME} ws2_32)
