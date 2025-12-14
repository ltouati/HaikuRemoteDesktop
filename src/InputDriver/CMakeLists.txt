add_library(virtual_input_server_addon SHARED VirtualMouse.cpp)

# Link against BeAPI
target_link_libraries(virtual_input_server_addon be)

# Haiku Add-on specific flags
target_compile_options(virtual_input_server_addon PRIVATE -fPIC)

# Install location (internal variable, used by install script later)
set_target_properties(virtual_input_server_addon PROPERTIES PREFIX "")