cmake_minimum_required(VERSION 3.16)
project(Segfault LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ------------------------------------------------------------------ raylib --
# Uses an installed raylib 5.5 if found, otherwise downloads it automatically.
find_package(raylib 5.5 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(raylib)
endif()

# ------------------------------------------------- exam solutions (no UI) --
# The reference solutions are a plain C/C++ library: the app AND the unit
# tests link against exactly the same code.
add_library(exam_solutions STATIC
    src/solutions/exam_output.cpp
    src/solutions/exam1.cpp
    src/solutions/exam2_pointers.cpp
    src/solutions/linked_list.cpp)
target_include_directories(exam_solutions PUBLIC src/solutions)

# ------------------------------------------------------------- the app ----
file(GLOB APP_SOURCES CONFIGURE_DEPENDS
    src/app/*.cpp src/content/*.cpp src/scenes/*.cpp)
add_executable(Segfault ${APP_SOURCES})
target_include_directories(Segfault PRIVATE src)
target_link_libraries(Segfault PRIVATE raylib exam_solutions)
if (APPLE)
    target_link_libraries(Segfault PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
endif()

# Copy fonts + the solution sources (shown inside the app) next to the exe.
add_custom_command(TARGET Segfault POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/resources $<TARGET_FILE_DIR:Segfault>/resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/solutions $<TARGET_FILE_DIR:Segfault>/resources/solutions)

# Windows: ship TinyCC so C problems can be compiled even without Visual Studio.
if (WIN32)
    add_custom_command(TARGET Segfault POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/third_party/tcc-win64 $<TARGET_FILE_DIR:Segfault>/resources/tcc)
endif()

if (MINGW)
    # no black console window behind the app
    target_link_options(Segfault PRIVATE -mwindows)
endif()

if (MSVC)
    target_compile_options(Segfault PRIVATE /W3 /utf-8)
    target_compile_options(exam_solutions PRIVATE /W3 /utf-8)
    target_compile_definitions(exam_solutions PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_definitions(Segfault PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Segfault)
else()
    target_compile_options(Segfault PRIVATE -Wall -Wextra)
    target_compile_options(exam_solutions PRIVATE -Wall -Wextra)
endif()

# ------------------------------------------------------------ unit tests --
enable_testing()
add_executable(pq_tests tests/test_solutions.cpp)
target_link_libraries(pq_tests PRIVATE exam_solutions)
if (MSVC)
    target_compile_definitions(pq_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
add_test(NAME exam_solutions COMMAND pq_tests)

add_executable(auth_tests tests/test_auth.cpp src/app/auth.cpp)
if (MSVC)
    target_compile_options(auth_tests PRIVATE /utf-8)
endif()
add_test(NAME auth COMMAND auth_tests)

add_executable(scrollbar_tests tests/test_scrollbar.cpp)
add_test(NAME scrollbar COMMAND scrollbar_tests)

add_executable(paste_tests tests/test_paste.cpp)
add_test(NAME paste COMMAND paste_tests)

add_executable(bracket_tests tests/test_brackets.cpp)
add_test(NAME brackets COMMAND bracket_tests)
