cmake_minimum_required(VERSION 3.30)
project(spectacle_autoimgur C)

set(CMAKE_C_STANDARD 99)

add_executable(spectacle_autoimgur main.c cJSON.c)

target_link_libraries(spectacle_autoimgur -lcurl)
