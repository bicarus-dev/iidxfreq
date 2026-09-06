cmake_minimum_required(VERSION 3.24)
project(iidxfreq LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT MINGW OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Use build_docker.bat with the x64 MinGW toolchain.")
endif()
add_link_options(-static)

set(SPICE_SDK_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/spice-sdk")

include(FetchContent)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
set(SOUNDSTRETCH OFF CACHE BOOL "" FORCE)
set(SOUNDTOUCH_DLL OFF CACHE BOOL "" FORCE)
set(OPENMP OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(soundtouch
    GIT_REPOSITORY https://codeberg.org/soundtouch/soundtouch.git
    GIT_TAG e83424d5928ab8513d2d082779c275765dee31b9)
FetchContent_MakeAvailable(soundtouch)

FetchContent_Declare(simpleini
    GIT_REPOSITORY https://github.com/brofield/simpleini.git
    GIT_TAG fd6db69efc40a687bf4ef81486b54066e34992dd)
FetchContent_MakeAvailable(simpleini)

add_library(iidxfreq_audio STATIC src/audio.cpp)
target_include_directories(iidxfreq_audio PUBLIC src)
target_link_libraries(iidxfreq_audio PUBLIC SoundTouch)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    FetchContent_Declare(minhook
        GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
        GIT_TAG c3fcafdc10146beb5919319d0683e44e3c30d537)
    FetchContent_MakeAvailable(minhook)
    add_library(iidxfreq SHARED
        src/iidxfreq.cpp
        src/config.cpp
        src/runtime.cpp
        src/module.cpp
        src/game_hooks.cpp
        src/wave.cpp)
    target_include_directories(iidxfreq PRIVATE "${SPICE_SDK_INCLUDE_DIR}")
    target_compile_definitions(iidxfreq PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX SI_NO_CONVERSION)
    target_link_libraries(iidxfreq PRIVATE iidxfreq_audio minhook SimpleIni::SimpleIni user32)
    set_target_properties(iidxfreq PROPERTIES OUTPUT_NAME "iidxfreq" PREFIX "")
    add_custom_command(TARGET iidxfreq POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/iidxfreq.ini $<TARGET_FILE_DIR:iidxfreq>/iidxfreq.ini)
endif()