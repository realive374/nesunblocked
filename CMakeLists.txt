cmake_minimum_required(VERSION 3.21)
project(RetroTube90 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(RT90_BUILD_APP   "Build the SDL2 desktop app" ON)
option(RT90_BUILD_TESTS "Build unit / integration tests" ON)
option(RT90_WERROR      "Treat compiler warnings as errors" ON)
option(RT90_STATIC_SDL  "Link SDL2 statically" ${WIN32})
option(RT90_WITH_FFMPEG "Enable MP4 import with FFmpeg" ON)

if(MINGW)
  # Self-contained .exe: no libstdc++ / libgcc / winpthread DLLs needed.
  add_link_options(-static -static-libgcc -static-libstdc++)
endif()

# Warning flags applied to our own targets only.
add_library(rt90_warnings INTERFACE)
if(MSVC)
  target_compile_options(rt90_warnings INTERFACE /W4 $<$<BOOL:${RT90_WERROR}>:/WX>)
else()
  target_compile_options(rt90_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wno-sign-conversion $<$<BOOL:${RT90_WERROR}>:-Werror>)
endif()

# --- Firmware: embed firmware/vcr1.asm into the binary --------------------
set(FIRMWARE_SRC ${CMAKE_SOURCE_DIR}/firmware/vcr1.asm)
set(FIRMWARE_CPP ${CMAKE_BINARY_DIR}/generated/firmware_vcr1.cpp)
add_custom_command(
  OUTPUT ${FIRMWARE_CPP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/generated
  COMMAND ${CMAKE_COMMAND} -DINPUT=${FIRMWARE_SRC} -DOUTPUT=${FIRMWARE_CPP}
          -DSYMBOL=kFirmwareVcr1Source -DNAMESPACE=rt90 -P ${CMAKE_SOURCE_DIR}/cmake/EmbedText.cmake
  DEPENDS ${FIRMWARE_SRC} ${CMAKE_SOURCE_DIR}/cmake/EmbedText.cmake
  COMMENT "Embedding VCR-1 firmware")

# --- Core emulator library (no SDL / FFmpeg dependencies) ------------------
add_library(rt90core STATIC
  src/core/bus.cpp
  src/core/cpu.cpp
  src/core/assembler.cpp
  src/core/ram.cpp
  src/core/ring_buffer.cpp
  src/av/av_sync.cpp
  src/av/interlace.cpp
  src/av/static_noise.cpp
  src/av/vhs_audio.cpp
  src/av/vhs_noise.cpp
  src/app/ui_overlay.cpp
  src/tape/tape_source.cpp
  src/tape/tape_streamer.cpp
  src/app/tv_set.cpp
  src/vcr/deck.cpp
  src/vcr/machine.cpp
  src/gpu/gpu_regs.cpp
  src/gpu/gpu.cpp
  src/gpu/crt_filter.cpp
  src/gpu/draw.cpp
  src/gpu/font.cpp
  src/gpu/test_pattern.cpp
  src/util/image_io.cpp
  src/util/file_util.cpp
  src/util/log.cpp
  src/tape/vhs_format.cpp
  src/tape/file_detect.cpp
  src/tape/jpeg.cpp
  src/tape/importer.cpp
  src/util/stb_impl.cpp
  ${FIRMWARE_CPP})
target_include_directories(rt90core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_include_directories(rt90core SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/external/stb)
set_source_files_properties(src/util/stb_impl.cpp PROPERTIES COMPILE_OPTIONS "$<IF:$<CXX_COMPILER_ID:MSVC>,/w,-w>")
find_package(Threads REQUIRED)
target_link_libraries(rt90core PRIVATE rt90_warnings PUBLIC Threads::Threads)

# --- FFmpeg (libavformat/libavcodec/libswscale/libswresample) --------------------
if(RT90_WITH_FFMPEG)
  find_package(PkgConfig REQUIRED)
  if(MINGW)
    # Static FFmpeg from scripts/build-ffmpeg-win64.sh: link its private deps too.
    set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SOURCE_DIR}/third_party/win64/lib/pkgconfig")
    set(ENV{PKG_CONFIG_SYSROOT_DIR} "")
    pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libswscale libswresample libavutil)
    execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --static --libs libavformat libavcodec libswscale libswresample libavutil
                    OUTPUT_VARIABLE FFMPEG_STATIC_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)
    separate_arguments(FFMPEG_STATIC_LIBS)
    target_include_directories(rt90core SYSTEM PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_libraries(rt90core PRIVATE ${FFMPEG_STATIC_LIBS})
  else()
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavformat libavcodec libswscale libswresample libavutil)
    target_link_libraries(rt90core PRIVATE PkgConfig::FFMPEG)
  endif()
  target_compile_definitions(rt90core PRIVATE RT90_HAVE_FFMPEG=1)
endif()

# --- Assembler CLI ----------------------------------------------------------
add_executable(vcrasm tools/vcrasm.cpp)
target_link_libraries(vcrasm PRIVATE rt90core rt90_warnings)

# --- Desktop app --------------------------------------------------------------
if(RT90_BUILD_APP)
  find_package(SDL2 REQUIRED CONFIG)
  add_executable(RetroTube90 WIN32 src/main.cpp src/platform/platform_win32.cpp src/platform/platform_posix.cpp)
  target_link_libraries(RetroTube90 PRIVATE rt90core rt90_warnings)
  if(WIN32)
    target_link_libraries(RetroTube90 PRIVATE comdlg32)
  endif()
  if(TARGET SDL2::SDL2main)
    target_link_libraries(RetroTube90 PRIVATE SDL2::SDL2main)
  endif()
  if(TARGET SDL2::SDL2-static AND RT90_STATIC_SDL)
    target_link_libraries(RetroTube90 PRIVATE SDL2::SDL2-static)
  else()
    target_link_libraries(RetroTube90 PRIVATE SDL2::SDL2)
  endif()
endif()

# --- Tests --------------------------------------------------------------------
if(RT90_BUILD_TESTS)
  enable_testing()
  add_executable(rt90_tests
    tests/test_main.cpp
    tests/test_cpu.cpp
    tests/test_assembler.cpp
    tests/test_firmware.cpp
    tests/test_gpu.cpp
    tests/test_ram_video.cpp
    tests/test_noise.cpp
    tests/test_tape.cpp)
  target_link_libraries(rt90_tests PRIVATE rt90core rt90_warnings)
  target_compile_definitions(rt90_tests PRIVATE
    RT90_TEST_MEDIA_DIR="${CMAKE_SOURCE_DIR}/tests/media"
    RT90_TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test_tmp")
  add_test(NAME rt90_tests COMMAND rt90_tests)
endif()
