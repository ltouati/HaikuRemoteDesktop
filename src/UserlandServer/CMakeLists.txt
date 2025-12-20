cmake_minimum_required(VERSION 3.10)
project(ScreenServer)

# Haiku build settings
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.1 -mssse3 -mavx -mavx2")

if (RELEASE_MODE)
    add_definitions(-DRELEASE_MODE)
endif ()

# Include directories
include_directories(
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ../InputDriver
)

# Find Protobuf
find_package(Protobuf REQUIRED)

# Find OpenSSL
find_package(OpenSSL REQUIRED)

# Find LibVPX
find_library(VPX_LIBRARY vpx REQUIRED)

# Generate Sources
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS messages.proto)

# Generate messages.js
# Generate messages.js
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/messages.js
        COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/protobufjs-cli/bin/pbjs -t static-module -w closure -o ${CMAKE_CURRENT_BINARY_DIR}/messages.js ${CMAKE_CURRENT_SOURCE_DIR}/messages.proto
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/messages.proto
        COMMENT "Generating messages.js from messages.proto into build dir"
)

# Build the C++ Screen Server

# Haiku Resources
find_program(RC_COMPILER rc)
if (RC_COMPILER)
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/HaikuRemoteDesktop.rsrc
            COMMAND ${RC_COMPILER} -o ${CMAKE_CURRENT_BINARY_DIR}/HaikuRemoteDesktop.rsrc ${CMAKE_CURRENT_SOURCE_DIR}/HaikuRemoteDesktop.rdef
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/HaikuRemoteDesktop.rdef
    )
    list(APPEND SCREEN_SERVER_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/HaikuRemoteDesktop.rsrc)
endif ()

add_executable(screen_server
        ${SCREEN_SERVER_SOURCES}
        server.cpp
        ScreenCapture.cpp
        VideoEncoder.cpp
        NetworkServer.cpp
        NetworkUtils.cpp
        NetworkUtils.cpp
        InputDriverManager.cpp
        handlers/MousePacketHandler.cpp
        handlers/KeyPacketHandler.cpp
        handlers/PingPacketHandler.cpp
        handlers/ResolutionPacketHandler.cpp
        handlers/CodecPacketHandler.cpp
        handlers/ClipboardPacketHandler.cpp
        handlers/PacketHandlerFactory.cpp
        messages.pb.cc
        ${CMAKE_CURRENT_BINARY_DIR}/messages.js
        ${CMAKE_CURRENT_BINARY_DIR}/style.css
)

# Generate style.css
add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/style.css
        COMMAND node ${CMAKE_CURRENT_SOURCE_DIR}/node_modules/tailwindcss/lib/cli.js -i ${CMAKE_CURRENT_SOURCE_DIR}/input.css -o ${CMAKE_CURRENT_BINARY_DIR}/style.css
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/input.css ${CMAKE_CURRENT_SOURCE_DIR}/tailwind.config.js ${CMAKE_CURRENT_SOURCE_DIR}/index.html
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating style.css from input.css"
)

include_directories(handlers)

target_link_libraries(screen_server
        be
        network
        game
        ${VPX_LIBRARY}
        ${Protobuf_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
)

# Install/Copy the binary to dist
add_custom_command(TARGET screen_server POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:screen_server> ${CMAKE_BINARY_DIR}/dist/screen_server
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/index.html ${CMAKE_BINARY_DIR}/dist/index.html
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/messages.js ${CMAKE_BINARY_DIR}/dist/messages.js
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/style.css ${CMAKE_BINARY_DIR}/dist/style.css
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/style.css ${CMAKE_BINARY_DIR}/dist/style.css
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:virtual_input_server_addon> ${CMAKE_BINARY_DIR}/dist/virtual_input_server_addon.so
)
