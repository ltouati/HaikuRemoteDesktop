cmake_minimum_required(VERSION 3.10)
project(HaikuRemoteDesktop)

# Global Output Directory (keeps things clean)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/dist)

include_directories("/boot/system/develop/headers/private/kernel")

# Add Sub-projects
add_subdirectory(src/InputDriver)
add_subdirectory(src/UserlandServer)

message(STATUS "Build configured. output will be in: ${CMAKE_BINARY_DIR}/dist")

# Haiku Package Creation
set(PACKAGE_NAME "HaikuRemoteDesktop-1.0.0-1-x86_64.hpkg")
set(STAGING_DIR "${CMAKE_BINARY_DIR}/package_staging")

add_custom_target(package_haiku
    COMMENT "Building Haiku Package..."
    
    # 1. Clean Staging
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${STAGING_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}
    
    # 2. Create Directory Structure
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/apps/HaikuRemoteDesktop
    COMMAND ${CMAKE_COMMAND} -E make_directory ${STAGING_DIR}/data/deskbar/menu/Applications
    
    # 3. Copy Binaries
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:screen_server> ${STAGING_DIR}/apps/HaikuRemoteDesktop/HaikuRemoteDesktop
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:virtual_input_server_addon> ${STAGING_DIR}/apps/HaikuRemoteDesktop/
    
    # 4. Copy Web Assets
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/UserlandServer/index.html ${STAGING_DIR}/apps/HaikuRemoteDesktop/index.html
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/src/UserlandServer/messages.js ${STAGING_DIR}/apps/HaikuRemoteDesktop/messages.js
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/src/UserlandServer/style.css ${STAGING_DIR}/apps/HaikuRemoteDesktop/style.css

    # 5. Copy License
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/LICENSE ${STAGING_DIR}/MIT

    # 6. Create Deskbar Symlink
    COMMAND ${CMAKE_COMMAND} -E create_symlink ../../../../apps/HaikuRemoteDesktop/HaikuRemoteDesktop ${STAGING_DIR}/data/deskbar/menu/Applications/HaikuRemoteDesktop
    
    # 7. Create Package (Remove -b flag!)
    COMMAND package create -v -i ${CMAKE_SOURCE_DIR}/PackageInfo -C ${STAGING_DIR} ${CMAKE_BINARY_DIR}/${PACKAGE_NAME}
    
    DEPENDS screen_server virtual_input_server_addon
)