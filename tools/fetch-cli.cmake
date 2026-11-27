# Downloads one yakbd CLI binary from a tagged yakbd-cli release and checks it against the release's SHA256SUMS.
#
#   cmake -DASSET=linux-amd64 -DDEST=/path/to/dir [-DTAG=v0.1.0-preview.1] [-DSUMS_SHA256=<hash>] -P tools/fetch-cli.cmake
#
# ASSET is linux-amd64, linux-arm64, darwin-amd64, darwin-arm64, windows-x64, windows-arm64 or windows-x86.
# Without TAG the pin in tools/cli-pins.cmake is used, and SHA256SUMS itself must then match the pinned hash.
# The result is DEST/yakbd (DEST/yakbd.exe for Windows), plus DEST/yakbd-cli-LICENSE and DEST/yakbd-cli-NOTICE.

include("${CMAKE_CURRENT_LIST_DIR}/cli-pins.cmake")
if(NOT ASSET OR NOT DEST)
  message(FATAL_ERROR "pass -DASSET=<os-arch> -DDEST=<folder>")
endif()
if(NOT TAG)
  set(TAG "${YAKBD_CLI_TAG}")
  set(SUMS_SHA256 "${YAKBD_CLI_SHA256SUMS_SHA256}")
endif()
string(REGEX REPLACE "^v" "" version "${TAG}")
set(base "https://github.com/yakbd/yakbd-cli/releases/download/${TAG}")
file(MAKE_DIRECTORY "${DEST}")
set(work "${DEST}/.download")
file(MAKE_DIRECTORY "${work}")

if(SUMS_SHA256)
  file(DOWNLOAD "${base}/SHA256SUMS" "${work}/SHA256SUMS" EXPECTED_HASH SHA256=${SUMS_SHA256} STATUS st)
else()
  file(DOWNLOAD "${base}/SHA256SUMS" "${work}/SHA256SUMS" STATUS st)
endif()
list(GET st 0 code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "SHA256SUMS of ${TAG}: ${st}")
endif()

set(name "yakbd-${version}-${ASSET}")
if(ASSET MATCHES "^windows-")
  set(name "${name}.zip")
endif()
file(STRINGS "${work}/SHA256SUMS" lines REGEX "  ${name}$")
if(NOT lines)
  message(FATAL_ERROR "${name} is not listed in SHA256SUMS of ${TAG}")
endif()
string(REGEX MATCH "^[0-9a-f]+" sha "${lines}")
file(DOWNLOAD "${base}/${name}" "${work}/${name}" EXPECTED_HASH SHA256=${sha} STATUS st)
list(GET st 0 code)
if(NOT code EQUAL 0)
  message(FATAL_ERROR "${name}: ${st}")
endif()

if(ASSET MATCHES "^windows-")
  file(ARCHIVE_EXTRACT INPUT "${work}/${name}" DESTINATION "${work}/unzip")
  file(GLOB_RECURSE exe "${work}/unzip/yakbd.exe")
  file(COPY_FILE "${exe}" "${DEST}/yakbd.exe")
else()
  file(COPY_FILE "${work}/${name}" "${DEST}/yakbd")
  file(CHMOD "${DEST}/yakbd" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
       WORLD_READ WORLD_EXECUTE)
endif()
file(COPY_FILE "${work}/SHA256SUMS" "${DEST}/yakbd-SHA256SUMS")
# The CLI's own LICENSE and NOTICE travel with it in every package (from its source at the same tag).
foreach(doc LICENSE NOTICE)
  file(DOWNLOAD "https://raw.githubusercontent.com/yakbd/yakbd-cli/${TAG}/${doc}" "${DEST}/yakbd-cli-${doc}" STATUS st)
  list(GET st 0 code)
  if(NOT code EQUAL 0)
    message(FATAL_ERROR "${doc} of yakbd-cli ${TAG}: ${st}")
  endif()
endforeach()
file(REMOVE_RECURSE "${work}")
message(STATUS "yakbd ${version} (${ASSET}) in ${DEST}, SHA-256 ${sha}")
