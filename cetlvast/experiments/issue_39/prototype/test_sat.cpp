#include "cetl/pf26/cetlpf.hpp"
#include "cetl/saturating.hpp"
#include <cstdint>
#include <cstdio>
#include <random>
#include <type_traits>

// ---- compile-time behaviour (all three source paths must agree) ----
static_assert(cetl::saturating_add<std::int8_t>(100, 100) == 127, "");
static_assert(cetl::saturating_add<std::int8_t>(-100, -100) == -128, "");
static_assert(cetl::saturating_add<std::uint8_t>(250, 10) == 255, "");
static_assert(cetl::saturating_sub<std::uint8_t>(3, 5) == 0, "");
static_assert(cetl::saturating_sub<std::int8_t>(-128, 1) == -128, "");
static_assert(cetl::saturating_mul<std::int8_t>(-128, -1) == 127, "");
static_assert(cetl::saturating_mul<std::int64_t>(INT64_C(4000000000), INT64_C(4000000000)) == INT64_MAX, "");
static_assert(cetl::saturating_div<std::int8_t>(-128, -1) == 127, "");
static_assert(cetl::saturating_div<std::int32_t>(-7, 2) == -3, "");
static_assert(cetl::saturating_cast<std::int8_t>(std::uint64_t{5}) == 5, "");
static_assert(cetl::saturating_cast<std::int32_t>(std::uint32_t{5}) == 5, "");
static_assert(cetl::saturating_cast<std::uint8_t>(-5) == 0, "");
static_assert(cetl::saturating_cast<std::uint64_t>(std::int8_t{-1}) == 0, "");
static_assert(cetl::saturating_cast<std::int8_t>(std::uint8_t{200}) == 127, "");
static_assert(cetl::saturating_cast<std::int8_t>(696) == 127, "");
static_assert(cetl::saturating_cast<std::uint8_t>(-696) == 0, "");
static_assert(cetl::saturating_cast<std::int64_t>(std::uint64_t{0xFFFFFFFFFFFFFFFFu}) == INT64_MAX, "");
static_assert(cetl::saturate_to_bits<10>(std::int16_t{+0x5555}) == 511, "");
static_assert(cetl::saturate_to_bits<10>(std::int16_t{-0x6666}) == -512, "");
static_assert(cetl::saturate_to_bits<10>(std::int16_t{-0xAA}) == -0xAA, "");
static_assert(cetl::saturate_to_bits<12>(std::uint16_t{0xFFFF}) == 4095, "");
static_assert(cetl::saturate_to_bits<16>(std::int16_t{-32768}) == -32768, "");
static_assert(cetl::saturate_to_bits<8>(std::uint8_t{255}) == 255, "");
static_assert(cetl::saturate_to_bits<1>(std::int8_t{5}) == 0, "");
static_assert(cetl::saturate_to_bits<1>(std::int8_t{-5}) == -1, "");
static_assert((cetl::saturating<std::int8_t>{100} + cetl::saturating<std::int8_t>{100}).value() == 127, "");
static_assert((-cetl::saturating<std::int8_t>{std::int8_t{-128}}).value() == 127, "");
static_assert(cetl::saturating<std::int8_t>{1000}.value() == 127, "");
static_assert(cetl::saturating<std::uint8_t>{-1}.value() == 0, "");
static_assert(std::is_same<decltype(cetl::saturating<std::int8_t>{} + cetl::saturating<std::int8_t>{}),
                           cetl::saturating<std::int8_t>>::value, "");

__extension__ typedef __int128 i128;
__extension__ typedef unsigned __int128 u128;
template <class T> using W = typename std::conditional<std::is_signed<T>::value, i128, u128>::type;
template <class T> i128 clampi(i128 v) { const i128 lo = std::numeric_limits<T>::min(), hi = std::numeric_limits<T>::max(); return v < lo ? lo : v > hi ? hi : v; }
template <class T> W<T> clampw(W<T> v) { const W<T> lo = std::numeric_limits<T>::min(), hi = std::numeric_limits<T>::max(); return v < lo ? lo : v > hi ? hi : v; }
template <class To, class From> i128 ref_cast(From v) { const i128 w = v; const i128 lo = std::numeric_limits<To>::min(), hi = std::numeric_limits<To>::max(); return w < lo ? lo : w > hi ? hi : w; }
static int fails = 0;
#define EXPECT(c) do { if (!(c)) { ++fails; if (fails <= 10) std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

