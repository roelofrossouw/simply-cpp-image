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
find_or_install_package(sc-core simply-cpp-dev simply-cpp 3.0.0) # log_level.h
# sc::canvas, which image implements, and sc::drawing, which svg2png reads SVG with.
find_or_install_package(sc-graphics simply-cpp-graphics-dev simply-cpp-graphics 7.0.0)


## OpenCV ##
set(SC_IMAGE_OPENCV_COMPONENTS core ml imgproc imgcodecs dnn videoio)
if (NOT APPLE)
    list(APPEND SC_IMAGE_OPENCV_COMPONENTS calib3d)
endif ()
find_or_install_package(OpenCV libopencv-dev opencv COMPONENTS ${SC_IMAGE_OPENCV_COMPONENTS})

###########
# Objects #
###########
set(SOURCE_OBJECTS include/sc/image/image.h)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)
# SVG to PNG: sc-graphics' drawing, drawn by the opencv object.
add_sc_object(svg2png LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS})
# On Linux it also quiets libjpeg's warnings below image's log level, needing libjpeg's header
# (the library itself comes with OpenCV).
set(SC_IMAGE_JPEG_INCLUDE)
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_or_install_package(JPEG libjpeg-dev jpeg)
    set(SC_IMAGE_JPEG_INCLUDE ${JPEG_INCLUDE_DIRS})
endif ()
add_sc_object(opencv INCLUDE_DIRS ${SC_IMAGE_JPEG_INCLUDE} LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS} ${CMAKE_DL_LIBS}
        PUBLIC_LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS} ${CMAKE_DL_LIBS})
# Whether a camera's picture is fit to look at: covered, dark, blurred, one colour, broken.
add_sc_object(image_quality LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS})
# Video files read frame by frame (OpenCV's videoio), each frame an sc::image.
add_sc_object(video LINK_LIBRARIES sc::sc-core sc::sc-graphics ${OpenCV_LIBS}
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

############
# Programs #
############
# Tools, not demos, so always built and installed with the runtime package, with a man page:
# svg2img (SVG files to PNG, JPEG, WebP, BMP or TIFF) and image-quality (camera pictures' quality,
# as lines of numbers and an HTML contact sheet, worst first).
foreach (program svg2img image-quality)
    add_executable(${program} apps/${program}.cpp)
    target_link_libraries(${program} PRIVATE sc::sc-core-shared sc::sc-image-shared)
    sc_program_info(${program})
    install(TARGETS ${program} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime)
    sc_man_page(${program} COMPONENT runtime)
endforeach ()

#############
# Packaging #
#############
set(CPACK_COMPONENTS_ALL runtime development)
# The library has sc-core built in; the tools and demos load libsc-core-shared (simply-cpp), and
# the CMake config finds sc-core (simply-cpp-dev).
package_sc_module(DEPENDS "simply-cpp (>= 3.0.0~)" "simply-cpp-graphics (>= 7.0.0~)"
        DEV_DEPENDS "simply-cpp-dev (>= 3.0.0~)" "simply-cpp-graphics-dev (>= 7.0.0~)" libopencv-dev)