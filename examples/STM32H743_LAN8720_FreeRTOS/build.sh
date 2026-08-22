#!/usr/bin/env bash
# Configure and build a CMake project. Works with CMake presets when available,
# and falls back to a conventional build/<type> directory otherwise.

set -Eeuo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
PRESET="${CMAKE_PRESET:-}"
BUILD_DIR="${BUILD_DIR:-}"
TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-}"
TARGET=""
JOBS=""
CLEAN_FIRST=false
FRESH=false
CONFIGURE_ARGS=()

usage() {
    cat <<'EOF'
Usage: ./build.sh [Debug|Release] [options] [-- <extra CMake options>]

Options:
  -t, --type TYPE       CMake build type (default: Debug)
  -p, --preset NAME     Configure preset (default: same as build type)
  -B, --build-dir DIR   Build directory; also disables automatic preset use
  -j, --jobs [N]        Parallel build, optionally limited to N jobs
      --target NAME     Build only the named target
      --toolchain FILE  CMake toolchain file for projects without presets
      --clean           Clean before building
      --fresh           Reconfigure without the existing CMake cache
  -h, --help            Show this help

Environment equivalents: BUILD_TYPE, CMAKE_PRESET, BUILD_DIR,
CMAKE_TOOLCHAIN_FILE. Extra arguments after -- are passed to CMake configure.
EOF
}

while (($#)); do
    case "$1" in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            BUILD_TYPE="$1"
            shift
            ;;
        -t|--type)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            BUILD_TYPE="$2"
            shift 2
            ;;
        -p|--preset)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            PRESET="$2"
            shift 2
            ;;
        -B|--build-dir)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        -j|--jobs)
            if [[ $# -ge 2 && "$2" =~ ^[0-9]+$ ]]; then
                JOBS="$2"
                shift 2
            else
                JOBS="auto"
                shift
            fi
            ;;
        --target)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            TARGET="$2"
            shift 2
            ;;
        --toolchain)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            TOOLCHAIN_FILE="$2"
            shift 2
            ;;
        --clean)
            CLEAN_FIRST=true
            shift
            ;;
        --fresh)
            FRESH=true
            shift
            ;;
        --)
            shift
            CONFIGURE_ARGS+=("$@")
            break
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "error: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

command -v cmake >/dev/null 2>&1 || {
    echo "error: cmake is not in PATH" >&2
    exit 127
}

if [[ -n "$BUILD_DIR" && "$BUILD_DIR" != /* ]]; then
    BUILD_DIR="$PROJECT_DIR/$BUILD_DIR"
fi

has_preset=false
if [[ -z "$BUILD_DIR" && ( -f "$PROJECT_DIR/CMakePresets.json" || -f "$PROJECT_DIR/CMakeUserPresets.json" ) ]]; then
    [[ -n "$PRESET" ]] || PRESET="$BUILD_TYPE"
    if cmake --list-presets=configure -S "$PROJECT_DIR" 2>/dev/null | grep -Fq "\"$PRESET\""; then
        has_preset=true
    elif [[ -n "${CMAKE_PRESET:-}" ]]; then
        echo "error: configure preset '$PRESET' does not exist" >&2
        cmake --list-presets=configure -S "$PROJECT_DIR" >&2 || true
        exit 2
    fi
fi

configure_cmd=(cmake)
$FRESH && configure_cmd+=(--fresh)

if $has_preset; then
    echo "==> Configuring preset: $PRESET"
    configure_cmd+=(--preset "$PRESET" -S "$PROJECT_DIR")
    configure_cmd+=("${CONFIGURE_ARGS[@]}")
    "${configure_cmd[@]}"
    build_cmd=(cmake --build --preset "$PRESET")
    ARTIFACT_ROOT="$PROJECT_DIR/build"
else
    [[ -n "$BUILD_DIR" ]] || BUILD_DIR="$PROJECT_DIR/build/$BUILD_TYPE"
    echo "==> Configuring: $BUILD_DIR ($BUILD_TYPE)"
    configure_cmd+=(-S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
    if [[ -z "$TOOLCHAIN_FILE" && -f "$PROJECT_DIR/cmake/gcc-arm-none-eabi.cmake" ]]; then
        TOOLCHAIN_FILE="$PROJECT_DIR/cmake/gcc-arm-none-eabi.cmake"
    fi
    [[ -z "$TOOLCHAIN_FILE" ]] || configure_cmd+=(-DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE")
    command -v ninja >/dev/null 2>&1 && configure_cmd+=(-G Ninja)
    configure_cmd+=("${CONFIGURE_ARGS[@]}")
    "${configure_cmd[@]}"
    build_cmd=(cmake --build "$BUILD_DIR" --config "$BUILD_TYPE")
    ARTIFACT_ROOT="$BUILD_DIR"
fi

$CLEAN_FIRST && build_cmd+=(--clean-first)
[[ -z "$TARGET" ]] || build_cmd+=(--target "$TARGET")
if [[ "$JOBS" == "auto" ]]; then
    build_cmd+=(--parallel)
elif [[ -n "$JOBS" ]]; then
    build_cmd+=(--parallel "$JOBS")
else
    build_cmd+=(--parallel)
fi

echo "==> Building"
"${build_cmd[@]}"

# Embedded CMake projects commonly produce ELF files. Also create BIN/HEX
# images when GNU objcopy is available, so multiple flashing tools can be used.
latest_elf=""
if [[ -d "$ARTIFACT_ROOT" ]]; then
    latest_record="$(find "$ARTIFACT_ROOT" -type f -name '*.elf' ! -path '*/CMakeFiles/*' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 || true)"
    [[ -z "$latest_record" ]] || latest_elf="${latest_record#* }"
fi

if [[ -n "$latest_elf" ]]; then
    objcopy=""
    for candidate in arm-none-eabi-objcopy llvm-objcopy; do
        if command -v "$candidate" >/dev/null 2>&1; then
            objcopy="$candidate"
            break
        fi
    done
    if [[ -n "$objcopy" ]]; then
        "$objcopy" -O binary "$latest_elf" "${latest_elf%.elf}.bin"
        "$objcopy" -O ihex "$latest_elf" "${latest_elf%.elf}.hex"
    fi
    command -v arm-none-eabi-size >/dev/null 2>&1 && arm-none-eabi-size "$latest_elf" || true
    echo "==> Firmware: $latest_elf"
else
    echo "==> Build completed (no .elf artifact detected)"
fi