template <class T> void exhaustive8() {
    const long lo = std::numeric_limits<T>::min(), hi = std::numeric_limits<T>::max();
    for (long a = lo; a <= hi; ++a) for (long b = lo; b <= hi; ++b) {
        const T x = static_cast<T>(a), y = static_cast<T>(b);
        EXPECT((i128) cetl::saturating_add(x, y) == clampi<T>((i128) a + (i128) b));
        EXPECT((i128) cetl::saturating_sub(x, y) == clampi<T>((i128) a - (i128) b));
        EXPECT((i128) cetl::saturating_mul(x, y) == clampi<T>((i128) a * (i128) b));
        if (b != 0) EXPECT((i128) cetl::saturating_div(x, y) == clampi<T>((i128) a / (i128) b));
    }
}
template <class To, class From> void exhaustive_cast() {
    const long lo = std::numeric_limits<From>::min(), hi = std::numeric_limits<From>::max();
    for (long a = lo; a <= hi; ++a) { const From x = static_cast<From>(a); EXPECT((i128) cetl::saturating_cast<To>(x) == ref_cast<To>(x)); }
}
template <class From> void exhaustive_casts_from() {
    exhaustive_cast<std::int8_t, From>(); exhaustive_cast<std::uint8_t, From>(); exhaustive_cast<std::int16_t, From>(); exhaustive_cast<std::uint16_t, From>();
    exhaustive_cast<std::int32_t, From>(); exhaustive_cast<std::uint32_t, From>(); exhaustive_cast<std::int64_t, From>(); exhaustive_cast<std::uint64_t, From>();
}
template <class T> void random64(std::mt19937_64& g) {
    std::uniform_int_distribution<T> any(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    const T edges[] = {std::numeric_limits<T>::min(), static_cast<T>(std::numeric_limits<T>::min() + 1), T{0}, T{1}, static_cast<T>(-1), static_cast<T>(std::numeric_limits<T>::max() - 1), std::numeric_limits<T>::max()};
    for (int i = 0; i < 300000; ++i) {
        const T x = (i % 3 == 0) ? edges[g() % 7] : any(g), y = (i % 5 == 0) ? edges[g() % 7] : any(g);
        EXPECT((W<T>) cetl::saturating_add(x, y) == clampw<T>((W<T>) x + y));
        EXPECT((i128) cetl::saturating_sub(x, y) == clampi<T>((i128) x - (i128) y));
        EXPECT((W<T>) cetl::saturating_mul(x, y) == clampw<T>((W<T>) x * y));
        if (y != 0) EXPECT((W<T>) cetl::saturating_div(x, y) == clampw<T>((W<T>) x / y));
        EXPECT((i128) cetl::saturating_cast<std::int8_t>(x) == ref_cast<std::int8_t>(x));
        EXPECT((i128) cetl::saturating_cast<std::uint32_t>(x) == ref_cast<std::uint32_t>(x));
        EXPECT((i128) cetl::saturating_cast<std::int64_t>(x) == ref_cast<std::int64_t>(x));
        EXPECT((i128) cetl::saturating_cast<std::uint64_t>(x) == ref_cast<std::uint64_t>(x));
    }
}
int main() {
    std::printf("source: %s | __cplusplus=%ld | builtins=%d\n", CETL_PF26_SATURATION_SOURCE, (long) __cplusplus,
#if defined(CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS)
        (int) CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS
#else
        -1
#endif
    );
    exhaustive8<std::int8_t>(); exhaustive8<std::uint8_t>();
    exhaustive_casts_from<std::int8_t>(); exhaustive_casts_from<std::uint8_t>(); exhaustive_casts_from<std::int16_t>(); exhaustive_casts_from<std::uint16_t>();
    std::mt19937_64 g(39);
    random64<std::int64_t>(g); random64<std::uint64_t>(g); random64<std::int32_t>(g); random64<std::uint32_t>(g);
    // saturate_to_bits vs reference for all int16 / 10 bits
    for (long a = -32768; a <= 32767; ++a) { const long r = a < -512 ? -512 : a > 511 ? 511 : a; EXPECT(cetl::saturate_to_bits<10>(static_cast<std::int16_t>(a)) == r); }
    for (long a = 0; a <= 65535; ++a) { const long r = a > 4095 ? 4095 : a; EXPECT(cetl::saturate_to_bits<12>(static_cast<std::uint16_t>(a)) == r); }
    // wrapper
    cetl::saturating<std::uint8_t> u{std::uint8_t{3}}; u -= std::uint8_t{5}; EXPECT(u.value() == 0);
    cetl::saturating<std::int8_t> s{std::int8_t{-128}}; --s; EXPECT(s.value() == -128); s = -s; EXPECT(s.value() == 127);
    std::printf("fails: %d\n", fails);
    return fails != 0;
}
