cmake_minimum_required(VERSION 3.14)
project(ppocr_service CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(WITH_STATIC_LIB "Use static library" ON)
option(WITH_DLL_LIB "Use DLL library" OFF)

# Set output directory based on build mode
if(WITH_DLL_LIB)
    set(DIST_OUTPUT_DIR "D:/tmp/tmp/dist/ppocr_dll")
else()
    set(DIST_OUTPUT_DIR "D:/tmp/tmp/dist/ppocr")
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DIST_OUTPUT_DIR})
message(STATUS "Output directory: ${DIST_OUTPUT_DIR}")

# Set paths
SET(PADDLE_LIB "" CACHE PATH "Location of Paddle library")
SET(OPENCV_DIR "" CACHE PATH "Location of OpenCV library")

if(NOT DEFINED PADDLE_LIB)
    message(FATAL_ERROR "Please set PADDLE_LIB with -DPADDLE_LIB=/path/paddle/lib")
endif()

if(NOT DEFINED OPENCV_DIR)
    message(FATAL_ERROR "Please set OPENCV_DIR with -DOPENCV_DIR=/path/opencv")
endif()

# Include directories
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../..")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/..")
include_directories("${PADDLE_LIB}/paddle/include")
include_directories("${PADDLE_LIB}/include")
include_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/include")
include_directories("${PADDLE_LIB}/third_party/install/gflags/include")
include_directories("${PADDLE_LIB}/third_party/install/utf8proc/include")
include_directories("${PADDLE_LIB}/third_party/install/dirent/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/abseil-cpp")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/clipper_ver6.4.2/cpp")

# OpenCV
set(OpenCV_DIR "${OPENCV_DIR}/x64/mingw/lib")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

# Abseil - use pre-built static libraries
# Note: We need to use --whole-archive to ensure all symbols are included

# Link directories
link_directories("${PADDLE_LIB}/paddle/lib")
link_directories("${PADDLE_LIB}/third_party/install/openblas/lib")
link_directories("${PADDLE_LIB}/third_party/install/onednn/lib")
link_directories("${PADDLE_LIB}/third_party/install/glog/lib")
link_directories("${PADDLE_LIB}/third_party/install/gflags/lib")
link_directories("${PADDLE_LIB}/third_party/install/protobuf/lib")
link_directories("${PADDLE_LIB}/third_party/install/zlib/lib")
link_directories("${PADDLE_LIB}/third_party/install/xxhash/lib")
link_directories("${PADDLE_LIB}/third_party/install/yaml-cpp/lib")

# Paddle libraries - use same order as main CMakeLists.txt
# libphi_core.a must be linked with --whole-archive
set(PADDLE_LIBS
    -Wl,--whole-archive
    ${PADDLE_LIB}/paddle/lib/libphi_core.a
    -Wl,--no-whole-archive
    ${PADDLE_LIB}/paddle/lib/libpaddle_inference.a
    ${PADDLE_LIB}/paddle/lib/libcommon.a
)

# Third party libraries
set(THIRD_PARTY_LIBS
    ${PADDLE_LIB}/third_party/install/openblas/lib/libopenblas.a
    ${PADDLE_LIB}/third_party/install/onednn/lib/libdnnl.a
    ${PADDLE_LIB}/third_party/install/glog/lib/libglog.a
    ${PADDLE_LIB}/third_party/install/gflags/lib/libgflags_static.a
    ${PADDLE_LIB}/third_party/install/protobuf/lib/libprotobuf.a
    ${PADDLE_LIB}/third_party/install/zlib/lib/libzlibstatic.a
    ${PADDLE_LIB}/third_party/install/xxhash/lib/libxxhash.a
    ${PADDLE_LIB}/third_party/install/yaml-cpp/lib/libyaml-cpp.a
)

# Abseil libraries - built from source via add_subdirectory
# The absl::statusor target will be available after add_subdirectory

# Clipper library
set(CLIPPER_LIB "${CMAKE_CURRENT_SOURCE_DIR}/../build_mingw/third_party/clipper_ver6.4.2/cpp/libpolyclipping.dll.a")

# ONNX Runtime
set(ONNXRUNTIME_DIR "D:/tmp/tmp/third_party/onnxruntime")
include_directories(${ONNXRUNTIME_DIR}/include)
link_directories(${ONNXRUNTIME_DIR}/lib)

# Build abseil from source
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../third_party/abseil-cpp" "${CMAKE_CURRENT_BINARY_DIR}/abseil-cpp")

# yaml-cpp library (static)
set(YAMLCPP_LIB "${PADDLE_LIB}/third_party/install/yaml-cpp/lib/libyaml-cpp.a")
# Define YAML_CPP_STATIC_DEFINE to use static library mode
add_definitions(-DYAML_CPP_STATIC_DEFINE)

# Source files
set(SERVICE_SRCS
    service_main.cc
    service_core.cc
    tcp_server.cc
    protocol.cc
    base64_utils.cc
    plate_detector.cc
    process_pool.cc
    worker_process.cc
)

set(CLIENT_SRCS
    client_example.cc
    tcp_client.cc
    protocol.cc
    base64_utils.cc
)

# OCR source files
file(GLOB OCR_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/api/pipelines/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/api/models/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/pipelines/**/*.cc"
)

file(GLOB COMMON_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/common/*.cc"
)

file(GLOB UTILS_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/utils/*.cc"
)

file(GLOB MODULES_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/modules/**/*.cc"
)

file(GLOB BASE_SRCS
    "${CMAKE_CURRENT_SOURCE_DIR}/../src/base/*.cc"
)

# Service executable
add_executable(ppocr_service
    ${SERVICE_SRCS}
    ${OCR_SRCS}
    ${COMMON_SRCS}
    ${UTILS_SRCS}
    ${MODULES_SRCS}
    ${BASE_SRCS}
)

target_link_libraries(ppocr_service
    ${PADDLE_LIBS}
    ${THIRD_PARTY_LIBS}
    ${OpenCV_LIBS}
    absl::statusor
    ${CLIPPER_LIB}
    ${YAMLCPP_LIB}
    onnxruntime
    -lgomp
    -lpthread
    -lshlwapi
    -lws2_32
    -ldbghelp
    -lbcrypt
    -lkernel32
    -luser32
    -lgdi32
    -lwinspool
    -lshell32
    -lole32
    -loleaut32
    -luuid
    -lcomdlg32
    -ladvapi32
)

# Client executable
add_executable(ppocr_client
    ${CLIENT_SRCS}
)

target_link_libraries(ppocr_client
    ws2_32
)

# Install
install(TARGETS ppocr_service ppocr_client
        RUNTIME DESTINATION bin)
