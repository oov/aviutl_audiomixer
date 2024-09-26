option(FORMAT_SOURCES "execute clang-format" ON)
option(USE_COMPILER_RT "use compiler-rt runtime" OFF)
add_subdirectory(3rd/ovbase)
add_subdirectory(3rd/ovutil)

if(FORMAT_SOURCES)
  file(GLOB_RECURSE sources LIST_DIRECTORIES false CONFIGURE_DEPENDS "*.h" "*.c")
  list(FILTER sources EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/3rd")
  find_program(CLANG_FORMAT_EXE clang-format)
  add_custom_target(${PROJECT_NAME}-format ALL
    COMMAND ${CLANG_FORMAT_EXE} -style=file -i ${sources}
  )
endif()

add_custom_target(generate_version_h COMMAND
  ${CMAKE_COMMAND}
  -Dlocal_dir="${PROJECT_SOURCE_DIR}"
  -Dinput_file="${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
  -Doutput_file="${CMAKE_CURRENT_BINARY_DIR}/version.h"
  -P "${ovutil_SOURCE_DIR}/src/cmake/version.cmake"
)

# generate i18n.rc
set(LANGCSV "${CMAKE_CURRENT_SOURCE_DIR}/i18n/langs.csv")
file(READ "${LANGCSV}" langs)
string(STRIP ${langs} langs)
string(REPLACE "\n" ";" langs "${langs}")
foreach(line IN LISTS langs)
  if (line MATCHES "^#.*$|^([^,]+),$")
    continue()
  endif()
  if (line MATCHES "^([^,]+),([^,]+)$")
    list(APPEND polist "${CMAKE_CURRENT_SOURCE_DIR}/i18n/${CMAKE_MATCH_1}.po.DO_NOT_EDIT")
  else()
    message(FATAL_ERROR "invalid language difinition: ${line}")
  endif()
endforeach()
add_custom_command(
  OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/i18n.rc"
  COMMAND
    ${CMAKE_COMMAND}
    -Dsrc_dir="${CMAKE_CURRENT_SOURCE_DIR}/i18n"
    -Doutput_dir="${CMAKE_CURRENT_BINARY_DIR}"
    -Drctmpl="${CMAKE_CURRENT_SOURCE_DIR}/i18n.rc.tmpl"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/i18n_rc.cmake"
  WORKING_DIRECTORY
    "${CMAKE_CURRENT_SOURCE_DIR}/i18n"
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/i18n.rc.tmpl"
    ${polist}
)
add_custom_target(generate_rc DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/i18n.rc")

set(is_clang "$<C_COMPILER_ID:Clang>")
set(v16_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,16>")
set(v18_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,18>")
set(v19_or_later "$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,19>")
add_library(audiomixer_intf INTERFACE)
target_include_directories(audiomixer_intf INTERFACE
  "${CMAKE_CURRENT_BINARY_DIR}" # for version.h
)
target_compile_definitions(audiomixer_intf INTERFACE
  _WIN32_WINNT=0x0502
  _WINDOWS
  $<$<CONFIG:Release>:NDEBUG>
)
target_compile_options(audiomixer_intf INTERFACE
  $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${USE_COMPILER_RT}>>:--rtlib=compiler-rt>
  -mstackrealign
  -Wall
  -Wextra
  -Werror
  -Weverything
  -Wshadow
  -Werror=return-type
  -pedantic-errors
  -Wno-declaration-after-statement
  -Wno-padded
  $<$<AND:${is_clang},${v16_or_later}>:-Wno-unsafe-buffer-usage>
  $<$<AND:${is_clang},${v18_or_later}>:-Wno-switch-default>
  $<$<AND:${is_clang},${v19_or_later}>:-Wno-pre-c11-compat>
  -ffunction-sections
  -fdata-sections
  -msse2
  -ffast-math
  $<$<CONFIG:Debug>:-O0>
  $<$<CONFIG:Release>:-O2>
  -flto
)
target_link_options(audiomixer_intf INTERFACE
  -fuse-ld=lld
  -Wl,--gc-sections
  # -Wl,--print-gc-sections
  -Wl,--kill-at
  $<$<CONFIG:Release>:-s>
)
target_link_libraries(audiomixer_intf INTERFACE
  ovbase
  ovutil
)

add_custom_target(copy_related_files
  COMMAND ${CMAKE_COMMAND} -E copy "${PROJECT_SOURCE_DIR}/README.md" "${CMAKE_BINARY_DIR}/bin/AudioMixer.txt"
)

add_library(audiomixer_auf SHARED
  array2d.c
  audiomixer.c
  audiomixer.rc
  aux_channel.c
  aviutl.c
  channel.c
  circbuffer.c
  circbuffer_i16.c
  dither.c
  dynamics.c
  error_axr.c
  i18n.rc
  lagger.c
  mixer.c
  parallel_output.c
  parallel_output_gui.c
  rbjeq.c
  uxfdreverb.c
)
set_target_properties(audiomixer_auf PROPERTIES
  OUTPUT_NAME "AudioMixer.auf"
  PREFIX ""
  SUFFIX ""
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
add_dependencies(audiomixer_auf generate_version_h copy_related_files)
target_link_libraries(audiomixer_auf PRIVATE audiomixer_intf)

add_executable(test_audiomixer audiomixer_test.c)
target_link_libraries(test_audiomixer PRIVATE audiomixer_intf)
add_test(NAME test_audiomixer COMMAND test_audiomixer)

add_executable(test_circbuffer circbuffer_test.c)
target_link_libraries(test_circbuffer PRIVATE audiomixer_intf)
add_test(NAME test_circbuffer COMMAND test_circbuffer)

add_executable(test_circbuffer_i16 circbuffer_i16_test.c)
target_link_libraries(test_circbuffer_i16 PRIVATE audiomixer_intf)
add_test(NAME test_circbuffer_i16 COMMAND test_circbuffer_i16)
