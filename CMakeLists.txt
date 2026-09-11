cmake_minimum_required(VERSION 3.28)
set(CMAKE_CXX_STANDARD 20)
project(simply-cpp-image)

set(CMAKE_POSITION_INDEPENDENT_CODE ON CACHE BOOL "" FORCE)
set(CMAKE_EXPORT_COMPILE_COMMANDS OFF CACHE BOOL "" FORCE)

############################
#  Fetch Third Party Code  #
############################
set(SETUP_VERSION_FILE "${CMAKE_BINARY_DIR}/setup.version")
if (NOT EXISTS "${SETUP_VERSION_FILE}")
    execute_process(COMMAND git init -b main WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    execute_process(COMMAND git submodule add https://github.com/roelofrossouw/simply-cpp.git third_party/sc WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    execute_process(COMMAND git submodule update --init --recursive WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    if (APPLE)
        execute_process(COMMAND brew install opencv WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    else ()
        execute_process(COMMAND apt -y install libopencv-dev WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif ()
    file(WRITE "${SETUP_VERSION_FILE}" "1")
endif ()


######################
#   Local Includes   #
######################
set(ADD_POSTGRESQL OFF)
add_subdirectory(third_party/sc sc EXCLUDE_FROM_ALL)

######################
#  Dependencies Used #
######################
if (APPLE)
    find_package(OpenCV CONFIG REQUIRED COMPONENTS core ml imgproc imgcodecs dnn highgui)
else ()
    find_package(OpenCV CONFIG REQUIRED COMPONENTS core ml imgproc imgcodecs dnn highgui calib3d)
endif ()

############
# Projects #
############

project(sc-image VERSION 1.0.1 LANGUAGES CXX)
add_library(${PROJECT_NAME} SHARED src/image.cpp include/image.h)
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(${PROJECT_NAME} PUBLIC sc ${OpenCV_LIBS})

project(test-image VERSION 1.0.1 LANGUAGES CXX)
add_executable(${PROJECT_NAME} EXCLUDE_FROM_ALL tests/image.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE sc-image)