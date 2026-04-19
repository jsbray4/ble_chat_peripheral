# Define minimal required version of CMake.
cmake_minimum_required(VERSION "3.25")

# Project definition
project(
	ble_chat_peripheral
	VERSION 1.0
	LANGUAGES C CXX ASM
)

# Include the definition of the slc_ble_chat_peripheral target,
# which contains the content of the SLC project
include(ble_chat_peripheral.cmake)

add_executable(ble_chat_peripheral
    # Add additional sources here
)

target_include_directories(ble_chat_peripheral PUBLIC
    # Add additional include paths here
)

target_compile_definitions(ble_chat_peripheral PUBLIC
    # Add additional macros here
)

target_compile_options(ble_chat_peripheral PUBLIC
    # Set additional compiler flags here
)

target_link_options(ble_chat_peripheral PUBLIC
    # Set additional linker flags here
)

# Link with the content defined in the SLC project
target_link_libraries(ble_chat_peripheral PRIVATE
    slc
)

# Include managed project content if available
include(ble_chat_peripheral_project.cmake OPTIONAL RESULT_VARIABLE managed_project)
if(managed_project)
    message(STATUS "Using managed project content from ${managed_project}")
endif()

# Force the gcc linker command
set_target_properties(ble_chat_peripheral PROPERTIES LINKER_LANGUAGE C)

# Create .bin, .hex and .s37 artifacts after building the project
add_custom_command(TARGET ble_chat_peripheral
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_SREC_CMD} "$<TARGET_FILE:ble_chat_peripheral>" "$<TARGET_FILE_DIR:ble_chat_peripheral>/$<TARGET_FILE_BASE_NAME:ble_chat_peripheral>.s37"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_IHEX_CMD} "$<TARGET_FILE:ble_chat_peripheral>" "$<TARGET_FILE_DIR:ble_chat_peripheral>/$<TARGET_FILE_BASE_NAME:ble_chat_peripheral>.hex"
    COMMAND ${CMAKE_OBJCOPY} ${OBJCOPY_BIN_CMD}  "$<TARGET_FILE:ble_chat_peripheral>" "$<TARGET_FILE_DIR:ble_chat_peripheral>/$<TARGET_FILE_BASE_NAME:ble_chat_peripheral>.bin" 
)

# Run post-build pipeline to perform additional post-processing
if(post_build_command)
add_custom_command(TARGET ble_chat_peripheral
    POST_BUILD
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
    COMMAND ${post_build_command}
)
endif()