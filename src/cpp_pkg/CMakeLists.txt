cmake_minimum_required(VERSION 3.20)
project(cpp_pkg)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Try to find ONNX Runtime (optional for yamnet_classification node)
find_path(ONNXRUNTIME_INCLUDE_DIR 
  NAMES onnxruntime_cxx_api.h
  PATHS /opt/onnxruntime/include /usr/local/include/onnxruntime /usr/include
)
find_library(ONNXRUNTIME_LIB 
  NAMES onnxruntime
  PATHS /opt/onnxruntime/lib /usr/local/lib /usr/lib
)

if(ONNXRUNTIME_INCLUDE_DIR AND ONNXRUNTIME_LIB)
  set(ONNXRUNTIME_FOUND TRUE)
  message(STATUS "ONNX Runtime found:")
  message(STATUS "  Include: ${ONNXRUNTIME_INCLUDE_DIR}")
  message(STATUS "  Library: ${ONNXRUNTIME_LIB}")
else()
  set(ONNXRUNTIME_FOUND FALSE)
  message(WARNING "ONNX Runtime NOT found - yamnet_classification node will not be built")
  message(STATUS "To install ONNX Runtime, see YAMNET_SETUP_GUIDE.md")
endif()

# reader node
add_executable(reader src/reader.cpp src/serial_port.cpp)
target_include_directories(reader PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(reader PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
)

# writer node
add_executable(writer src/writer.cpp)
target_link_libraries(writer PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
)

# build_waveform node
add_executable(build_waveform src/build_waveform.cpp)
target_link_libraries(build_waveform PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
)

# yamnet_classification node (only if ONNX Runtime is available)
set(TARGETS_TO_INSTALL reader writer build_waveform)

if(ONNXRUNTIME_FOUND)
  add_executable(yamnet_classification src/yamnet_classification.cpp)
  target_include_directories(yamnet_classification PUBLIC
    ${ONNXRUNTIME_INCLUDE_DIR}
  )
  target_link_libraries(yamnet_classification PUBLIC
    rclcpp::rclcpp
    ${std_msgs_TARGETS}
    ${ONNXRUNTIME_LIB}
  )
  list(APPEND TARGETS_TO_INSTALL yamnet_classification)
  message(STATUS "yamnet_classification node will be built")
endif()

install(TARGETS
  ${TARGETS_TO_INSTALL}
  DESTINATION lib/${PROJECT_NAME})

# Install launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
