#!/usr/bin/env bash

# Build distributable Linux packages from this source tree:
#   - .deb      (Debian/Ubuntu, host architecture)
#   - .AppImage (x86_64 or aarch64)
#
# Local counterpart of the "linux-deb" and "linux-appimage" jobs in
# .github/workflows/build-packages.yml.
#
# Usage:
#   tools/build-linux-packages.sh [deb|appimage|all|check]    # default: all
#
#   check  parse the workflow file and show/verify the configuration that
#          would be used, without building anything
#
# Environment overrides (optional):
#   JOBS=N              Parallel build jobs (default: nproc)
#   SKIP_DEPS=1         Do not apt-get install the build dependencies
#   SKIP_STOCK_LIBS=1   Do not bundle the official symbol/footprint/3D model
#                       libraries; the package then has empty library tables
#                       (the first run clones ~2 GB of git repositories)
#   DISTRO=24.04        Cross-build for another Ubuntu LTS inside an
#                       ubuntu:<DISTRO> container (podman or docker), the
#                       same way CI builds its matrix.  Without DISTRO the
#                       packages target the host distro; since glibc is not
#                       bundled, host-built packages only run on distros with
#                       glibc <= the host's.
#   INSTALL_PREFIX=...  Install prefix compiled into the binaries (default:
#                       /opt/kicad).  The .deb stages everything under this
#                       prefix; the AppImage lays the same tree out under
#                       usr/ inside the image, because KiCad's AppImage code
#                       paths hardcode $APPDIR/usr locations (common/
#                       kiway.cpp dso_search_path, common/paths.cpp
#                       GetStockEDALibraryPath)
#   DOWNLOADS_DIR=...   Persistent cache for downloaded components (the
#                       stock library repos and the linuxdeploy tool),
#                       default: .downloads/ in the repo root (gitignored) -
#                       kept separate from the build tree so wiping the
#                       build never re-downloads
#   BUILD_ROOT=...      Scratch area root (default: build/linux-packages,
#                       already covered by the "/build*" .gitignore rule)
#   GIT_CLONE_PROTO=ssh|https   Protocol for cloning the stock library repos
#                       (default: ssh - uses the developer's SSH key; CI
#                       runners themselves keep anonymous https)
#
# Single source of truth: everything that is *configuration* (CMake options,
# apt dependency list, stock library repos, desktop entry excludes, the
# linuxdeploy download) is parsed from .github/workflows/build-packages.yml at
# runtime instead of being duplicated here - edit the workflow and this script
# follows.  Any parse failure is fatal with a pointer at the parser, so a
# workflow layout change cannot silently produce artifacts unlike CI's.  Only
# the packaging *procedure* (dpkg-shlibdeps, control file, AppRun, linuxdeploy
# invocation) lives in this script, mirroring the corresponding workflow steps.
#
# Notes:
# - Targets Debian/Ubuntu hosts: the dependency list is apt-based and the .deb
#   needs dpkg-shlibdeps to compute its dependency list.
# - As in CI, glibc is not bundled: a package built here installs on distros
#   with glibc <= the build host's.  Build on the oldest LTS you target.

set -euo pipefail

SCRIPT_DIR="$(builtin cd "$(dirname "$0")" >/dev/null 2>&1 && pwd -P)"
REPO_DIR="$(builtin cd "${SCRIPT_DIR}/.." >/dev/null 2>&1 && pwd -P)"
WF_FILE="${REPO_DIR}/.github/workflows/build-packages.yml"

TARGET="${1:-all}"
case "${TARGET}" in
  deb|appimage|all|check) ;;
  *) echo "Error: unknown target '${TARGET}' (expected deb, appimage, all or check)" >&2; exit 1 ;;
esac

JOBS="${JOBS:-$(nproc)}"
BUILD_ROOT="${BUILD_ROOT:-${REPO_DIR}/build/linux-packages}"
BUILD_DIR="${BUILD_ROOT}/cmake"
STAGE_DIR="${BUILD_ROOT}/debstage"
APPDIR="${BUILD_ROOT}/AppDir"
DIST_DIR="${BUILD_ROOT}/dist"

# Persistent download cache, kept in a dedicated repo-root directory instead
# of the build tree: the stock library clones (~2 GB) and the linuxdeploy
# tool survive `rm -rf build/` and are reused across builds and build
# directories.  Stock repos live in DOWNLOADS_DIR/stock, linuxdeploy in
# DOWNLOADS_DIR/linuxdeploy-<arch>.AppImage.
DOWNLOADS_DIR="${DOWNLOADS_DIR:-${REPO_DIR}/.downloads}"
STOCK_DIR="${DOWNLOADS_DIR}/stock"

# Install prefix baked into the binaries at configure time (KICAD_DATA and
# friends); the .deb stages everything under /opt/kicad per the /opt
# packaging convention.  Override only together with a fresh build directory.
INSTALL_PREFIX="${INSTALL_PREFIX:-/opt/kicad}"

log() { echo "=== $* ==="; }
die() { echo "Error: $*" >&2; exit 1; }

# ---------------------------------------------------------------------------
# Workflow parsing
# ---------------------------------------------------------------------------

# Load the output lines of $2 (a parse_* function) into the array named $1;
# $3 is a human-readable description used in the fatal error message.
load_parsed()
{
  local -n arr="$1"
  local desc="$2"
  shift 2
  local out line
  out="$("$@" 2>/dev/null || true)"
  if [[ -z "${out}" ]]; then
    die "could not parse ${desc} from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"
  fi
  while IFS= read -r line; do
    [[ -n "${line}" ]] && arr+=( "${line}" )
  done <<< "${out}"
}

