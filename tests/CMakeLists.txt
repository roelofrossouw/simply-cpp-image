# Resources for Tests #
set(RESOURCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resource")
set(RESOURCE_DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")
add_custom_target(copy_sc_image_resources ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCE_SOURCE_DIR} ${RESOURCE_DESTINATION_DIR})

## TESTS ##
# What every test in this directory links against.
set(SC_TEST_LINK_LIBRARIES sc-image)

add_sc_test(svg2png LABELS unit)
add_sc_test(opencv LABELS unit)
add_dependencies(test-svg2png copy_sc_image_resources)
add_dependencies(test-opencv copy_sc_image_resources)
