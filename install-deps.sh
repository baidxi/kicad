#!/bin/bash
# Dependencies for a local KiCad build, kept aligned with
# .github/workflows/build-packages.yml (linux jobs).
#
# Not included here on purpose (they match what the workflow does NOT enable):
#   - doxygen/dblatex/po4a/asciidoc/source-highlight: only for the docs build
#   - python3-pytest/python3-cairosvg/python3-numpy: only for the QA suite
#   - debhelper: the workflow packs the .deb with dpkg-deb directly

sudo apt-get update && sudo apt-get install -y \
  debhelper \
  cmake \
  cmake-curses-gui \
  build-essential \
  pkg-config \
  gettext \
  dpkg-dev \
  file \
  ninja-build \
  glslang-tools \
  spirv-tools \
  libvulkan-dev \
  libwebkit2gtk-4.1-dev \
  desktop-file-utils \
  libcairo2-dev \
  libpng-dev \
  libglu1-mesa-dev \
  libgl1-mesa-dev \
  libx11-dev \
  mesa-common-dev \
  libgtk-3-dev \
  libglm-dev \
  libwxgtk3.2-dev \
  libwxgtk-webview3.2-dev \
  libbz2-dev \
  libssl-dev \
  libzstd-dev \
  zlib1g-dev \
  python3-dev \
  libocct-modeling-algorithms-dev \
  libocct-modeling-data-dev \
  libocct-data-exchange-dev \
  libocct-visualization-dev \
  libocct-foundation-dev \
  libocct-ocaf-dev \
  libgit2-dev \
  libsecret-1-dev \
  libboost-all-dev \
  libcurl4-openssl-dev \
  libngspice0-dev \
  unixodbc-dev \
  libsqlite3-dev \
  libspnav-dev \
  libnng-dev \
  libprotobuf-dev \
  protobuf-compiler \
  shared-mime-info \
  libzint-dev \
  libpoppler-dev \
  libpoppler-glib-dev
