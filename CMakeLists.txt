cmake_minimum_required(VERSION 3.24)
project(MCnotepad VERSION 0.1.0 LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MCnotepad WIN32
    src/main.cpp
    resources/app.rc
)

target_compile_definitions(MCnotepad PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MCnotepad PRIVATE comctl32 comdlg32 shlwapi advapi32)

target_include_directories(MCnotepad PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    target_compile_options(MCnotepad PRIVATE /W4 /permissive- /EHsc /utf-8)
    target_link_options(MCnotepad PRIVATE /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(MCnotepad PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_custom_command(TARGET MCnotepad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/resources
            $<TARGET_FILE_DIR:MCnotepad>/resources
)
