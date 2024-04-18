cmake_minimum_required(VERSION 3.4)

project(libfranka-hapticRendering CXX)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/../cmake)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT FRANKA_IS_FOUND)
    find_package(Franka REQUIRED)
endif()
find_package(Eigen3 REQUIRED)
find_package(Poco REQUIRED COMPONENTS Foundation)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(hapticRendering_common STATIC
  hapticRendering_common.cpp
)

target_link_libraries(hapticRendering_common PUBLIC Franka::Franka Eigen3::Eigen3)

set(HAPTIC_RENDERING
  cartesian_impedance_control_2
  force_control_2
)

foreach(hapticRendering ${HAPTIC_RENDERING})
  add_executable(${hapticRendering} ${hapticRendering}.cpp)
  target_link_libraries(${hapticRendering} Franka::Franka hapticRendering_common Eigen3::Eigen3)
endforeach()

# target_link_libraries(joint_impedance_control Threads::Threads)
# target_link_libraries(motion_with_control Poco::Foundation)

include(GNUInstallDirs)
install(TARGETS ${HAPTIC_RENDERING}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
