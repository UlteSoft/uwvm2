# Called before the cache lock/configure/build transaction. Never create or
# mutate a cache here: the caller has already created this exact directory.
# Resolve aliases with CMake itself so native and cross builds do not need a
# host-specific realpath command or temporarily change xmake's working directory.
cmake_minimum_required(VERSION 3.20)
if(NOT IS_DIRECTORY "${UWVM_ROS_BUILD_DIRECTORY}")
  message(FATAL_ERROR "Missing bundled LLVM build directory")
endif()
file(REAL_PATH "${UWVM_ROS_BUILD_DIRECTORY}" physical)
execute_process(COMMAND "${CMAKE_COMMAND}" -E echo "${physical}"
  COMMAND_ERROR_IS_FATAL ANY)
