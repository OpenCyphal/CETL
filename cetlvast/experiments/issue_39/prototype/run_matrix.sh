#!/usr/bin/env bash
# Builds and runs the prototype test with every compiler/standard combination found on this machine.
# Usage: ./run_matrix.sh   (from any directory)
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
CETL_INCLUDE="$HERE/../../../../include"
WARN=(-pedantic -Wall -Wextra -Werror -Wconversion -Wsign-conversion -Wsign-compare -Wsign-promo -Wtype-limits -Wshadow
      -Wzero-as-null-pointer-constant -Wfloat-equal -Wdouble-promotion -Wcast-align)
run() {  # compiler std [extra-defines] [sanitizer flags...]
    local CXX="$1" STD="$2" EXTRA="${3:-}"; shift 3 || shift $#
    local OUT; OUT="$(mktemp -t sat_XXXXXX)"
    if "$CXX" -std="$STD" "${WARN[@]}" $EXTRA "$@" -I "$HERE" -I "$CETL_INCLUDE" -o "$OUT" "$HERE/test_sat.cpp" 2> "$OUT.log"; then
        printf '%-32s %-6s %-36s => ' "$("$CXX" --version | head -1 | cut -c1-32)" "$STD" "${EXTRA:-(builtins)}"; "$OUT" | tr '\n' ' '; echo
    else
        printf '%-32s %-6s %-36s => COMPILE FAILED\n' "$("$CXX" --version | head -1 | cut -c1-32)" "$STD" "${EXTRA:-(builtins)}"; grep error "$OUT.log" | head -5
    fi
    rm -f "$OUT" "$OUT.log"
}
UB=(-fsanitize=undefined -fno-sanitize-recover=all)
for CXX in /opt/homebrew/opt/llvm/bin/clang++ /usr/bin/clang++; do
    [ -x "$CXX" ] || continue
    for STD in c++14 c++17 c++20 c++23 c++26; do run "$CXX" "$STD" "" "${UB[@]}"; done
    run "$CXX" c++14 -DCETL_PF26_NO_OVERFLOW_BUILTINS "${UB[@]}"
    run "$CXX" c++14 -DCETL_ENABLE_DEBUG_ASSERT=1 "${UB[@]}"
done
for CXX in g++-13 g++-14 g++-15; do   # Homebrew GCC on macOS has no libubsan, so no sanitizer here
    command -v "$CXX" > /dev/null || continue
    run "$CXX" c++14 ""
    "$CXX" -std=c++26 -x c++ -fsyntax-only - < /dev/null 2> /dev/null && run "$CXX" c++26 ""   # older GCC has no c++26 mode
    run "$CXX" c++14 -DCETL_PF26_NO_OVERFLOW_BUILTINS
done
