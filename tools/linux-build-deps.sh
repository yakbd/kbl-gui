#!/bin/sh
# Installs what the Linux build needs on Ubuntu 22.04 (the oldest supported build host, so the binaries run on
# every distribution with glibc 2.35 or later), plus a pinned CMake from Kitware (Qt 6.12 needs CMake 3.25+).
# Used by the CI job (container ubuntu:22.04) and by tools/docker/build.Dockerfile. Runs as root.
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates curl file git g++ make ninja-build pkg-config \
  libgl-dev libegl-dev libxkbcommon-dev libxkbcommon-x11-0 libfontconfig1 libfreetype6 libdbus-1-3 libglib2.0-0 \
  libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 \
  libxcb-xinerama0 libxcb-xkb1 libxcb-xfixes0 libx11-xcb1 libwayland-client0 libwayland-cursor0 libwayland-egl1 \
  libgl1-mesa-dri libgtk-3-0 adwaita-icon-theme fonts-dejavu-core xvfb xauth desktop-file-utils
CMAKE_VERSION=4.4.5
CMAKE_SHA256=ed85135c5a5135b45e50423cb406dea04ca3338da0b8ef3c9ede963559680be0
curl -fsSL -o /tmp/cmake.tar.gz \
  "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum -c -
tar -xzf /tmp/cmake.tar.gz -C /opt
ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cmake" /usr/local/bin/cmake
ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/ctest" /usr/local/bin/ctest
ln -sf "/opt/cmake-${CMAKE_VERSION}-linux-x86_64/bin/cpack" /usr/local/bin/cpack
rm -f /tmp/cmake.tar.gz
