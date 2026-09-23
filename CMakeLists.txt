cmake_minimum_required(VERSION 3.22)
set(SC_MODULE image)
project(simply-cpp-${SC_MODULE} LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options:
option(SC_IMAGE_BUILD_TESTS "Build simply-cpp-image tests" ${PROJECT_IS_TOP_LEVEL})
include(cmake/sc_bootstrap.cmake)
get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################

find_or_install_package(sc-core simply-cpp-dev simply-cpp)
find_or_install_package(lunasvg simply-cpp-lunasvg lunasvg)

## OpenCV ##
set(SC_IMAGE_OPENCV_COMPONENTS core ml imgproc imgcodecs dnn highgui)
if (NOT APPLE)
    list(APPEND SC_IMAGE_OPENCV_COMPONENTS calib3d)
endif ()
find_or_install_package(OpenCV libopencv-dev opencv COMPONENTS ${SC_IMAGE_OPENCV_COMPONENTS})

###########
# Objects #
###########
set(SOURCE_OBJECTS include/image.h src/image.cpp)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)
add_sc_object(svg2png LINK_LIBRARIES sc::sc-core lunasvg::lunasvg PUBLIC_LINK_LIBRARIES lunasvg::lunasvg)
add_sc_object(opencv LINK_LIBRARIES sc::sc-core ${OpenCV_LIBS} PUBLIC_LINK_LIBRARIES sc::sc-core ${OpenCV_LIBS})

######################
# Exported Libraries #
######################
add_sc_libraries(DESCRIPTION "simply-cpp-image")

################
# Installation #
################
install_sc_module()

#########
# Tests #
#########
if (SC_IMAGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests sc-image-test)
endif ()

############
# Binaries #
############

add_executable(sc-image-demo apps/sc-image-demo.cpp)
target_link_libraries(sc-image-demo PRIVATE sc::sc-core-shared sc::sc-image-shared)
install(TARGETS sc-image-demo RUNTIME DESTINATION bin COMPONENT runtime)

#############
# Packaging #
#############
set(CPACK_COMPONENTS_ALL runtime development)
package_sc_module()