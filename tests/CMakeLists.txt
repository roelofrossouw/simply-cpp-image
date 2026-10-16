# Resources for Tests #
set(RESOURCE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resource")
set(RESOURCE_DESTINATION_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")
add_custom_target(copy_sc_image_resources ALL COMMAND ${CMAKE_COMMAND} -E copy_directory ${RESOURCE_SOURCE_DIR} ${RESOURCE_DESTINATION_DIR})

## TESTS ##
# What every test in this directory links against.
set(SC_TEST_LINK_LIBRARIES sc-image)

add_sc_test(svg2png LABELS unit)
add_dependencies(test-svg2png copy_sc_image_resources)
add_sc_test(opencv LABELS unit)
add_dependencies(test-opencv copy_sc_image_resources)

# The svg2img tool, run as a user would: a file next to its input, and another type and size.
add_test(NAME test-svg2img COMMAND svg2img ${RESOURCE_DESTINATION_DIR}/logo18.svg ${CMAKE_CURRENT_BINARY_DIR}/svg2img-logo.jpg --width 64 --quality 80)
set_tests_properties(test-svg2img PROPERTIES LABELS unit PASS_REGULAR_EXPRESSION "svg2img-logo.jpg: 64x[0-9]+ jpg")
add_test(NAME test-svg2img-mistakes COMMAND svg2img ${RESOURCE_DESTINATION_DIR}/logo18.svg ${CMAKE_CURRENT_BINARY_DIR}/out.png --type jpg)
set_tests_properties(test-svg2img-mistakes PROPERTIES LABELS unit WILL_FAIL TRUE)
