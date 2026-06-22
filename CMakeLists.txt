cmake_minimum_required(VERSION 3.22)

project(SlamminTuner
    VERSION 1.1.6
    DESCRIPTION "Slammin Tuner realtime strobe tuner plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SLAMMIN_TUNER_FETCH_JUCE "Fetch JUCE with CMake FetchContent when JUCE_DIR is not provided" ON)

if (DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    if (NOT SLAMMIN_TUNER_FETCH_JUCE)
        message(FATAL_ERROR "JUCE_DIR was not provided and SLAMMIN_TUNER_FETCH_JUCE is OFF")
    endif()

    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.12
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

add_subdirectory(src/strobe_tuner)

