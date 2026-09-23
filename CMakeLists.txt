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
if (FETCH_SC)
    find_or_fetch_package(sc-core FORCE ${FETCH_SC} GIT_REPOSITORY ${SC_HELPERS_REPOSITORY} GIT_TAG ${SC_HELPERS_TAG})
else ()
    find_or_install_package(sc-core simply-cpp-dev simply-cpp)
endif ()

## LunaSVG ##
# simply-cpp-lunasvg on apt, lunasvg on brew - a real library to link against
# instead of compiling its source into our own objects (which also can't work
# inside Homebrew's sandboxed builds, since those refuse FetchContent).
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