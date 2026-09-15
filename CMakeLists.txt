cmake_minimum_required(VERSION 3.22)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
project(simply-cpp-image)

find_package(Git QUIET)
if (Git_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE SC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE GIT_RESULT
    )

    if (GIT_RESULT EQUAL 0)
        message(STATUS "Git tag: ${SC_IMAGE_VERSION}")
        string(REGEX REPLACE "^v" "" SC_IMAGE_VERSION "${SC_IMAGE_VERSION}")
        set(SC_IMAGE_VERSION "1.0.0")
    else ()
        set(SC_IMAGE_VERSION "1.0.0")
    endif ()
else ()
    set(SC_IMAGE_VERSION "1.0.0")
endif ()
message(STATUS "SC Image Version: ${SC_IMAGE_VERSION}")

# Options:
option(SC_IMAGE_BUILD_TESTS "Build simply-cpp tests" ${PROJECT_IS_TOP_LEVEL})
option(FETCH_SC "Always fetch Simply-CPP" OFF)
# option(FETCH_LUNASVG "Always fetch LunaSVG" OFF)

include(FetchContent)

######################
#  Dependencies Used #
######################

if (NOT FETCH_SC)
    find_package(sc QUIET)
endif ()

if (sc_FOUND)
    message(STATUS "Simply-CPP installed - ${sc_VERSION}")
else ()
    FetchContent_Declare(sc GIT_REPOSITORY https://github.com/roelofrossouw/simply-cpp GIT_TAG origin/main GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(sc)
endif ()

# Fetching luna source so we can consume instead of linking and having a dependency.
#if (NOT FETCH_LUNASVG)
#    find_package(lunasvg QUIET)
#endif ()
#
#if (lunasvg_FOUND)
#    message(STATUS "LunaSVG installed - ${lunasvg_VERSION}")
#else ()
    set(lunasvg_VERSION v3.5.0)
    message(STATUS "Fetching LunaSVG from github - ${lunasvg_VERSION}")
    set(LUNASVG_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(lunasvg GIT_REPOSITORY https://github.com/sammycage/lunasvg.git GIT_TAG ${lunasvg_VERSION} GIT_SHALLOW TRUE EXCLUDE_FROM_ALL)
    FetchContent_MakeAvailable(lunasvg)
    set(lunasvg_sources
            ${lunasvg_SOURCE_DIR}/source/lunasvg.cpp
            ${lunasvg_SOURCE_DIR}/source/graphics.cpp
            ${lunasvg_SOURCE_DIR}/source/svgelement.cpp
            ${lunasvg_SOURCE_DIR}/source/svggeometryelement.cpp
            ${lunasvg_SOURCE_DIR}/source/svglayoutstate.cpp
            ${lunasvg_SOURCE_DIR}/source/svgpaintelement.cpp
            ${lunasvg_SOURCE_DIR}/source/svgparser.cpp
            ${lunasvg_SOURCE_DIR}/source/svgproperty.cpp
            ${lunasvg_SOURCE_DIR}/source/svgrenderstate.cpp
            ${lunasvg_SOURCE_DIR}/source/svgtextelement.cpp
    )

    set(lunasvg_headers
            ${lunasvg_SOURCE_DIR}/plutovg/include/plutovg.h
            ${lunasvg_SOURCE_DIR}/include/lunasvg.h
            ${lunasvg_SOURCE_DIR}/source/graphics.h
            ${lunasvg_SOURCE_DIR}/source/svgelement.h
            ${lunasvg_SOURCE_DIR}/source/svggeometryelement.h
            ${lunasvg_SOURCE_DIR}/source/svglayoutstate.h
            ${lunasvg_SOURCE_DIR}/source/svgpaintelement.h
            ${lunasvg_SOURCE_DIR}/source/svgparserutils.h
            ${lunasvg_SOURCE_DIR}/source/svgproperty.h
            ${lunasvg_SOURCE_DIR}/source/svgrenderstate.h
            ${lunasvg_SOURCE_DIR}/source/svgtextelement.h
    )

    set(plutovg_sources
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-blend.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-canvas.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-font.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-matrix.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-paint.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-path.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-rasterize.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-surface.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-math.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-raster.c
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-stroker.c
    )

    set(plutovg_headers
            ${lunasvg_SOURCE_DIR}/plutovg/include/plutovg.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-private.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-utils.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-math.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-raster.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-stroker.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-ft-types.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-stb-image-write.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-stb-image.h
            ${lunasvg_SOURCE_DIR}/plutovg/source/plutovg-stb-truetype.h
    )
#endif ()

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
        message(FATAL_ERROR "Failed to install or locate OpenCV (result=${INSTALL_RESULT})")
    endif ()
endif ()

###########
# Modules #
###########
set(SOURCE_OBJECTS src/image.cpp include/image.h)
set(SOURCE_LIBRARIES)

set(OBJECT_NAME sc-svg)
add_library(${OBJECT_NAME} OBJECT src/svg2png.cpp include/svg2png.h
        ${lunasvg_sources} ${lunasvg_headers}
        ${plutovg_sources} ${plutovg_headers})
set_target_properties(${OBJECT_NAME} PROPERTIES EXCLUDE_FROM_ALL ON)
#target_include_directories(${OBJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_include_directories(${OBJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(${OBJECT_NAME} PRIVATE ${lunasvg_SOURCE_DIR}/include)
target_include_directories(${OBJECT_NAME} PRIVATE ${lunasvg_SOURCE_DIR}/plutovg/include)
target_link_libraries(${OBJECT_NAME} PRIVATE sc) #  lunasvg::lunasvg)
# list(APPEND SOURCE_LIBRARIES ${OBJECT_NAME})
list(APPEND SOURCE_OBJECTS $<TARGET_OBJECTS:${OBJECT_NAME}>)

set(OBJECT_NAME sc-opencv)
add_library(${OBJECT_NAME} STATIC src/opencv.cpp include/opencv.h)
set_target_properties(${OBJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${OBJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${OBJECT_NAME} PUBLIC sc ${OpenCV_LIBS})
list(APPEND SOURCE_LIBRARIES ${OBJECT_NAME})

#############
# Libraries #
#############

# Combined static libary for easy usage
project(sc-image VERSION ${SC_IMAGE_VERSION} DESCRIPTION "simply-cpp-image consolidated static library")
add_library(${PROJECT_NAME} STATIC ${SOURCE_OBJECTS})
add_library(sc::sc-image ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC ${SOURCE_LIBRARIES})

# Combined shared libary for easy usage
project(sc-image-shared VERSION ${SC_IMAGE_VERSION} DESCRIPTION "simply-cpp-image common shared library")
add_library(${PROJECT_NAME} SHARED ${SOURCE_OBJECTS})
add_library(sc::sc-image-shared ALIAS ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})
target_include_directories(${PROJECT_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(${PROJECT_NAME} PUBLIC ${SOURCE_LIBRARIES})

list(APPEND SOURCE_LIBRARIES sc-image)
list(APPEND SOURCE_LIBRARIES sc-image-shared)

install(TARGETS ${SOURCE_LIBRARIES} EXPORT sc-imageTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT sc-imageTargets FILE sc-imageTargets.cmake NAMESPACE sc:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-image)
configure_package_config_file(cmake/sc-imageConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/sc-imageConfig.cmake INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sc-image)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/sc-imageConfigVersion.cmake VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sc-imageConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/sc-imageConfigVersion.cmake DESTINATION lib/cmake/sc-image)

###########
#  Tests  #
###########
if (SC_IMAGE_BUILD_TESTS)
    add_subdirectory(tests sc-image-test EXCLUDE_FROM_ALL)
endif ()