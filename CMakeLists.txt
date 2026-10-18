cmake_minimum_required(VERSION 3.28)
project(lpp LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(LPP_WERROR "警告をエラーとして扱う（検証タスク用）" OFF)
option(LPP_SANITIZE "Debug ビルドで address/undefined サニタイザを有効にする" ON)
option(LPP_COVERAGE "カバレッジ計測を有効にする（mise run coverage 用）" OFF)

# 段階ごとにソースが増えるため、ファイル追加のたびに CMakeLists を編集せずに済むよう
# CONFIGURE_DEPENDS 付きの glob を使う。ビルドし直せば新しい .c を拾う。
file(GLOB LPP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(mpplc ${LPP_SOURCES})
target_include_directories(mpplc PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_options(mpplc PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes)

if(LPP_WERROR)
  target_compile_options(mpplc PRIVATE -Werror)
endif()

# clang のソースベースカバレッジ。実行すると .profraw が落ちるので、計測は専用のビルドツリー
# （build-coverage）で行う。通常のビルドには入れない。
if(LPP_COVERAGE)
  target_compile_options(mpplc PRIVATE -fprofile-instr-generate -fcoverage-mapping)
  target_link_options(mpplc PRIVATE -fprofile-instr-generate)
endif()

if(LPP_SANITIZE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_options(mpplc PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
  target_link_options(mpplc PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_subdirectory(tests)
