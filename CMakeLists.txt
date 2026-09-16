cmake_minimum_required(VERSION 3.22)
set(SC_MODULE image)
project(simply-cpp-${SC_MODULE} LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options:
option(SC_IMAGE_BUILD_TESTS "Build simply-cpp-image tests" ${PROJECT_IS_TOP_LEVEL})
option(FETCH_SC "Always fetch simply-cpp core rather than using an installed one" OFF)
if (FETCH_SC)
    # The bootstrap has to leave an installed core alone as well: find_package() there
    # imports sc::sc-core, and the source we are about to build cannot then define it.
    set(SC_HELPERS_USE_PACKAGE OFF)
endif ()

# The build helpers - get_sc_version(), add_sc_object(), add_sc_libraries(),
# install_sc_module(), add_sc_test() - come from core, from an installed sc-core, the
# copy in cmake/, or the repository, whichever the bootstrap finds first.
include(cmake/sc_bootstrap.cmake)

get_sc_version()
message(STATUS "SimplyCPP ${SC_MODULE} Version: ${SC_VERSION}")

################
# Dependencies #
################

## simply-cpp core ##
find_or_fetch_package(sc-core FORCE ${FETCH_SC} GIT_REPOSITORY ${SC_HELPERS_REPOSITORY} GIT_TAG ${SC_HELPERS_TAG})

## LunaSVG ##
# Built from source rather than linked, so the consolidated libraries carry it and a
# consumer needs nothing extra.
set(lunasvg_VERSION v3.5.0)
message(STATUS "Fetching LunaSVG from github - ${lunasvg_VERSION}")
set(LUNASVG_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lunasvg GIT_REPOSITORY https://github.com/sammycage/lunasvg.git GIT_TAG ${lunasvg_VERSION}
        GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(lunasvg)

file(GLOB lunasvg_sources ${lunasvg_SOURCE_DIR}/source/*.cpp)
file(GLOB plutovg_sources ${lunasvg_SOURCE_DIR}/plutovg/source/*.c)

## OpenCV ##
# Named once: find_or_install_package() uses them here and sc-imageConfig.cmake.in
# substitutes them, so a consumer resolves the same components.
if (APPLE)
    set(SC_IMAGE_OPENCV_COMPONENTS core ml imgproc imgcodecs dnn highgui)
else ()
    set(SC_IMAGE_OPENCV_COMPONENTS core ml imgproc imgcodecs dnn highgui calib3d)
endif ()
find_or_install_package(OpenCV libopencv-dev opencv COMPONENTS ${SC_IMAGE_OPENCV_COMPONENTS})

###########
# Objects #
###########
set(SOURCE_OBJECTS include/image.h src/image.cpp)
set(SOURCE_LIBRARIES)
set(SOURCE_LINK_LIBRARIES)

add_sc_object(svg2png
        SOURCES ${lunasvg_sources} ${plutovg_sources}
        INCLUDE_DIRS ${lunasvg_SOURCE_DIR}/include ${lunasvg_SOURCE_DIR}/plutovg/include
        LINK_LIBRARIES sc::sc-core)

add_sc_object(opencv
        LINK_LIBRARIES sc::sc-core ${OpenCV_LIBS}
        PUBLIC_LINK_LIBRARIES sc::sc-core ${OpenCV_LIBS})

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
