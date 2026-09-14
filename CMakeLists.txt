cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
project(simply-cpp-image)

include(FetchContent)
FetchContent_Declare(
        sc
        GIT_REPOSITORY https://github.com/roelofrossouw/simply-cpp
        GIT_TAG origin/main # Or a specific tag to stay stable #
        GIT_SHALLOW TRUE
        EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(sc)

######################
#  Dependencies Used #
######################
if (APPLE)
    set(OPENCV_COMPONENTS core ml imgproc imgcodecs dnn highgui)
else ()
    set(OPENCV_COMPONENTS core ml imgproc imgcodecs dnn highgui calib3d)
endif ()
find_package(OpenCV QUIET COMPONENTS ${OPENCV_COMPONENTS})
if (NOT OpenCV_FOUND)
    if (UNIX AND EXISTS "/usr/bin/apt")
        execute_process(COMMAND sudo apt -y install libopencv-dev RESULT_VARIABLE INSTALL_RESULT)
    elseif (APPLE)
        execute_process(COMMAND brew install -y opencv RESULT_VARIABLE INSTALL_RESULT)
    endif ()
    find_package(OpenCV QUIET COMPONENTS ${OPENCV_COMPONENTS})
    if (NOT OpenCV_FOUND)
        message(FATAL_ERROR "Failed to install or locate OpenCV (apt result=${INSTALL_RESULT})")
    endif ()
endif ()

###########
# Modules #
###########
project(sc-image VERSION 1.0.1 LANGUAGES CXX)
add_library(${PROJECT_NAME} SHARED src/image.cpp include/image.h)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PUBLIC sc ${OpenCV_LIBS})

###########
#  Tests  #
###########
project(test-image VERSION 1.0.1 LANGUAGES CXX)
add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL tests/image.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sc-image)