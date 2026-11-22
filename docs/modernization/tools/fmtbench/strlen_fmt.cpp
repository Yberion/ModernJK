// {fmt}'s length of a C string (basic_string_view(const char*)) in every case a fix of it must keep, built against a
// copy of {fmt} given by the include path: build/fmt-ref (12.2.1 unchanged: a loop with MSVC), libs/fmt (its patch:
// the C library's strlen with MSVC, outside constant evaluation) or build/fmt-a (__builtin_strlen with MSVC too,
// strlen_a.patch).
// Plan step 22's check before proposing a fix to {fmt}'s authors. Written in C++14, MSVC's oldest mode; the program
// header-only ({fmt}'s FMT_HEADER_ONLY), so that one command builds it, the constant cases on fmt/base.h alone.
//
// -DCASE=<n> (/DCASE=<n>), 1 by default:
//   1  a program: the lengths and the formatted texts checked at run time, then the time of one call on a text of 300
//      characters: basic_string_view(const char*) alone, and format_to_n("{}\n") (fmtbench's case)
//   2  compiled only: a constexpr fmt::string_view made from a string literal
//   3  compiled only: the same from the array of a constexpr object ({fmt}'s issue 4423; its own test, in
//      base-test.cc, runs in C++20 only, FMT_USE_CONSTEVAL)
#ifndef CASE
#define CASE 1
#endif

#if CASE == 1
#define FMT_HEADER_ONLY
#include <fmt/format.h>

#include <chrono>
#include <cstdio>
#include <cstring>

namespace {
const char* volatile g_text; // read again at every call, so that the compiler cannot keep the length

template <typename F> double BestNs(F f, int iters, int runs)
{
    double best = 1e30;
    for (int r = 0; r < runs; ++r) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int i = 0; i < iters; ++i)
            f();
        const auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / iters;
        if (ns < best)
            best = ns;
    }
    return best;
}
} // namespace

int main()
{
    static char text[301];
    std::memset(text, 'a', 300);
    int failures = 0;
    char out[512];
    const size_t lengths[] = {0, 1, 15, 16, 31, 32, 255, 256, 300};
    for (const size_t n : lengths) {
        text[n] = '\0';
        const char* p = text;
        if (fmt::string_view(p).size() != n) {
            std::printf("FAIL: string_view of %zu characters: %zu\n", n, fmt::string_view(p).size());
            ++failures;
        }
        const auto r = fmt::format_to_n(out, sizeof(out) - 1, "{}\n", p);
        if (r.size != n + 1 || std::memcmp(out, text, n) != 0 || out[n] != '\n') {
            std::printf("FAIL: format_to_n of %zu characters: %zu\n", n, r.size);
            ++failures;
        }
        text[n] = 'a';
    }
    text[300] = '\0';
    g_text = text;

    size_t sink = 0;
    const double view_ns = BestNs(
        [&] {
            const char* p = g_text;
            sink += fmt::string_view(p).size();
        },
        1000000, 7);
    const double format_ns = BestNs(
        [&] {
            const char* p = g_text;
            sink += fmt::format_to_n(out, sizeof(out) - 1, "{}\n", p).size;
        },
        1000000, 7);
    std::printf("%s string_view %.1f ns, format_to_n %.1f ns (300 characters; %zu)\n", failures ? "FAIL" : "ok", view_ns, format_ns, sink % 10);
    return failures ? 1 : 0;
}
#elif CASE == 2 || CASE == 3
#include <fmt/base.h>
#endif

#if CASE == 2
constexpr fmt::string_view literal_view("x={}");
static_assert(literal_view.size() == 4, "the length of the literal");
#elif CASE == 3
template <size_t N> struct fixed_string {
    char data[N] = {};
    constexpr fixed_string(const char (&m)[N])
    {
        for (size_t i = 0; i != N; ++i)
            data[i] = m[i];
    }
};
constexpr fixed_string<5> format_text("x={}");
constexpr fmt::string_view array_view(format_text.data);
static_assert(array_view.size() == 4, "the length of the object's array");
#elif CASE != 1
#error "CASE must be 1, 2 or 3"
#endif
