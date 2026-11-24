cmake_minimum_required(VERSION 3.24)
project(MCnotepad VERSION 0.1.0 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Chinese -> pinyin (Apache-2.0), built as a static library.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CPP_PINYIN_INSTALL OFF CACHE BOOL "" FORCE)
set(CPP_PINYIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/cpp-pinyin EXCLUDE_FROM_ALL)

# Simplified <-> Traditional Chinese (Apache-2.0). Library only: MCnotepad
# ships plain-text dictionaries in resources/opencc, so the dictionary build
# pipeline and command-line tools are not needed.
set(OPENCC_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(OPENCC_BUILD_DATA OFF CACHE BOOL "" FORCE)
set(OPENCC_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_GTEST OFF CACHE BOOL "" FORCE)
set(ENABLE_BENCHMARK OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/opencc EXCLUDE_FROM_ALL)

# Faker C++ (MIT) supplies locale-aware person, address, commerce, internet,
# lorem and identifier data for the Data menu.
set(FAKER_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(FAKER_CODE_COVERAGE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/faker-cxx EXCLUDE_FROM_ALL)

# Duktape embeds the JavaScript runtime used by the bundled Showdown converter.
# Only the amalgamated engine is compiled; no shell, filesystem or networking
# bindings are enabled.
add_library(duktape STATIC
    third_party/duktape-2.7.0/src/duktape.c
)
target_include_directories(duktape PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/duktape-2.7.0/src)
if(MSVC)
    target_compile_options(duktape PRIVATE /W0 /wd4244 /wd4267 /wd4996 /wd4706)
endif()
if(MSVC)
    # Vendored sources are UTF-8; don't let MSVC read them as the ANSI code page.
    foreach(_vendored libopencc marisa cpp-pinyin faker-cxx)
        if(TARGET ${_vendored})
            target_compile_options(${_vendored} PRIVATE /utf-8 /W0)
        endif()
    endforeach()
endif()

add_executable(MCnotepad WIN32
    src/main.cpp
    resources/app.rc
)

target_compile_definitions(MCnotepad PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MCnotepad PRIVATE comctl32 comdlg32 shlwapi advapi32 dwmapi uxtheme crypt32 cpp-pinyin::cpp-pinyin OpenCC::OpenCC faker-cxx duktape)

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

# Pinyin dictionaries are read at runtime from <exe dir>/dict/mandarin.
add_custom_command(TARGET MCnotepad POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cpp-pinyin/res/dict/mandarin
            $<TARGET_FILE_DIR:MCnotepad>/dict/mandarin
)
