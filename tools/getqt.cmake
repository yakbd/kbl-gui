# Downloads and unpacks the pinned Qt build for one platform, with no Python, no Qt account and no installer.
#
#   cmake -DPLATFORM=linux_gcc_64 -DDEST=/path/to/qt [-DDOWNLOADS=/path/to/cache] -P tools/getqt.cmake
#
# PLATFORM is linux_gcc_64, clang_64, win64_mingw or mingw_toolchain (Qt's MinGW 15.1 for the Windows build).
# Every archive is checked against the SHA-1 committed in tools/qt-pins.cmake; a mismatch stops the script.
# Archives already in DOWNLOADS with the right hash are not downloaded again.

include("${CMAKE_CURRENT_LIST_DIR}/qt-pins.cmake")
if(NOT PLATFORM OR NOT DEST)
  message(FATAL_ERROR "pass -DPLATFORM=<platform> -DDEST=<folder>")
endif()
if(NOT DEFINED QT_PIN_${PLATFORM})
  message(FATAL_ERROR "no pins for platform ${PLATFORM} in tools/qt-pins.cmake")
endif()
if(NOT DOWNLOADS)
  set(DOWNLOADS "${DEST}-downloads")
endif()
file(MAKE_DIRECTORY "${DOWNLOADS}" "${DEST}")

set(pins ${QT_PIN_${PLATFORM}})
list(LENGTH pins n)
math(EXPR last "${n} - 1")
foreach(i RANGE 0 ${last} 2)
  math(EXPR j "${i} + 1")
  list(GET pins ${i} archive)
  list(GET pins ${j} sha1)
  set(path "${DOWNLOADS}/${archive}")
  set(have FALSE)
  if(EXISTS "${path}")
    file(SHA1 "${path}" got)
    if(got STREQUAL sha1)
      set(have TRUE)
    endif()
  endif()
  if(NOT have)
    message(STATUS "download ${archive}")
    file(DOWNLOAD "${QT_PIN_${PLATFORM}_URL}/${archive}" "${path}" EXPECTED_HASH SHA1=${sha1} STATUS st)
    list(GET st 0 code)
    if(NOT code EQUAL 0)
      message(FATAL_ERROR "${archive}: ${st}")
    endif()
  endif()
  message(STATUS "extract ${archive}")
  file(ARCHIVE_EXTRACT INPUT "${path}" DESTINATION "${DEST}")
endforeach()

# The Linux ICU archive unpacks its libraries at the top level; Qt looks for them in lib/.
file(GLOB icu "${DEST}/libicu*.so*")
foreach(f IN LISTS icu)
  get_filename_component(name "${f}" NAME)
  file(RENAME "${f}" "${DEST}/lib/${name}")
endforeach()

# Qt's online installer writes a qt.conf-free, relocatable tree; nothing else to patch for CMake use.
message(STATUS "Qt ${QT_PIN_VERSION} (${PLATFORM}) ready in ${DEST}")
