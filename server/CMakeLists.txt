cmake_minimum_required(VERSION 3.10)
project(aesdsocket_app C)

# Define the executable name and its source file
add_executable(aesdsocket aesdsocket.c thr_cnx.c util.c)

target_compile_options(aesdsocket PRIVATE -Wall -Werror)
target_compile_definitions(aesdsocket PRIVATE USE_AESD_CHAR_DEVICE=1)

# Enable ASan and UBSan for Debug builds
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(aesdsocket PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_libraries(aesdsocket PRIVATE -fsanitize=address,undefined)
endif()
