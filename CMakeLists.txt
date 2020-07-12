cmake_minimum_required(VERSION 3.16)

project(FacialLandmarksForCubism_project)

add_subdirectory(lib/dlib/dlib dlib_build)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})

add_library(FacialLandmarksForCubism STATIC src/facial_landmark_detector.cpp)
set_target_properties(FacialLandmarksForCubism PROPERTIES PUBLIC_HEADER include/facial_landmark_detector.h)

target_include_directories(FacialLandmarksForCubism PRIVATE include lib/dlib)
target_link_libraries(FacialLandmarksForCubism ${OpenCV_LIBS} dlib::dlib)

