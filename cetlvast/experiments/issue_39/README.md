# Saturating math for CETL (issue #39): evaluation and design proposal

Evaluation of the inputs to [CETL issue 39](https://github.com/OpenCyphal/CETL/issues/39) and a proposal for how
saturating arithmetic should enter CETL, written against CETL revision `d55d530`. Inputs examined:

- Pavel's `Archive.tar.gz` (`dyshlo/saturating.hpp` + `test.cpp`, Zubax 2022, C++20) from the 2026-10-21 comment.
- The original libcyphal `util/math.hpp` the issue title refers to (libuavcan 2019, removed from libcyphal in
  January 2024) and today's libcyphal (`140995f`, 2025-12-17).
- nunavut's C++ templates and support header (`ed853f9`, 2026-10-19).
- C++26 `[numeric.sat]` as adopted (P0543R3) and as renamed (P4052R0), plus the related accepted and unaccepted papers.

Everything in `prototype/` was compiled and tested as described in section 8. Nothing in this directory is committed;
it exists to anchor the discussion on the issue and the forum.

## 1. Recommendation

1. Add a standard-exact polyfill of C++26 saturation arithmetic: `cetl::pf26::saturating_add`, `saturating_sub`,
   `saturating_mul`, `saturating_div` and `saturating_cast` in `cetl/pf26/numeric.hpp`, with a
   `cetl/pf26/cetlpf.hpp` that selects the standard library's functions when
   `__cpp_lib_saturation_arithmetic` says they exist. The switch must be three-way, because libraries shipped
   between 2023 and 2026 only have the pre-rename names (`std::add_sat`), and it must key on the feature-test
   macro rather than `__cplusplus` (every compiler tested still reports `202400L` for `-std=c++26`).
2. Add a CETL-specific header, `cetl/saturating.hpp`, holding what the standard does not provide but the two
   consumers need: `cetl::saturate_to_bits<N>(x)` for nunavut's `saturated intN` fields and a
   `cetl::saturating<T>` wrapper type modelled on Rust's `std::num::Saturating<T>` and on the better parts of
   Pavel's `Saturating<T>`.
3. Reuse the two existing code bases as design input only. Both contain correctness bugs (sections 4 and 5), and
   Pavel's is C++20-only. The C++14 prototype in `prototype/` passes exhaustive and randomized oracle tests on
   GCC 7.5, 13, 15 and Clang 21, 23 in every standard mode from C++14 to C++26.

## 2. What C++26 standardized

[P0543R3 "Saturation arithmetic"](https://wg21.link/p0543r3) (Jens Maurer) was adopted at Kona in November 2023.
[P4052R0 "Renaming saturation arithmetic functions"](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p4052r0.html)
(Jan Schultke, Corentin Jabot, 2026-03-13) resolved NB comment FR-026-265 by renaming all five functions; it was
accepted for C++26 in March 2026 and cppreference, libc++ and libstdc++ already reflect it. The paper removes the
old names outright (no deprecated aliases) and bumps the feature-test macro.

| P0543R3 name (2023)  | Final C++26 name (P4052R0) | Semantics                                                                    |
|----------------------|----------------------------|------------------------------------------------------------------------------|
| `add_sat(x, y)`      | `saturating_add(x, y)`     | `x + y` with infinite range, clamped to `[min, max]` of `T`                  |
| `sub_sat(x, y)`      | `saturating_sub(x, y)`     | same for `x - y`                                                             |
| `mul_sat(x, y)`      | `saturating_mul(x, y)`     | same for `x * y`                                                             |
| `div_sat(x, y)`      | `saturating_div(x, y)`     | `x / y`; precondition `y != 0`; `min / -1` returns `max`                     |
| `saturate_cast<T>(u)`| `saturating_cast<T>(u)`    | `u` if representable in `T`, otherwise the nearer bound of `T`               |

Properties that constrain the polyfill:

- Header `<numeric>`, all `constexpr noexcept`, freestanding.
- `T` must be a *signed or unsigned integer type*: `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`
  are excluded. `saturating_cast` accepts integer types only on both sides (no floating point).
- No integral promotion: `saturating_add<unsigned char>(255, 4)` is 255, but `saturating_add(UCHAR_MAX, 4)`
  deduces `int` and yields 259.
- Both operands share one `T`; mixed types fail deduction exactly like `std::min`/`std::max`. A wrapper class
  template was considered and rejected by LEWG as over-engineered and un-adoptable by C.
- Division by zero is a precondition violation (undefined behaviour, not a core constant expression).
- `__cpp_lib_saturation_arithmetic` is `202311L` for the P0543R3 names and `202603L` for the final names.

Library support (cppreference compiler-support table, confirmed locally where marked):

| Library / toolchain                         | `-std=c++26` macro value   | Names available          |
|---------------------------------------------|----------------------------|--------------------------|
| libstdc++ 14, 15 (g++-15 15.1 confirmed)     | `202311L`                  | `add_sat` family only    |
| libstdc++ 16                                | `202603L` (per cppreference) | `saturating_add` family |
| libc++ 18 to 22 (Apple clang 21 confirmed)  | `202311L`                  | `add_sat` family only    |
| libc++ 23 (Homebrew clang 23.1.3 confirmed) | `202603L`                  | `saturating_add` family  |
| libstdc++ 13 (g++-13 confirmed)             | undefined                  | none                     |
| MSVC STL                                    | unverified                 | cppreference lists an entry I could not find in the STL changelog; check before relying on it |

All of these compilers report `__cplusplus == 202400L` under `-std=c++26` today (`202603L` is the final value), so
a `pf26` switch based on `__cplusplus` would be wrong on every current toolchain.

Related standard work:

- C++26 also imports C23's `<stdckdint.h>` (`ckd_add`, `ckd_sub`, `ckd_mul`: overflow-detecting, wrapping result;
  P3370R1, macro `__cpp_lib_stdckdint_h` = `202603L`). These are the building block, not a replacement.
- [P2956R3](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p2956r3) proposes `std::simd` overloads of the
  saturating functions (LEWG, 2026).
- [P3161R6 "Unified integer overflow arithmetic"](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3161r6.html)
  (Tiago Freire, target C++29) adds `carrying_add`, `borrowing_sub`, `widening_mul`, `narrowing_div`; R6 was
  rewritten to follow the P4052R0 naming scheme. [P3018R0](https://open-std.org/JTC1/SC22/WG21/docs/papers/2023/p3018r0.pdf)
  (Weis) covers the same low-level ground.

## 3. Older and unaccepted proposals

- [N3864](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3864.html) (Fioravante, 2014, "A constexpr
  bitwise operations library") contained `satadd(l, r)` and `satsub(l, r)` returning `decltype(l + r)`, i.e. with
  promotion. P0543 cites it as the only prior proposal and deliberately dropped promotion and mixed types.
- Lawrence Crowl's P0103 (overflow-detecting and double-wide operations) and P0105 (rounding and overflow) were
  merged into the SG6 working paper [P1889R1 "C++ Numerics Work In Progress"](https://wg21.link/p1889r1) (2020).
  Its section 3.2 "Overflow-Handling Operations" is the closest thing to a general saturation design the committee
  has seen: `enum class overflow { impossible, undefined, abort, exception, special, saturate, modulo_shifted }`,
  `convert<overflow::saturate, T>(value)`, `limit(mode, lower, upper, value)`,
  `limit_twoscomp(mode, upper, value)` (clamp to `[-upper-1, upper]`, which is exactly the N-bit case nunavut
  needs) and `scale_up(mode, value, count)` (saturating left shift). Section 3.1 has `overflow_add(T*, T, T)` etc.
  P1889 has not advanced since R1.
- John McFarlane's P0037 (fixed point), P0554 (composition of arithmetic types) and P0828 (elastic integers), and
  the CNL library that implements them, take the opposite approach: an `overflow_integer<Rep, Tag>` whose overflow
  policy (including saturation) is a template parameter. Nothing from this line has been adopted.
- Rust stabilized `std::num::Saturating<T>` in 1.74 (2023): a transparent wrapper whose binary operators are
  defined only between two `Saturating<T>`, whose compound assignments also accept plain `T`, with `Neg` for signed
  types only, no shifts, and division by zero panicking as for plain integers. It is the best-tested precedent for
  a wrapper type and the model recommended below.
- Hardware and compiler support: ARM `QADD`/`QSUB`, SSE2/NEON vector saturating ops; GCC 5+ and Clang provide
  `__builtin_{add,sub,mul}_overflow`, and Clang 21 adds `__builtin_elementwise_add_sat`/`sub_sat`, which is what
  libstdc++ and libc++ use.

## 4. Evaluation of Pavel's archive (`dyshlo::saturating`)

Contents: free functions `add`, `sub`, `neg`, `mul`, `div`, `mod`, `lsl`, `lsr`, `cast<T>` constrained by a
`Saturable` concept; a `Saturating<T>` wrapper with "contagious" semantics and widest-operand result types;
`std::numeric_limits<Saturating<T>>` with `is_modulo = false`; a test file with exhaustive-random checks against a
constrained reference and an Eigen matrix test.

What is good and worth keeping as design input:

- The add/sub/mul algorithms are promotion-free and correct: exhaustive `int8_t` and `uint8_t` tests (65,536 pairs
  each) found no mismatches.
- The wrapper's semantics are mostly right: implicit construction from `T`, explicit saturating construction from
  other types, explicit conversion out, saturating `++`/`--`, unary minus that maps `min` to `max`, and the
  `numeric_limits` specialization.
- The idea of testing against a wide-integer oracle.

Defects found (compiled with Clang 23 `-std=c++20 -fsanitize=undefined`; harness in the session scratchpad):

| # | Defect | Reproduction and observed result |
|---|--------|----------------------------------|
| 1 | `cast<T>(A)` compares through `std::common_type_t<T, A>`, which is unsigned whenever either side is unsigned, so sign is lost. The `Saturating<T>(R)` converting constructor inherits it. | `cast<int8_t>(uint64_t{5})` = -128; `cast<int32_t>(uint32_t{5})` = INT_MIN; `cast<uint64_t>(int8_t{-1})` = UINT64_MAX (expected 5, 5, 0) |
| 2 | `div` does not handle `min / -1`. | `div<int32_t>(INT_MIN, -1)`: UBSan "division of -2147483648 by -1"; `div<int8_t>(-128, -1)` = -128 (expected 127). `mod` is built on `div`, so `mod<int8_t>(-128, -1)` = -128 (expected 0) |
| 3 | `lsr(num, nbit)` shifts by `nbit` unguarded. | `lsr<int64_t>(-1, 64)`: UBSan "shift exponent 64 is too large" |
| 4 | Floating-point `cast` only clamps strictly outside the range and ignores NaN. | `cast<int64_t>(9223372036854775808.0)`: UBSan out-of-range conversion; `cast<int8_t>(NaN)`: UBSan out-of-range (returned 0 by luck) |
| 5 | Mixed-type operators promote to the widest operand, so a literal silently widens the result. | `int_sat8_t(100) + 100` has type `Saturating<int>` and value 200: the 8-bit saturation is lost unless the literal is spelled `int8_t{100}` |
| 6 | `Saturable` admits `bool`, `char`, `char16_t` (the standard excludes them). | `static_assert(Saturable<bool>)` holds |
| 7 | `numeric_limits<Saturating<T>>::max()` returns `T`, not `Saturating<T>`. | `decltype(numeric_limits<int_sat8_t>::max())` is `int8_t` |
| 8 | Division by zero saturates with the dividend's sign and `0 / 0 == 0`. | Not a bug, but a semantic choice that diverges from both C++26 and Rust (precondition / panic) |

Portability: the header needs concepts, `requires` clauses (including one on an alias template), `operator<=>`,
`if constexpr` and `auto` non-type template parameters. None of that survives C++14, so this is a rewrite in any
case; the C++14 techniques are shown in `prototype/`.

Verdict: keep the wrapper design ideas and the test approach, rewrite the algorithms on top of the standard
functions, drop width promotion, and resolve item 8 on the forum (recommendation below is the standard's).

## 5. Evaluation of the original libcyphal `util/math.hpp`

History: added to libuavcan in May 2019 (`uavcan/util/math.hpp`, PR 212), renamed twice, last version at
`libcyphal/include/libcyphal/util/math.hpp`, removed by the design-doc commit `8d95780` (PR 333, 2024-01-18)
together with its only consumer, `types/time.hpp`, whose `Duration` and `TimePoint` did microsecond arithmetic
through `util::saturating_add/sub`. The current libcyphal has no saturating code at all.

API: `saturating_add`/`saturating_sub` for two signed, two unsigned, and unsigned-LHS/signed-RHS operands;
promotion-free; the signed versions are not `constexpr`.

Defects found (Clang 23 `-std=c++17 -fsanitize=undefined`, header compiled with stubbed libcyphal includes):

| # | Defect | Reproduction and observed result |
|---|--------|----------------------------------|
| 1 | `_saturating_sub_d` evaluates `std::abs(numeric_limits::min() - left)`; for `left == 0` that is `abs(INT_MIN)`. | `saturating_sub<int64_t>(0, 5)` = INT64_MIN (UBSan: negation of INT64_MIN). Any `int`, `long` or `long long` subtraction from zero is wrong; `int8_t`/`int16_t` work only because they promote to `int` |
| 2 | The mixed unsigned/signed overloads cast a negative RHS to the unsigned type before dispatching. | `saturating_sub<uint8_t, int8_t>(0, -127)` = 129 (expected 127); 48,514 of the 65,536 `(uint8_t, int8_t)` pairs are wrong |
| 3 | Same-type `int8_t`/`uint8_t` add and sub are correct exhaustively, and the 64-bit edge cases other than defect 1 are correct. | |

Verdict: the "promotionless" requirement it documents is right and is met by the prototype; the code itself should
not be hoisted.

## 6. How the two consumers would use it

### libcyphal

- `application/node/heartbeat_producer.hpp:181` narrows uptime seconds with
  `static_cast<std::uint32_t>(uptime_in_secs.count())`. That is the textbook `saturating_cast` call:
  `message_.uptime = cetl::saturating_cast<std::uint32_t>(uptime_in_secs.count());`
- Deadlines are formed as `approx_now + response_timeout_` (`registry_provider.hpp:124,144`,
  `get_info_provider.hpp:225`, `heartbeat_producer.hpp:191`) and `TimePoint::max()` is used as a "never" sentinel
  (`client_impl.hpp:308`, `single_threaded_executor.hpp:228`). Today the left operand is always a real `now`, so
  nothing overflows, but a caller-supplied `Duration::max()` timeout would be undefined behaviour on the signed
  `int64` representation. This is the use case the 2019 header served. A `std::chrono` adapter
  (`saturating_add(TimePoint, Duration)`) is a natural later addition on top of layer 1 and is listed as an open
  question rather than proposed now.
- The many `static_cast<std::int32_t>(a) - static_cast<std::int32_t>(b)` comparison idioms in the session trees
  would benefit from C++20 `std::cmp_less`/`in_range` polyfills, which the `saturating_cast` implementation needs
  anyway (section 7.1).

### nunavut

Today `_serialize_integer` in `src/nunavut/lang/cpp/templates/serialization.j2` emits, for every `saturated`
integer whose width is not a standard width:

```c++
std::int16_t _sat0_ = obj.i10_4[i];
if (_sat0_ < -512) { _sat0_ = -512; }
if (_sat0_ > 511)  { _sat0_ = 511;  }
```

That is a clamp to an N-bit range, not a `saturating_cast` to a standard type, so the standard functions do not
cover it. With layer 2 the template becomes one line in the CETL flavour (which already includes
`<cetl/pf17/cetlpf.hpp>` in every generated header):

```jinja
{% if t is saturated and not t.standard_bit_length %}
    {% set ref_value = 'sat'|to_template_unique_name %}
    const {{ t|type_from_primitive }} {{ ref_value }} = cetl::saturate_to_bits<{{ t.bit_length }}U>({{ reference }});
{% else %}
    {% set ref_value = reference %}
{% endif %}
```

`_serialize_float` (clamp finite values to the float16 range, pass infinities and NaN through) and deserialization
(values always fit by construction) need nothing from CETL. The `std` flavour of nunavut has no CETL dependency and
can keep the inline clamp or use `std::clamp`.

## 7. Proposed design

### 7.1 Layer 1: `cetl/pf26/numeric.hpp` (standard-exact)

```c++
namespace cetl { namespace pf26 {
template <typename T> constexpr T saturating_add(T x, T y) noexcept;   // SFINAE-constrained to signed/unsigned integer types
template <typename T> constexpr T saturating_sub(T x, T y) noexcept;
template <typename T> constexpr T saturating_mul(T x, T y) noexcept;
template <typename T> constexpr T saturating_div(T x, T y) noexcept;   // precondition y != 0, CETL_DEBUG_ASSERT
template <typename T, typename U> constexpr T saturating_cast(U x) noexcept;
}}
```

- Semantics, constraints and the single-`T` deduction model are the standard's, so code written against
  `cetl::saturating_add` recompiles unchanged against `std::saturating_add`.
- Implementation: `__builtin_{add,sub,mul}_overflow` when the compiler has them (GCC 5+, Clang; `__has_builtin`
  checked), otherwise a portable, UB-free predicate (`y > 0 ? x > max - y : x < min - y`, and the sign-paired
  division test for multiplication so that `min / -1` is never formed). The bound is chosen from the operand signs
  exactly as libstdc++ and libc++ do. Both paths are `constexpr` in C++14 (GCC 7.5 verified).
- `saturating_cast` needs sign-correct comparisons; the prototype carries C++14 versions of `std::cmp_less` and
  `std::cmp_greater` in `detail`. They are good candidates to graduate to `cetl::pf20::cmp_equal/cmp_less/...` and
  `cetl::pf20::in_range` in `cetl/pf20/utility.hpp` as true C++20 polyfills.
- C++14 techniques used instead of C++20 ones: `std::enable_if_t` with a defaulted `int` NTTP instead of concepts,
  tag dispatch on `std::is_signed<T>` instead of `if constexpr`, a comma expression for the debug assert.

### 7.2 `cetl/pf26/cetlpf.hpp`: three-way switch

```c++
#include <numeric>   // defines __cpp_lib_saturation_arithmetic where the library has the feature
#if defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202603L)
namespace cetl { using std::saturating_add; /* ... */ using std::saturating_cast; }
#elif defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202311L)
namespace cetl { template <typename T> constexpr T saturating_add(T x, T y) noexcept { return std::add_sat(x, y); } /* ... */ }
#else
#include "cetl/pf26/numeric.hpp"
namespace cetl { using cetl::pf26::saturating_add; /* ... */ }
#endif
```

The middle branch is what makes this a true polyfill on the toolchains OpenCyphal actually uses this year
(libstdc++ 14/15, libc++ 18-22): the standard functions are used, under the final standard names.

### 7.3 Layer 2: `cetl/saturating.hpp` (CETL-specific, namespace `cetl`)

- `template <std::size_t Bits, typename T> constexpr T saturate_to_bits(T x) noexcept`: clamps `x` to the range of
  a `Bits`-wide integer of `T`'s signedness (`max() >> (width - Bits)`, `-max - 1` or `0`); `static_assert`s
  `1 <= Bits <= width`. This is P1889's `limit_twoscomp` specialised for what DSDL needs.
- `template <typename T> class saturating` (Rust `Saturating<T>` rules, Pavel's ergonomics where they agree):
  - `T` must be a signed or unsigned integer type (same constraint as the standard functions).
  - Implicit construction from exactly `T`; explicit, saturating construction from any other integer type
    (`cetl::saturating<std::int8_t>{1000}` is 127).
  - `+ - * /` and the compound forms only between two `saturating<T>`. No width promotion, no implicit mixing with
    plain integers, which removes Pavel's defect 5 by construction. `++`/`--` saturate.
  - Unary minus for signed `T` only (`-saturating<int8_t>{-128}` is 127).
  - `value()` and explicit `operator T()`; `==`, `!=`, `<` (and the rest) between two `saturating<T>`.
  - `std::numeric_limits<cetl::saturating<T>>` specialised with `is_modulo = false` and bounds returned as
    `saturating<T>` (fixes Pavel's defect 7).
  - Division by zero is a precondition (debug-asserted), as in the standard and in Rust.
- Possible later additions, deliberately out of the first PR: a floating-point to integer saturating conversion with
  defined NaN behaviour (Rust `as` semantics: NaN to 0, infinities to the bounds; it cannot safely be an overload of
  `saturating_cast` because the standard's version is ill-formed, not SFINAE-friendly, for non-integers), `%` and
  shifts on the wrapper (P1889 `scale_up` precedent for a saturating left shift), `abs`, and the `std::chrono`
  adapters for libcyphal.

### 7.4 Deliberately not proposed

- Pavel's widest-operand promotion rules and contagion with plain integers: the standard chose one `T`, and the
  literal-widening trap is real.
- Saturating division by zero: no precedent in C++26 or Rust; a debug assert catches the bug instead of hiding it.
- Shifts and modulo in layer 1: not in the standard; keep `pf26` standard-exact.
- `int_sat8_t`-style aliases: `cetl::saturating<std::int8_t>` reads fine and follows CONTRIBUTING's naming table.

### 7.5 Housekeeping

- `cetl/cetl.hpp`: add `CETL_CPP_STANDARD_23 202302L` and `CETL_CPP_STANDARD_26 202603L` for completeness; the
  existing `static_assert` already accepts anything `>= CETL_CPP_STANDARD_20`, so current compilers' `202400L` is
  fine. The `pf26` selection itself must not use these.
- Doxygen: document namespace `cetl::pf26` next to `pf17`/`pf20`, add `example_13_saturating.cpp`, mention `pf26`
  in the README's polyfill paragraph.
- Version: additive, so `CETL_VERSION_MINOR` 5 to 6.

### 7.6 Tests (CETLVaSt)

- `cetlvast/suites/unittest/test_pf26_numeric.cpp`: `static_assert` constant-expression cases; exhaustive 8-bit
  add/sub/mul/div against a wide oracle; exhaustive `saturating_cast` from every 8/16-bit type into every 8-64-bit
  type; randomized 32/64-bit trials with edge-value injection; run in both the polyfill and the standard-selected
  configurations (a `cpp-26` preset is needed for the latter; toolshed's clang 22 takes the `202311L` branch, which
  is the branch most users will hit this year).
- `cetlvast/suites/unittest/test_saturating.cpp` for layer 2, plus `test_deprecated_names.cpp` untouched.
- `cetlvast/suites/compile`: must-not-compile cases for `bool`, `char`, mixed `T`, and `saturating<T> + int`.
- `cetlvast/build_fuzz`: a libFuzzer target comparing against `__int128`, matching the existing suite.
- The GCC 7 preset: the prototype already passes on g++-7.5 with CETL's full warning set.

## 8. Prototype evidence

Files: `prototype/cetl/pf26/numeric.hpp`, `prototype/cetl/pf26/cetlpf.hpp`, `prototype/cetl/saturating.hpp`,
`prototype/test_sat.cpp`, `prototype/run_matrix.sh`. The test contains 31 `static_assert`s, exhaustive 8-bit
checks of all four operations (65,536 pairs per type), exhaustive `saturating_cast` from the 8- and 16-bit types
into all eight fixed-width types, 300,000 random-plus-edge trials per 32/64-bit type against a 128-bit oracle,
exhaustive `saturate_to_bits` checks for `int16_t/10` and `uint16_t/12`, and wrapper spot checks. Flags were CETL's
`default.cmake` warning set with `-Werror`, plus UBSan on Clang.

| Toolchain                                   | `-std=`              | Path selected                     | Result  |
|---------------------------------------------|----------------------|-----------------------------------|---------|
| Homebrew clang 23.1.3 / libc++ 23           | c++14, 17, 20, 23    | `cetl::pf26` polyfill (builtins)  | 0 fails, UBSan clean |
| Homebrew clang 23.1.3 / libc++ 23           | c++26                | `std` (P4052R0 names)             | 0 fails |
| Homebrew clang 23.1.3                       | c++14, no builtins   | `cetl::pf26` polyfill (portable)  | 0 fails |
| Homebrew clang 23.1.3                       | c++14, debug asserts | `cetl::pf26` polyfill             | 0 fails |
| Apple clang 21 / libc++                     | c++14                | `cetl::pf26` polyfill             | 0 fails |
| Apple clang 21 / libc++                     | c++26                | `std` (P0543R3 names, adapted)    | 0 fails |
| g++-13 / libstdc++ 13                       | c++14 (3 variants)   | `cetl::pf26` polyfill             | 0 fails |
| g++-15 / libstdc++ 15                       | c++14                | `cetl::pf26` polyfill             | 0 fails |
| g++-15 / libstdc++ 15                       | c++26 (2 variants)   | `std` (P0543R3 names, adapted)    | 0 fails |
| g++-7.5 (toolshed ts24.4.3, aarch64)        | c++14 (3 variants)   | `cetl::pf26` polyfill             | 0 fails |

Reproduce locally with `cetlvast/experiments/issue_39/prototype/run_matrix.sh`. For GCC 7:

```bash
docker run --rm -v "$PWD:/w:ro" ghcr.io/opencyphal/toolshed:ts24.4.3 bash -c 'g++-7 -std=c++14 -pedantic -Wall -Wextra -Werror -Wconversion -Wsign-conversion -I /w/cetlvast/experiments/issue_39/prototype -I /w/include -o /tmp/t /w/cetlvast/experiments/issue_39/prototype/test_sat.cpp && /tmp/t'
```

## 9. Open questions for the forum thread

1. Wrapper division by zero: precondition (proposed; standard and Rust) or Pavel's sign-preserving saturation.
2. Should `saturating<T>` accept plain `T` on the right of binary operators for ergonomics (`s + T{3}`), or only
   in compound assignment as Rust does (proposed)?
3. Name of the N-bit clamp: `saturate_to_bits<N>` (proposed), `saturating_cast_bits`, or P1889's `limit_twoscomp`.
4. Whether the `cmp_*`/`in_range` helpers should be published as `cetl::pf20` polyfills in the same release.
5. Floating-point to integer saturating conversion: in scope, and under what name.
6. `std::chrono` adapters for libcyphal's deadline arithmetic: CETL or libcyphal?

## 10. Suggested sequencing

1. PR 1 (CETL 1.6.0): layer 1 + `cetlpf.hpp`, tests, example, docs, `cpp-26` preset.
2. PR 2: layer 2 (`saturate_to_bits`, `saturating<T>`), tests and compile-fail cases.
3. Follow-ups outside CETL: nunavut `_serialize_integer` in the CETL flavour; libcyphal heartbeat uptime, then the
   chrono question.

## References

- P0543R3 Saturation arithmetic: https://wg21.link/p0543r3
- P4052R0 Renaming saturation arithmetic functions: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p4052r0.html
- cppreference `std::saturating_add`, `saturating_cast`, `saturating_div`, `ckd_add`, compiler support for C++26
- P1889R1 C++ Numerics Work In Progress: https://wg21.link/p1889r1
- P0103R1 Overflow-Detecting and Double-Wide Arithmetic Operations: https://wg21.link/p0103r1
- N3864 A constexpr bitwise operations library for C++: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2014/n3864.html
- P3161R6 Unified integer overflow arithmetic: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p3161r6.html
- P2956R3 std::simd saturating overloads: https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p2956r3
- Rust `std::num::Saturating`: https://doc.rust-lang.org/std/num/struct.Saturating.html
- libstdc++ `bits/sat_arith.h`, libc++ `__numeric/saturation_arithmetic.h` (implementation technique)
- libcyphal history: `git log --all -- '*util/math.hpp'` in OpenCyphal-Garage/libcyphal (added b74d56c 2019-05-04, removed 8d95780 2024-01-18)