# Value of a single-line key in the top-level 'env:' block (workflow scopes:
# "env: CMAKE_BUILD_TYPE" at the top of build-packages.yml).
parse_top_env_scalar()
{
  local key="$1"
  awk -v key="${key}" '
    /^env:/ { f=1; next }
    f && /^[^ \t#]/ { exit }
    f && $0 ~ "^  " key ":" { sub( "^  " key ":[ \t]*", "" ); print; exit }
  ' "${WF_FILE}"
}

# Folded scalar '>-': join the continuation lines with spaces, exactly what
# YAML folding does to the KICAD_CMAKE_EXTRA option list.
parse_kicad_cmake_extra()
{
  awk '
    /^env:/ { f=1; next }
    f && /^[^ \t#]/ { exit }
    f && /^  KICAD_CMAKE_EXTRA:/ { e=1; next }
    e && /^$/ { next }
    e && /^    / { sub( /^    /, "" ); printf "%s ", $0; next }
    e { exit }
  ' "${WF_FILE}"
}

# Union of every "apt-get install -y --no-install-recommends <list>" in the
# workflow (the Linux deb and AppImage jobs).  Continuation lines are joined;
# nested "|| sudo apt-get install ..." fallback commands restart collection,
# so both the wxWidgets 3.2 list and its 3.3 fallback end up in the union.
parse_apt_packages()
{
  awk '
    /apt-get install -y --no-install-recommends \\$/ { collect=1; next }
    collect && /(sudo|apt-get|\|\|)/ { next }
    collect && /^[ \t]+[^ \t]/ {
      line=$0
      sub( /^[ \t]+/, "", line )
      cont=(line ~ /\\$/)
      sub( /[ \t]*\\$/, "", line )
      gsub( /[ \t]+/, "\n", line )
      print line
      if( !cont ) collect=0
      next
    }
    { collect=0 }
  ' "${WF_FILE}" | sort -u
}

# The "for spec in repo:mirror:branch" list shared by all Linux jobs.
parse_stock_specs()
{
  # Print the whole for-loop statement (from "for spec in" through "; do")
  # and pull the quoted repo:mirror:branch items out of it.
  awk '/for spec in/{ f=1 } f{ print } f && /; do/{ exit }' "${WF_FILE}" \
    | grep -oE '"[^":]+:[^":]+:[^":]+"' | tr -d '"' | sort -u
}

# Base URL of the stock library mirrors (the clone target inside the loop).
parse_stock_mirror_base()
{
  awk '/git clone --depth 1 -b/{ f=1 } f && /github\.com/{ print; exit }' "${WF_FILE}" \
    | grep -oE 'https://github\.com/[^/"]+/'
}

# Desktop-entry names excluded when picking the AppImage's main .desktop.
parse_desktop_excludes()
{
  # Anchored on the org.kicad.*.desktop selector so the prefix of the
  # applications directory (usr/share or /opt layouts) does not matter.
  awk '/org\.kicad\.\*\.desktop/{ f=1 } f{ print } f && /head -1/{ exit }' "${WF_FILE}" \
    | grep -oE "![[:space:]]+-name '[^']+'" | sed -E "s/.*-name '([^']+)'.*/\1/" | sort -u
}

# The linuxdeploy AppImage download URL (x86_64 variant in the workflow).
parse_linuxdeploy_url()
{
  grep -oE 'https://github.com/linuxdeploy/linuxdeploy/releases/download/[^"]+\.AppImage' \
    "${WF_FILE}" | sort -u | head -1
}

# CMAKE_INSTALL_PREFIX of the linux-appimage job.  The CI stages straight
# into AppDir/usr - there is no opt->usr folding like this script has - so
# anything but /usr silently produces a broken image there.  Parsed only to
# be able to assert it in verify_config().
parse_appimage_prefix()
{
  awk '
    /^  linux-appimage:/ { f=1; next }
    f && /^  [a-zA-Z-]+:/ { exit }
    f && /-DCMAKE_INSTALL_PREFIX=/ { print; exit }
  ' "${WF_FILE}" | grep -oE 'CMAKE_INSTALL_PREFIX=[^ "\]+'
}

CMAKE_BUILD_TYPE_CFG=""
KICAD_CMAKE_EXTRA=""
APPIMAGE_PREFIX_CFG=""
APT_PACKAGES=()
STOCK_SPECS=()
STOCK_MIRROR_BASE=""
DESKTOP_EXCLUDES=()
LINUXDEPLOY_URL=""

load_workflow_config()
{
  [[ -f "${WF_FILE}" ]] || die "workflow file not found: ${WF_FILE}"

  CMAKE_BUILD_TYPE_CFG="$(parse_top_env_scalar CMAKE_BUILD_TYPE || true)"
  [[ -n "${CMAKE_BUILD_TYPE_CFG}" ]] \
    || die "could not parse CMAKE_BUILD_TYPE from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"

  KICAD_CMAKE_EXTRA="$(parse_kicad_cmake_extra || true)"
  [[ -n "${KICAD_CMAKE_EXTRA}" ]] \
    || die "could not parse KICAD_CMAKE_EXTRA from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"

  load_parsed APT_PACKAGES      "the apt dependency list"      parse_apt_packages
  load_parsed STOCK_SPECS       "the stock library repo specs" parse_stock_specs
  load_parsed DESKTOP_EXCLUDES  "the desktop entry excludes"   parse_desktop_excludes

  STOCK_MIRROR_BASE="$(parse_stock_mirror_base || true)"
  [[ -n "${STOCK_MIRROR_BASE}" ]] \
    || die "could not parse the stock library mirror URL from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"

  LINUXDEPLOY_URL="$(parse_linuxdeploy_url || true)"
  [[ -n "${LINUXDEPLOY_URL}" ]] \
    || die "could not parse the linuxdeploy download URL from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"

  APPIMAGE_PREFIX_CFG="$(parse_appimage_prefix || true)"
  [[ -n "${APPIMAGE_PREFIX_CFG}" ]] \
    || die "could not parse the linux-appimage CMAKE_INSTALL_PREFIX from $(basename "${WF_FILE}") - the workflow layout changed; update the parser in this script"
}

# Sanity-check the parsed configuration against what the packaging steps
# below actually rely on; used by the 'check' target.
verify_config()
{
  local ok=1 pkg

  case "${CMAKE_BUILD_TYPE_CFG}" in
    Release|RelWithDebInfo|Debug|MinSizeRel) ;;
    *) echo "unexpected CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE_CFG}"; ok=0 ;;
  esac
  [[ "${KICAD_CMAKE_EXTRA}" == *-DKICAD_USE_SENTRY=OFF* ]] \
    || { echo "KICAD_CMAKE_EXTRA lost its -DKICAD_USE_SENTRY=OFF entry"; ok=0; }

  # The CI AppImage job has no opt->usr folding: a non-/usr prefix there
  # stages the tree into AppDir/<prefix> and the image comes out without
  # bin/lib.  This script would not notice (it folds locally), so assert it.
  [[ "${APPIMAGE_PREFIX_CFG}" == "CMAKE_INSTALL_PREFIX=/usr" ]] \
    || { echo "linux-appimage ${APPIMAGE_PREFIX_CFG} is invalid - the CI job stages straight into AppDir/usr, so the prefix must stay /usr"; ok=0; }

  # Packages the steps below depend on by name.
  for pkg in cmake ninja-build g++ pkg-config dpkg-dev desktop-file-utils file; do
    [[ " ${APT_PACKAGES[*]} " == *" ${pkg} "* ]] \
      || { echo "apt list is missing '${pkg}' (needed by the packaging steps)"; ok=0; }
  done

  [[ ${#STOCK_SPECS[@]} -eq 4 ]] \
    || { echo "expected 4 stock library repos, found ${#STOCK_SPECS[@]}: ${STOCK_SPECS[*]}"; ok=0; }
  [[ "${LINUXDEPLOY_URL}" == *linuxdeploy*.AppImage ]] \
    || { echo "unexpected linuxdeploy URL: ${LINUXDEPLOY_URL}"; ok=0; }

  [[ ${ok} -eq 1 ]] || die "workflow configuration verification failed"
}

show_config()
{
  cat <<EOF
Configuration parsed from ${WF_FILE}:

  CMAKE_BUILD_TYPE    : ${CMAKE_BUILD_TYPE_CFG}
  KICAD_CMAKE_EXTRA   : ${KICAD_CMAKE_EXTRA}
  appimage prefix     : ${APPIMAGE_PREFIX_CFG#CMAKE_INSTALL_PREFIX=}
  apt packages        : ${#APT_PACKAGES[@]} (union of the Linux "Install dependencies" steps)
  stock library repos : ${STOCK_SPECS[*]}
  stock mirror        : ${STOCK_MIRROR_BASE}
  desktop excludes    : ${DESKTOP_EXCLUDES[*]}
  linuxdeploy URL     : ${LINUXDEPLOY_URL}

  install prefix      : ${INSTALL_PREFIX}
  build dir           : ${BUILD_DIR}
  download cache      : ${DOWNLOADS_DIR}
  artifacts           : ${DIST_DIR}
EOF
}

# ---------------------------------------------------------------------------
# Environment helpers
# ---------------------------------------------------------------------------

host_distro_id()
{
  ( . /etc/os-release && echo "${VERSION_ID:-unknown}" )
}

host_arch()
{
  uname -m
}

deb_arch()
{
  dpkg --print-architecture
}

# Multiarch library directory name (usr/lib/<multiarch>) of the build host;
# baked into the AppRun so non-x86_64 builds resolve their libraries too.
host_multiarch()
{
  local ma
  ma="$(dpkg-architecture -qDEB_HOST_MULTIARCH 2>/dev/null || true)"
  [[ -n "${ma}" ]] || ma="$(gcc -dumpmachine 2>/dev/null || true)"
  [[ -n "${ma}" ]] || ma="x86_64-linux-gnu"
  echo "${ma}"
}

require_tools()
{
  command -v git   >/dev/null 2>&1 || die "git not found"
  command -v cmake >/dev/null 2>&1 || die "cmake not found"
  command -v ninja >/dev/null 2>&1 || die "ninja not found"
  command -v curl  >/dev/null 2>&1 || die "curl not found (needed to fetch linuxdeploy)"
  if [[ "${TARGET}" != appimage ]]; then
    command -v dpkg-deb       >/dev/null 2>&1 || die "dpkg-deb not found (apt install dpkg-dev)"
    command -v dpkg-shlibdeps >/dev/null 2>&1 || die "dpkg-shlibdeps not found (apt install dpkg-dev)"
  fi
}

# ---------------------------------------------------------------------------
# Build steps (mirror the workflow jobs step by step)
# ---------------------------------------------------------------------------

# Mirrors "Install dependencies" in the linux-deb / linux-appimage jobs.
install_deps()
{
  if [[ "${SKIP_DEPS:-0}" == 1 ]]; then
    log "Skipping dependency installation (SKIP_DEPS=1)"
    return
  fi
  if [[ ! -e /etc/debian_version ]]; then
    log "Not a Debian/Ubuntu host; skipping apt dependency installation (set SKIP_DEPS=1 to silence this)"
    return
  fi

  # The workflow installs wxWidgets 3.2 first and falls back to 3.3; here both
  # candidates come from the same parsed list, so filter by availability in
  # the apt sources instead - on 24.04 that keeps 3.2, on 26.04 it keeps 3.3.
  local available=() missing=() pkg
  for pkg in "${APT_PACKAGES[@]}"; do
    if apt-cache show --no-all-versions "${pkg}" >/dev/null 2>&1; then
      available+=( "${pkg}" )
    else
      echo "  not in apt sources, skipping: ${pkg}"
    fi
  done
  [[ ${#available[@]} -gt 0 ]] || die "none of the parsed apt packages are available in the apt sources (run 'apt-get update' first?)"

  # Install only what is actually missing, so a prepared machine never
  # touches apt (and therefore never needs sudo) at all.
  for pkg in "${available[@]}"; do
    if ! dpkg-query -W -f='${Status}' "${pkg}" 2>/dev/null | grep -q "install ok installed"; then
      missing+=( "${pkg}" )
    fi
  done
  if [[ ${#missing[@]} -eq 0 ]]; then
    log "All build dependencies already installed; no root privileges needed"
    return
  fi
  log "Missing build dependencies: ${missing[*]}"

  # apt-get install is the only step of the whole script that needs root
  # (system -dev packages go into /usr), the same as the CI runners, which
  # also invoke it through sudo.  Everything else stays in user directories.
  local SUDO=""
  if [[ "$(id -u)" != 0 ]]; then
    command -v sudo >/dev/null 2>&1 || die "installing the missing packages needs root and sudo is unavailable.  Either install them manually:
  apt-get install -y --no-install-recommends ${missing[*]}
or run this script as root, or set SKIP_DEPS=1 once they are in place"
    SUDO="sudo"
  fi

  $SUDO apt-get update
  $SUDO apt-get install -y --no-install-recommends "${missing[@]}"
}

# Mirrors "Configure" + "Build" (the Linux jobs).
configure_and_build()
{
  # CMake caches the derived data-path variables (KICAD_DATA, KICAD_LIBRARY_
  # DATA, KICAD_PLUGINS, KICAD_DEMOS, KICAD_TEMPLATE, KICAD_DOCS) as absolute
  # paths computed from the prefix, and cached entries win over the
  # -DCMAKE_INSTALL_PREFIX passed on later reconfigures: files then install
  # to the old locations and the binaries keep the old compiled-in paths.
  # Wipe the cached tree when any of those cache entries predates the
  # requested prefix so a clean reconfigure happens.
  local stale_path
  # Capture groups: DATA|LIBRARY_DATA|... = \1, PATH|STRING = \2, value = \3.
  stale_path="$( sed -n -E 's/^KICAD_(DATA|LIBRARY_DATA|PLUGINS|DEMOS|TEMPLATE|DOCS):(PATH|STRING)=(.*)$/\3/p' \
                   "${BUILD_DIR}/CMakeCache.txt" 2>/dev/null \
                 | grep -v "^${INSTALL_PREFIX}/" | head -1 || true )"
  if [[ -n "${stale_path}" ]]; then
    log "Cached KiCad data path '${stale_path}' predates INSTALL_PREFIX=${INSTALL_PREFIX}; wiping the build tree for a clean reconfigure"
    rm -rf "${BUILD_DIR}"
  fi

  log "Configuring (${BUILD_DIR})"
  # The parsed KICAD_CMAKE_EXTRA is intentionally word-split into options.
  # shellcheck disable=SC2086
  cmake -G Ninja -S "${REPO_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE_CFG}" \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_PREFIX}" \
    ${KICAD_CMAKE_EXTRA}

  log "Building (${JOBS} jobs)"
  cmake --build "${BUILD_DIR}" -j"${JOBS}"
}

# Mirrors "Bundle stock libraries" (all jobs): install the official stock
# repositories through their own CMake install rules into <prefix>/share/kicad.
# The CMake route is required, not cosmetic: kicad-symbols' default
# pack_symbols target (KICAD_PACK_SYM_LIBRARIES=ON) generates the packed
# single-file *.kicad_sym libraries plus the matching sym-lib-table, and the
# install rules exclude .git and other repository bookkeeping.
install_stock_libraries()
{
  local prefix="$1"
  local spec repo mirror branch repo_dir build_dir url mirror_url

  # The mirror base is parsed from the workflow in its https form; clone over
  # SSH locally instead (uses the developer's SSH key and avoids HTTPS rate
  # limits).  Set GIT_CLONE_PROTO=https to keep the workflow's form.  A base
  # that is not an https github URL is left untouched and cloned as parsed.
  mirror_url="${STOCK_MIRROR_BASE}"
  if [[ "${GIT_CLONE_PROTO:-ssh}" == ssh ]]; then
    mirror_url="${mirror_url/https:\/\/github.com\//git@github.com:}"
  fi

  for spec in "${STOCK_SPECS[@]}"; do
    repo="${spec%%:*}"
    rest="${spec#*:}"
    mirror="${rest%%:*}"
    branch="${rest##*:}"
    repo_dir="${STOCK_DIR}/${repo}"
    build_dir="${STOCK_DIR}/${repo}-build"
    url="${mirror_url}${mirror}.git"

    if [[ ! -d "${repo_dir}" ]]; then
      log "Cloning ${mirror} (${branch})"
      git clone --depth 1 -b "${branch}" "${url}" "${repo_dir}"
    fi

    # The per-repo CMake build dir is not relocatable: its cache embeds the
    # absolute source/build paths, so drop it when it was created elsewhere
    # (e.g. after the cache migration above) and let the configure below
    # re-create it - these repos are install-only, so rebuilding is cheap.
    if [[ -f "${build_dir}/CMakeCache.txt" ]]; then
      local cached_dir
      cached_dir="$( sed -n 's/^CMAKE_CACHEFILE_DIR:INTERNAL=//p' \
                        "${build_dir}/CMakeCache.txt" | head -1 || true )"
      cached_dir="$( realpath -m "${cached_dir}" 2>/dev/null || echo "${cached_dir}" )"
      if [[ "${cached_dir}" != "$(realpath -m "${build_dir}")" ]]; then
        log "Stock build cache for ${repo} was created at ${cached_dir}; re-creating at ${build_dir}"
        rm -rf "${build_dir}"
      fi
    fi

    # CMAKE_POLICY_VERSION_MINIMUM: kicad-packages3D still declares
    # cmake_minimum_required(2.6.1), which CMake 4 refuses without it
    # (harmlessly ignored by CMake 3.x).
    cmake -S "${repo_dir}" -B "${build_dir}" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
      -DCMAKE_INSTALL_PREFIX="${prefix}"
    # kicad-symbols runs its pack_symbols target here; a no-op elsewhere.
    cmake --build "${build_dir}"
    cmake --install "${build_dir}"
  done
}

# Mirrors the "bundled stock data verification" block: fail on anything
# missing or implausibly small instead of shipping packages without icons
# or libraries.
verify_stock_data()
{
  local data_root="$1"
  local nsym nfp n3d

  echo "--- bundled stock data verification ---"
  test -f "${data_root}/resources/images.tar.gz"    && echo "  icon archive        : OK"
  test -f "${data_root}/schemas/api.v1.schema.json" && echo "  api schema          : OK"
  test -f "${data_root}/template/sym-lib-table"     && echo "  default sym table   : OK"
  test -f "${data_root}/template/fp-lib-table"      && echo "  default fp table    : OK"

  # The recursion also tolerates stray subdirectories; the inner (find || true)
  # keeps 'set -e' from aborting when the directory is missing.
  nsym=$( (find "${data_root}/symbols"   -name '*.kicad_sym' 2>/dev/null || true) | wc -l )
  nfp=$( find "${data_root}/footprints" -maxdepth 1 -type d -name '*.pretty'    2>/dev/null | wc -l )
  n3d=$( find "${data_root}/3dmodels"   -maxdepth 1 -type d -name '*.3dshapes' 2>/dev/null | wc -l )
  echo "  symbol libraries    : ${nsym}"
  echo "  footprint libraries : ${nfp}"
  echo "  3d model libraries  : ${n3d}"
  if [[ "${nsym}" -eq 0 || "${nfp}" -eq 0 || "${n3d}" -eq 0 ]]; then
    echo "WARNING: zero library count - dumping the layouts for diagnosis:" >&2
    find "${data_root}/symbols"    -maxdepth 2 2>/dev/null | head -12 >&2 || true
    find "${data_root}/footprints" -maxdepth 2 2>/dev/null | head -12 >&2 || true
    find "${data_root}/3dmodels"   -maxdepth 2 2>/dev/null | head -12 >&2 || true
  fi
  [[ "${nsym}" -gt 50 && "${nfp}" -gt 50 && "${n3d}" -gt 50 ]]
}

bundle_stock_into()
{
  local prefix="$1"
  if [[ "${SKIP_STOCK_LIBS:-0}" == 1 ]]; then
    log "Skipping stock library bundling (SKIP_STOCK_LIBS=1): library tables will be empty"
    return
  fi
  install_stock_libraries "${prefix}"
  verify_stock_data "${prefix}/share/kicad"
}

# Mirrors "Assemble DEB" in the linux-deb job.
package_deb()
{
  log "Assembling .deb"
  local version distro_id arch deb_file executables libs shlibs multiarch

  version="$(git -C "${REPO_DIR}" describe --tags --always --dirty || echo dev)"
  version="${version#v}"
  distro_id="$(host_distro_id)"
  arch="$(deb_arch)"
  multiarch="$(host_multiarch)"
  deb_file="kicad-linux-${arch}-${distro_id}-${version}.deb"

  rm -rf "${STAGE_DIR}"
  install -d "${STAGE_DIR}/DEBIAN"
  # DESTDIR staging: with prefix /opt/kicad the tree lands in
  # debstage/opt/kicad/{bin,lib,share/kicad}.
  local staged_prefix="${STAGE_DIR}${INSTALL_PREFIX}"
  DESTDIR="${STAGE_DIR}" cmake --install "${BUILD_DIR}"

  bundle_stock_into "${staged_prefix}"

  # Auto-generate the shared-library dependency list from the staged files.
  # dpkg-shlibdeps requires a debian/control in its CWD and dies without one
  # (a failure CI masked with 2>/dev/null: the Depends line silently fell
  # back to bare libc6), so run it from a scratch directory holding a
  # minimal control file.  --ignore-missing-info skips the KiCad-internal
  # libraries (libkigal, the .kiface plugins...), which ship inside the
  # package and belong to no apt package; LD_LIBRARY_PATH lets their
  # DT_NEEDED entries resolve against the staged tree.  The unquoted
  # expansions are intentional: staged paths contain no spaces.  stderr is
  # kept in a log file for diagnosis.
  executables=$(find "${staged_prefix}" -type f -executable | sort)
  libs=$(find "${staged_prefix}" \( -name '*.so' -o -name '*.kiface' \) | sort)
  local shlibs_dir="${BUILD_ROOT}/dpkg-shlibdeps-run"
  mkdir -p "${shlibs_dir}/debian"
  cat > "${shlibs_dir}/debian/control" <<EOF
Source: kicad
Section: electronics
Priority: optional
Maintainer: local build <local@invalid>

Package: kicad
Architecture: ${arch}
Description: KiCad EDA suite (unofficial build)
EOF
  # shellcheck disable=SC2086
  shlibs=$( cd "${shlibs_dir}" \
              && LD_LIBRARY_PATH="${staged_prefix}/lib/${multiarch}:${staged_prefix}/lib:${LD_LIBRARY_PATH:-}" \
                 dpkg-shlibdeps -O --ignore-missing-info ${executables} ${libs} \
                 2>"${BUILD_ROOT}/dpkg-shlibdeps.log" \
              | sed -n 's/^shlibs:Depends=//p' || true )
  if [[ -z "${shlibs}" ]]; then
    echo "WARNING: dpkg-shlibdeps produced no dependency list; see ${BUILD_ROOT}/dpkg-shlibdeps.log" >&2
  fi

  cat > "${STAGE_DIR}/DEBIAN/control" <<EOF
Package: kicad
Version: ${version}
Section: electronics
Priority: optional
Architecture: ${arch}
Maintainer: local build <local@invalid>
Depends: ${shlibs:-libc6}, shared-mime-info, xdg-utils
Description: KiCad EDA suite (unofficial build)
 A full featured electronics design automation suite including
 schematic capture, PCB layout, Gerber viewer and more.
 Built from source by tools/build-linux-packages.sh.
EOF

  # Kept for debugging, same as the CI shlibs.deps file.
  echo "${shlibs}" > "${STAGE_DIR}/DEBIAN/shlibs.deps"

  # /opt libraries sit outside the default dynamic loader path: ship an
  # ld.so configuration fragment pointing at the staged lib dirs and run
  # ldconfig on install and removal.  Listed as a conffile so local edits
  # survive package upgrades.
  install -d "${STAGE_DIR}/etc/ld.so.conf.d"
  {
    echo "${INSTALL_PREFIX}/lib"
    echo "${INSTALL_PREFIX}/lib/${multiarch}"
  } > "${STAGE_DIR}/etc/ld.so.conf.d/kicad.conf"
  echo "/etc/ld.so.conf.d/kicad.conf" > "${STAGE_DIR}/DEBIAN/conffiles"
  cat > "${STAGE_DIR}/DEBIAN/postinst" <<'EOF'
#!/bin/sh
set -e
ldconfig
EOF
  cat > "${STAGE_DIR}/DEBIAN/prerm" <<'EOF'
#!/bin/sh
set -e
ldconfig
EOF
  chmod 755 "${STAGE_DIR}/DEBIAN/postinst" "${STAGE_DIR}/DEBIAN/prerm"

  mkdir -p "${DIST_DIR}"
  dpkg-deb --root-owner-group --build "${STAGE_DIR}" "${DIST_DIR}/${deb_file}"
  echo "Built ${DIST_DIR}/${deb_file}"
}

# Mirrors "Build AppImage" in the linux-appimage job.
package_appimage()
{
  log "Assembling AppImage"
  local distro_id arch multiarch
  distro_id="$(host_distro_id)"
  arch="$(host_arch)"
  multiarch="$(host_multiarch)"

  rm -rf "${APPDIR}"
  # DESTDIR redirects the whole install tree - including rules with absolute
  # DESTINATIONs, like the API schemas - under AppDir/opt/kicad; the tree is
  # then folded into usr/, the FHS layout KiCad's AppImage code paths expect
  # (common/kiway.cpp dso_search_path, common/paths.cpp
  # GetStockEDALibraryPath).  The compiled-in /opt/kicad paths stay in the
  # binaries and are inert inside the image - AppRun overrides them.
  DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"
  rmdir "${APPDIR}/usr" 2>/dev/null || true
  mv "${APPDIR}/opt/kicad" "${APPDIR}/usr"
  rmdir "${APPDIR}/opt"

  # The AppImage always uses the CI FHS layout - the whole KiCad tree under
  # usr/: KIWAY::dso_search_path() looks for the .kiface files next to
  # $APPDIR/usr/bin/kicad (common/kiway.cpp) and
  # PATHS::GetStockEDALibraryPath() resolves $APPDIR/usr/share/kicad
  # (common/paths.cpp).  The compiled-in /opt/kicad prefix is inert inside
  # the image; AppRun points the data paths back into it.

  # Find KiCad's internal libraries: the versioned shared libraries
  # (libkigal.so.10.99.0, libkicommon...) do not match a plain '*.so' glob
  # and the .kiface plugins are dlopen()ed, so linuxdeploy's own dependency
  # scan misses them.  CMAKE_INSTALL_LIBDIR resolves to the multiarch dir on
  # Debian/Ubuntu, hence the whole lib tree is searched.
  local internal_libs=() lib
  while IFS= read -r lib; do
    internal_libs+=( "${lib}" )
  done < <(find "${APPDIR}/usr/lib" -type f \
             \( -name '*.kiface' -o -name '*.so' -o -name '*.so.*' \) | sort)
  if [[ ${#internal_libs[@]} -eq 0 ]]; then
    echo "ERROR: no internal KiCad libraries found under ${APPDIR}/usr/lib" >&2
    find "${APPDIR}/usr" -maxdepth 3 -type d | sort >&2
    exit 1
  fi
  echo "Found ${#internal_libs[@]} internal libraries"

  # linuxdeploy resolves DT_NEEDED entries against the top level of
  # AppDir/usr/lib only - it does not recurse into subdirectories - so
  # flatten the internal libraries up to usr/lib; it then deploys them (and
  # their system dependencies) itself.
  mkdir -p "${APPDIR}/usr/lib"
  for lib in "${internal_libs[@]}"; do
    # Skip libraries that already sit at the flatten target (e.g. libkiapi
    # installs straight into usr/lib on some layouts).
    if [[ "${lib}" != "${APPDIR}/usr/lib/"* ]]; then
      cp -f "${lib}" "${APPDIR}/usr/lib/"
    fi
  done
  export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${LD_LIBRARY_PATH:-}"

  # Bundle libstdc++/libgcc_s too: linuxdeploy treats them as always available
  # on the target system, but carrying our own makes the AppImage start on
  # older distros whose system libstdc++ lacks the required GLIBCXX versions.
  for lib in /usr/lib/"${multiarch}"/libstdc++.so.6 \
             /usr/lib/"${multiarch}"/libgcc_s.so.1; do
    if [[ -e "${lib}" ]]; then cp -fL "${lib}" "${APPDIR}/usr/lib/"; fi
  done

  # The GLVND dispatch libraries (libOpenGL, libGL, libEGL, libGLdispatch,
  # libGLX, libGLESv2) are on linuxdeploy's blacklist, but unlike the actual
  # drivers (mesa etc., dlopened at runtime from the host) they are
  # driver-independent loaders: bundle them so the image also starts on
  # machines without a full GL dev stack (e.g. missing libopengl0).
  for lib in libOpenGL.so.0 libGL.so.1 libEGL.so.1 \
             libGLdispatch.so.0 libGLX.so.0 libGLESv2.so.2; do
    if [[ -e "/usr/lib/${multiarch}/${lib}" ]]; then
      cp -fL "/usr/lib/${multiarch}/${lib}" "${APPDIR}/usr/lib/"
    fi
  done

  # The stock libraries land in the same usr/share/kicad tree as KiCad's own
  # data (resources, schemas, i18n), matching the CI AppImage: both are read
  # through AppRun's KICAD_STOCK_DATA_HOME and the $APPDIR/usr hardcodes.
  bundle_stock_into "${APPDIR}/usr"

  # Libraries built from source (e.g. a /usr/local wxWidgets) have no dpkg
  # copyright file for linuxdeploy to collect - it warns and ships them
  # without a license text.  Cover the LGPL obligation explicitly.
  install -d "${APPDIR}/usr/share/doc/kicad"
  cp -f /usr/share/common-licenses/LGPL-3 \
     "${APPDIR}/usr/share/doc/kicad/LICENSE.LGPL-3" 2>/dev/null || true

  # Custom AppRun: the binaries' RUNPATH points at the system install
  # location (/opt/kicad/lib/...) and the flattened internal libraries live
  # in usr/lib - point the loader at the image's lib dirs explicitly.
  # KICAD_STOCK_DATA_HOME points PATHS::GetStockDataPath at the bundled data
  # (API schemas, icon archive, i18n, stock libraries): the compiled-in
  # /opt/kicad prefix does not exist on machines running the image.
  #
  # The versioned path variables are exported as externally defined - same
  # as the workflow's AppRun: the mount point changes on every run, and
  # LIBRARY_MANAGER::StockTableReferenceURI only stores the re-resolvable
  # "${KICAD10_TEMPLATE_DIR}/..." token (instead of an absolute path that
  # dangles after remounting) when they are.  The KICAD10_ prefix must match
  # ENV_VAR::GetVersionedEnvVarName() of the built version.
  cat > "${BUILD_ROOT}/AppRun" <<'EOF'
#!/bin/bash
HERE="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$HERE/usr/lib/kicad:$HERE/usr/lib/x86_64-linux-gnu:$HERE/usr/lib64:$HERE/usr/lib:${LD_LIBRARY_PATH}"
export KICAD_STOCK_DATA_HOME="$HERE/usr/share/kicad"
export KICAD10_TEMPLATE_DIR="$KICAD_STOCK_DATA_HOME/template"
export KICAD10_SYMBOL_DIR="$KICAD_STOCK_DATA_HOME/symbols"
export KICAD10_FOOTPRINT_DIR="$KICAD_STOCK_DATA_HOME/footprints"
export KICAD10_3DMODEL_DIR="$KICAD_STOCK_DATA_HOME/3dmodels"
exec "$HERE/usr/bin/kicad" "$@"
EOF
  chmod +x "${BUILD_ROOT}/AppRun"

  # Swap the x86_64 linuxdeploy variant for the host architecture if needed
  # (upstream publishes aarch64 under the same release).
  local deploy_url="${LINUXDEPLOY_URL//x86_64/${arch}}"
  # Cached per architecture so x86_64 and aarch64 builds do not evict each
  # other's copy of the tool.  Presence (-f) is the cache criterion: a file
  # that was copied or restored without the exec bit would otherwise fail
  # the -x test and be re-downloaded on every run (chmod below heals it).
  local deploy_tool="${DOWNLOADS_DIR}/linuxdeploy-${arch}.AppImage"
  if [[ ! -f "${deploy_tool}" ]]; then
    log "Downloading linuxdeploy (${deploy_url})"
    curl -sSfL -o "${deploy_tool}" "${deploy_url}"
  fi
  chmod +x "${deploy_tool}"

  # Note: while generating the squashfs, linuxdeploy downloads the type-2
  # runtime from GitHub on every run - its CLI has neither a runtime-file
  # flag nor an appdir output plugin to avoid that, so this ~650 KB fetch is
  # a fixed part of every packaging run.
  # Prefer the main KiCad desktop entry so the AppImage is named and
  # iconified after KiCad itself; without it linuxdeploy silently uses the
  # first .desktop it finds (which happens to be pcbnew's).
  local desktop_file
  local desktop_args=()
  local find_args=( -name 'org.kicad.*.desktop' )
  for lib in "${DESKTOP_EXCLUDES[@]}"; do
    find_args+=( ! -name "${lib}" )
  done
  desktop_file="$(find "${APPDIR}/usr/share/applications" "${find_args[@]}" | head -1 || true)"
  if [[ -n "${desktop_file}" ]]; then
    desktop_args+=( --desktop-file "${desktop_file}" )
  fi

  # Icon diagnostics: linuxdeploy deploys the icon named by the desktop
  # file's Icon= key (kicad) - copying it to the AppDir root and linking
  # .DirIcon.  Both must exist in the staged tree for that to work; without
  # them file managers and the dock show a generic icon.
  log "Icon / desktop diagnostics"
  ls "${APPDIR}/usr/share/applications/" 2>/dev/null || true
  find "${APPDIR}/usr/share/icons" -name 'kicad*.svg' 2>/dev/null | head -3 || true

  log "Running linuxdeploy"
  mkdir -p "${DIST_DIR}"
  (
    cd "${DIST_DIR}"
    "${deploy_tool}" --appimage-extract-and-run \
      --appdir "${APPDIR}" \
      --custom-apprun "${BUILD_ROOT}/AppRun" \
      "${desktop_args[@]}" \
      --output appimage

    # linuxdeploy names the image after the desktop entry; normalize it to
    # KiCad-<arch>-<distro> so builds from different hosts do not collide.
    local generated
    generated="$(find . -maxdepth 1 -type f -name '*.AppImage' ! -name 'linuxdeploy*' | head -1 || true)"
    [[ -n "${generated}" ]] || die "linuxdeploy produced no AppImage"
    mv -f "${generated}" "KiCad-${arch}-${distro_id}.AppImage"
  )
  # Fallback icon deployment: if linuxdeploy skipped it (e.g. the desktop
  # entry was not matched), install the stock svg into the AppDir root by
  # hand so the image and the dock still get an icon.
  if [[ ! -e "${APPDIR}/.DirIcon" ]]; then
    log "WARNING: linuxdeploy deployed no icon; installing the stock one"
    local icon_src
    icon_src="$(find "${APPDIR}/usr/share/icons" -name 'kicad.svg' 2>/dev/null | head -1 || true)"
    if [[ -n "${icon_src}" ]]; then
      cp -f "${icon_src}" "${APPDIR}/kicad.svg"
      ln -sf kicad.svg "${APPDIR}/.DirIcon"
    else
      die "no kicad.svg found anywhere in ${APPDIR}/usr/share/icons - the icons component did not install"
    fi
  fi
  log "Icon check: .DirIcon -> $(readlink "${APPDIR}/.DirIcon" 2>/dev/null || echo file)"

  echo "Built ${DIST_DIR}/KiCad-${arch}-${distro_id}.AppImage"

  # Self-containment diagnostics: list what the packaged binaries would still
  # resolve from system paths at runtime.  Entries like glibc, libGL/Mesa,
  # X11, DBus, input-method or fontconfig libs are expected and correct.
  echo "--- runtime dependencies resolving from SYSTEM paths (informational) ---"
  ldd "${APPDIR}/usr/bin/kicad" 2>/dev/null \
    | grep -Ev 'AppDir|/tmp/\.mount|linuxdeploy' || true
  echo "------------------------------------------------------------------------"
}

# Cross-build entry: run this script inside an ubuntu:<DISTRO> container so
# the artifacts bind to that LTS's glibc and package set, exactly like the CI
# runners (ubuntu-24.04 / ubuntu-26.04).  Requires podman or docker.  The
# container runs as root (apt needs it) and the scratch tree is chowned back
# to the invoking user afterwards; the stock repos clone over https because
# the container has no SSH keys.
run_in_container()
{
  local tool
  if [[ -n "${CONTAINER_TOOL:-}" ]]; then
    tool="${CONTAINER_TOOL}"
  elif command -v podman >/dev/null 2>&1; then
    tool="podman"
  elif command -v docker >/dev/null 2>&1; then
    tool="docker"
  else
    die "DISTRO=${DISTRO} needs podman or docker to run an ubuntu:${DISTRO} container (or build on a matching host instead)"
  fi

  # :Z on podman for SELinux-labeled hosts; docker ignores it but Ubuntu
  # hosts have no SELinux anyway.
  local vols=( -v "${REPO_DIR}:/work" )
  if [[ "${tool}" == podman ]]; then
    vols=( -v "${REPO_DIR}:/work:Z" )
  fi

  log "Building inside an ubuntu:${DISTRO} container (${tool}); first run installs the full toolchain and compiles everything - expect a long build"
  local inner_cmd
  inner_cmd="$( cat <<EOF
set -euo pipefail
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends git curl ca-certificates
git config --global --add safe.directory /work
export GIT_CLONE_PROTO=https
# Keep the download caches inside the mounted tree: the container HOME is
# not persisted.
export DOWNLOADS_DIR=/work/build/linux-packages/downloads
bash /work/tools/build-linux-packages.sh ${TARGET}
chown -R $(id -u):$(id -g) /work/build/linux-packages
EOF
)"
  "${tool}" run --rm "${vols[@]}" -e IN_CONTAINER=1 "ubuntu:${DISTRO}" bash -c "${inner_cmd}"

  log "Done. ${DISTRO} artifacts in ${DIST_DIR}:"
  ls -lh "${DIST_DIR}"
}

main()
{
  load_workflow_config

  if [[ "${TARGET}" == check ]]; then
    show_config
    verify_config
    echo "workflow configuration OK"
    return
  fi

  # A DISTRO different from the host goes through the container path: the
  # build host's glibc bounds what the resulting packages run on.
  if [[ -z "${IN_CONTAINER:-}" && -n "${DISTRO:-}" && "${DISTRO}" != "$(host_distro_id)" ]]; then
    run_in_container
    return
  fi
  if [[ -n "${IN_CONTAINER:-}" ]]; then
    DISTRO=""
  fi

  require_tools
  mkdir -p "${BUILD_ROOT}" "${DIST_DIR}" "${STOCK_DIR}" "${DOWNLOADS_DIR}"

  # One-time migration of the download caches from their former location
  # inside the build tree, so a wiped BUILD_ROOT does not trigger multi-GB
  # re-downloads.
  if [[ -d "${BUILD_ROOT}/stock" && ! -d "${STOCK_DIR}" ]]; then
    log "Migrating stock library cache to ${STOCK_DIR}"
    mkdir -p "$(dirname "${STOCK_DIR}")"
    # Move the repository clones only: their CMake build dirs embed absolute
    # paths and cannot be relocated (install_stock_libraries re-creates
    # them on demand).
    mv "${BUILD_ROOT}/stock" "${STOCK_DIR}"
    rm -rf "${STOCK_DIR}"/*-build
  fi
  if [[ -f "${BUILD_ROOT}/linuxdeploy.AppImage" ]]; then
    local new_deploy="${DOWNLOADS_DIR}/linuxdeploy-$(host_arch).AppImage"
    if [[ ! -f "${new_deploy}" ]]; then
      log "Migrating linuxdeploy cache to ${new_deploy}"
      mkdir -p "${DOWNLOADS_DIR}"
      mv "${BUILD_ROOT}/linuxdeploy.AppImage" "${new_deploy}"
    fi
  fi

  install_deps
  configure_and_build

  if [[ "${TARGET}" == deb || "${TARGET}" == all ]]; then
    package_deb
  fi
  if [[ "${TARGET}" == appimage || "${TARGET}" == all ]]; then
    package_appimage
  fi

  log "Done. Artifacts in ${DIST_DIR}:"
  ls -lh "${DIST_DIR}"
}

main
