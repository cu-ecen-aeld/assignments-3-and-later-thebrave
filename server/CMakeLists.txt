cmake_minimum_required(VERSION 3.10)
project(aesdsocket_app C)

# Define the executable name and its source file
add_executable(aesdsocket aesdsocket.c)

# Enable ASan and UBSan for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(aesdsocket PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_libraries(aesdsocket PRIVATE -fsanitize=address,undefined)
endif()