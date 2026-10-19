cmake_minimum_required(VERSION 3.22)
set(SC_MODULE image)
project(simply-cpp-${SC_MODULE} HOMEPAGE_URL "https://github.com/roelofrossouw/simply-cpp-image" LANGUAGES CXX)
include(cmake/sc_bootstrap.cmake)

# Options:
option(SC_IMAGE_BUILD_TESTS "Build simply-cpp-image tests" ${PROJECT_IS_TOP_LEVEL})
get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################

# The demos use sc::console subheadings and SC_SHOW with commas (1.5.15).
find_or_install_package(sc-core simply-cpp-dev simply-cpp 1.9.0)
# sc::canvas, which image implements, and sc::drawing, which svg2png reads SVG with.
find_or_install_package(sc-graphics simply-cpp-graphics-dev simply-cpp-graphics 5.1.0)


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
# SVG to PNG: sc-graphics' drawing, drawn by the opencv object.
add_sc_object(svg2png LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS})
add_sc_object(opencv LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS}
        PUBLIC_LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS})

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
# Examples #
############
add_sc_example(sc-image-demo LINK_LIBRARIES sc::sc-core-shared sc::sc-image-shared)
add_sc_example(sc-image-shapes LINK_LIBRARIES sc::sc-core-shared sc::sc-image-shared)
add_sc_example(sc-image-transform LINK_LIBRARIES sc::sc-core-shared sc::sc-image-shared)
# A tool, not a demo: SVG files to PNG, JPEG, WebP, BMP or TIFF.
add_sc_example(svg2img LINK_LIBRARIES sc::sc-core-shared sc::sc-image-shared)

#############
# Packaging #
#############
set(CPACK_COMPONENTS_ALL runtime development)
package_sc_module(DEPENDS "simply-cpp-dev (>= 1.9.0~)" "simply-cpp-graphics (>= 5.1.0~)" DEV_DEPENDS "simply-cpp-graphics-dev (>= 5.1.0~)" libopencv-dev)