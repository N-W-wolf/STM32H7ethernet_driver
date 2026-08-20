#!/usr/bin/env bash
# Flash an ELF/HEX/BIN image through an ST-Link probe.

set -Eeuo pipefail

PROJECT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build/$BUILD_TYPE}"
FIRMWARE="${FIRMWARE:-}"
BACKEND="${STLINK_BACKEND:-auto}"
ADDRESS="${FLASH_ADDRESS:-0x08000000}"
CONNECT_MODE="${STLINK_CONNECT_MODE:-UR}"
VERIFY=true
RESET=true
CHECK_ONLY=false

usage() {
    cat <<'EOF'
Usage: ./flash.sh [Debug|Release] [options]

Options:
  -f, --file FILE       Firmware image (.elf, .hex or .bin)
  -B, --build-dir DIR   Directory searched for firmware
  -b, --backend NAME    auto, cube or st-flash (default: auto)
  -a, --address ADDR    Load address for raw BIN files (default: 0x08000000)
      --connect-mode M  CubeProgrammer mode: UR, HOTPLUG or NORMAL (default: UR)
      --no-verify       Do not verify after programming
      --no-reset        Do not reset after programming
      --check           Validate tools and image without accessing hardware
  -h, --help            Show this help

Environment equivalents: BUILD_TYPE, BUILD_DIR, FIRMWARE, STLINK_BACKEND,
FLASH_ADDRESS and STLINK_CONNECT_MODE.
EOF
}

while (($#)); do
    case "$1" in
        Debug|Release|RelWithDebInfo|MinSizeRel)
            BUILD_TYPE="$1"
            BUILD_DIR="$PROJECT_DIR/build/$1"
            shift
            ;;
        -f|--file)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            FIRMWARE="$2"
            shift 2
            ;;
        -B|--build-dir)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            BUILD_DIR="$2"
            shift 2
            ;;
        -b|--backend)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            BACKEND="$2"
            shift 2
            ;;
        -a|--address)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            ADDRESS="$2"
            shift 2
            ;;
        --connect-mode)
            [[ $# -ge 2 ]] || { echo "error: $1 requires a value" >&2; exit 2; }
            CONNECT_MODE="$2"
            shift 2
            ;;
        --no-verify)
            VERIFY=false
            shift
            ;;
        --no-reset)
            RESET=false
            shift
            ;;
        --check)
            CHECK_ONLY=true
            shift
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

[[ "$BUILD_DIR" == /* ]] || BUILD_DIR="$PROJECT_DIR/$BUILD_DIR"
if [[ -n "$FIRMWARE" && "$FIRMWARE" != /* ]]; then
    FIRMWARE="$PROJECT_DIR/$FIRMWARE"
fi

case "$BACKEND" in
    auto)
        if command -v STM32_Programmer_CLI >/dev/null 2>&1; then
            BACKEND=cube
        elif command -v st-flash >/dev/null 2>&1; then
            BACKEND=st-flash
        else
            echo "error: neither STM32_Programmer_CLI nor st-flash is in PATH" >&2
            exit 127
        fi
        ;;
    cube)
        command -v STM32_Programmer_CLI >/dev/null 2>&1 || {
            echo "error: STM32_Programmer_CLI is not in PATH" >&2
            exit 127
        }
        ;;
    st-flash)
        command -v st-flash >/dev/null 2>&1 || {
            echo "error: st-flash is not in PATH" >&2
            exit 127
        }
        ;;
    *)
        echo "error: backend must be auto, cube or st-flash" >&2
        exit 2
        ;;
esac

if [[ -z "$FIRMWARE" ]]; then
    [[ -d "$BUILD_DIR" ]] || {
        echo "error: build directory not found: $BUILD_DIR" >&2
        echo "hint: run ./build.sh $BUILD_TYPE first" >&2
        exit 1
    }
    if [[ "$BACKEND" == cube ]]; then
        image_formats=(elf hex bin)
    else
        image_formats=(bin elf hex)
    fi
    for image_format in "${image_formats[@]}"; do
        latest_record="$(find "$BUILD_DIR" -type f -name "*.$image_format" ! -path '*/CMakeFiles/*' -printf '%T@ %p\n' 2>/dev/null | sort -nr | head -n 1 || true)"
        if [[ -n "$latest_record" ]]; then
            FIRMWARE="${latest_record#* }"
            break
        fi
    done
fi

[[ -n "$FIRMWARE" && -f "$FIRMWARE" ]] || {
    echo "error: no firmware image found; run ./build.sh or pass --file" >&2
    exit 1
}

# Prefer ELF over a newly generated BIN/HEX when all share the same basename.
base="${FIRMWARE%.*}"
if [[ "$BACKEND" == cube && -f "$base.elf" ]]; then
    FIRMWARE="$base.elf"
fi

echo "Backend : $BACKEND"
echo "Firmware: $FIRMWARE"

if $CHECK_ONLY; then
    echo "Check OK; hardware was not accessed."
    exit 0
fi

if [[ "$BACKEND" == cube ]]; then
    cmd=(STM32_Programmer_CLI -c port=SWD mode="$CONNECT_MODE")
    if [[ "$FIRMWARE" == *.bin ]]; then
        cmd+=(-w "$FIRMWARE" "$ADDRESS")
    else
        cmd+=(-w "$FIRMWARE")
    fi
    $VERIFY && cmd+=(-v)
    $RESET && cmd+=(-rst)
    "${cmd[@]}"
else
    bin_file="$FIRMWARE"
    if [[ "$bin_file" != *.bin ]]; then
        if [[ -f "$base.bin" ]]; then
            bin_file="$base.bin"
        elif command -v arm-none-eabi-objcopy >/dev/null 2>&1; then
            bin_file="$base.bin"
            arm-none-eabi-objcopy -O binary "$FIRMWARE" "$bin_file"
        else
            echo "error: st-flash needs a BIN image (or arm-none-eabi-objcopy)" >&2
            exit 1
        fi
    fi
    cmd=(st-flash)
    $RESET && cmd+=(--reset)
    cmd+=(write "$bin_file" "$ADDRESS")
    "${cmd[@]}"
    if $VERIFY; then
        command -v cmp >/dev/null 2>&1 && command -v stat >/dev/null 2>&1 || {
            echo "error: st-flash verification requires cmp and stat" >&2
            exit 127
        }
        verify_file="$(mktemp "${TMPDIR:-/tmp}/st-flash-verify.XXXXXX")"
        trap 'rm -f -- "$verify_file"' EXIT
        image_size="$(stat -c %s "$bin_file")"
        st-flash read "$verify_file" "$ADDRESS" "$image_size"
        if cmp -s "$bin_file" "$verify_file"; then
            echo "Verification OK"
        else
            echo "error: flash verification failed" >&2
            exit 1
        fi
    fi
fi
