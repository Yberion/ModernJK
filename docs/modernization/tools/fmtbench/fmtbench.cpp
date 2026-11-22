// The formatting benchmark of plan step 22: today's functions (vsnprintf behind Com_sprintf and va) against {fmt}
// (libs/fmt) the way the project's header calls it: a template taking a checked format string that calls a function
// which is not a template, with the arguments packed (fmt::format_args).
//
// usage: fmtbench [--runs N] [--iters N] [--core K] [--check-only]
//   --runs N      timed runs, the fastest kept (default 9)
//   --iters N     calls per case and run (default 200000)
//   --core K      the thread pinned to core K (as the netcodec A/B scripts do)
//   --check-only  only the comparison of the outputs
//
// It first checks that both write the same text: every case on every input, then 1,000,000 floats per float format.
// Then, per case, the warm time of one call in ns:
//   sprintf     Com_sprintf today: vsnprintf into the caller's buffer
//   fmt span    the header's Com_sprintf: fmt::vformat_to_n into a std::span<char>
//   va today    va: vsnprintf into one of 4 static buffers of 32,000 bytes
//   va string   va returning a std::string (fmt::vformat)
//   va static   va on {fmt} with today's 4 static buffers
//   va small    va returning a text of its own type: 255 characters in the object, the heap beyond
#include <fmt/format.h>

#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sched.h>
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define NOINLINE __declspec(noinline)
#define PRINTF_LIKE(f, a)
#else
#define NOINLINE __attribute__((noinline))
#define PRINTF_LIKE(f, a) __attribute__((format(printf, f, a)))
#endif

namespace {

constexpr int kVaString = 32000;
constexpr int kVaBuffers = 4;

// Today's functions, as src/codemp/qcommon/q_shared.cpp writes them (Q_vsnprintf is vsnprintf).
NOINLINE int old_sprintf(char* dest, int size, const char* format, ...) PRINTF_LIKE(3, 4);
NOINLINE int old_sprintf(char* dest, int size, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    const int len = std::vsnprintf(dest, static_cast<size_t>(size), format, ap);
    va_end(ap);
    return len;
}

NOINLINE const char* old_va(const char* format, ...) PRINTF_LIKE(1, 2);
NOINLINE const char* old_va(const char* format, ...)
{
    static char string[kVaBuffers][kVaString];
    static int index = 0;
    char* buf = string[index++ & (kVaBuffers - 1)];
    va_list ap;
    va_start(ap, format);
    std::vsnprintf(buf, kVaString, format, ap);
    va_end(ap);
    return buf;
}

// The header's functions on {fmt}.
NOINLINE size_t new_vsprintf(std::span<char> dest, fmt::string_view format, fmt::format_args args)
{
    const auto result = fmt::vformat_to_n(dest.data(), dest.size() - 1, format, args);
    *result.out = '\0';
    return result.size;
}

template <typename... T> size_t new_sprintf(std::span<char> dest, fmt::format_string<T...> format, T&&... args)
{
    return new_vsprintf(dest, format.get(), fmt::make_format_args(args...));
}

NOINLINE std::string new_vva_string(fmt::string_view format, fmt::format_args args) { return fmt::vformat(format, args); }

template <typename... T> std::string new_va_string(fmt::format_string<T...> format, T&&... args)
{
    return new_vva_string(format.get(), fmt::make_format_args(args...));
}

NOINLINE const char* new_vva_static(fmt::string_view format, fmt::format_args args)
{
    static char string[kVaBuffers][kVaString];
    static int index = 0;
    char* buf = string[index++ & (kVaBuffers - 1)];
    const auto result = fmt::vformat_to_n(buf, kVaString - 1, format, args);
    *result.out = '\0';
    return buf;
}

template <typename... T> const char* new_va_static(fmt::format_string<T...> format, T&&... args)
{
    return new_vva_static(format.get(), fmt::make_format_args(args...));
}

// va's result as the header writes it: up to 255 characters in the object itself, a text longer than that formatted
// a second time into an array on the heap.
class SmallText {
public:
    [[nodiscard]] const char* c_str() const noexcept { return heap_ ? heap_.get() : inline_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }

    NOINLINE static SmallText vformat(fmt::string_view format, fmt::format_args args);

private:
    static constexpr size_t kInline = 256;
    char inline_[kInline]; // written before it is read: not initialised, for speed
    std::unique_ptr<char[]> heap_;
    size_t size_ = 0;
};

SmallText SmallText::vformat(fmt::string_view format, fmt::format_args args)
{
    SmallText text;
    const auto result = fmt::vformat_to_n(text.inline_, kInline - 1, format, args);
    text.size_ = result.size;
    if (result.size < kInline) {
        *result.out = '\0';
    }
    else {
        text.heap_ = std::make_unique_for_overwrite<char[]>(result.size + 1);
        const auto again = fmt::vformat_to_n(text.heap_.get(), result.size, format, args);
        *again.out = '\0';
    }
    return text;
}

template <typename... T> SmallText new_va_small(fmt::format_string<T...> format, T&&... args)
{
    return SmallText::vformat(format.get(), fmt::make_format_args(args...));
}

// Inputs: as the game prints them (counts, scores, coordinates, names, a long text).
constexpr int kInputs = 1024;
struct Inputs {
    int ints[kInputs];
    float floats[kInputs];
    std::string names[kInputs];
    std::string longText;
};

uint32_t g_seed = 12345u;
uint32_t next_random()
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8;
}

Inputs make_inputs()
{
    Inputs in;
    for (int i = 0; i < kInputs; ++i) {
        in.ints[i] = static_cast<int>(next_random() % 101000u) - 1000;
        in.floats[i] = (static_cast<float>(next_random() % 2000000u) - 1000000.0f) / 997.0f;
        const int len = 5 + static_cast<int>(next_random() % 16u);
        for (int c = 0; c < len; ++c)
            in.names[i] += static_cast<char>('a' + next_random() % 26u);
    }
    for (int c = 0; c < 300; ++c)
        in.longText += static_cast<char>('a' + next_random() % 26u);
    return in;
}

// One case: the same text written by today's syntax and by {fmt}'s, through each implementation.
struct Case {
    const char* name;
    void (*oldSprintf)(const Inputs&, int, char*, int);
    void (*newSprintf)(const Inputs&, int, std::span<char>);
    const char* (*oldVa)(const Inputs&, int);
    std::string (*newVaString)(const Inputs&, int);
    const char* (*newVaStatic)(const Inputs&, int);
    SmallText (*newVaSmall)(const Inputs&, int);
};

#define CASE(label, printfFormat, fmtFormat, ...)                                                                                                    \
    Case{label,                                                                                                                                      \
         [](const Inputs& in, int i, char* d, int n) { old_sprintf(d, n, printfFormat, __VA_ARGS__); },                                            \
         [](const Inputs& in, int i, std::span<char> d) { new_sprintf(d, fmtFormat, __VA_ARGS__); },                                               \
         [](const Inputs& in, int i) { return old_va(printfFormat, __VA_ARGS__); },                                                                 \
         [](const Inputs& in, int i) { return new_va_string(fmtFormat, __VA_ARGS__); },                                                             \
         [](const Inputs& in, int i) { return new_va_static(fmtFormat, __VA_ARGS__); },                                                             \
         [](const Inputs& in, int i) { return new_va_small(fmtFormat, __VA_ARGS__); }}

const Case kCases[] = {
    CASE("int \"%i\"", "%i", "{}", in.ints[i]),
    CASE("int \"%03i\"", "%03i", "{:03}", in.ints[i] & 255),
    CASE("float \"%.2f\"", "%.2f", "{:.2f}", static_cast<double>(in.floats[i])),
    CASE("float \"%5.1f\"", "%5.1f", "{:5.1f}", static_cast<double>(in.floats[i])),
    CASE("command \"print \\\"%s\\n\\\"\"", "print \"%s\n\"", "print \"{}\n\"", in.names[i].c_str()),
    CASE("two strings \"%s^7 %s\\n\"", "%s^7 %s\n", "{}^7 {}\n", in.names[i].c_str(), in.names[(i + 1) & (kInputs - 1)].c_str()),
    CASE("info \"\\\\%s\\\\%i\"", "\\%s\\%i", "\\{}\\{}", in.names[i].c_str(), in.ints[i]),
    CASE("mixed \"%s: %i %i %i %.1f\\n\"", "%s: %i %i %i %.1f\n", "{}: {} {} {} {:.1f}\n", in.names[i].c_str(), in.ints[i],
         in.ints[(i + 1) & (kInputs - 1)], in.ints[(i + 2) & (kInputs - 1)], static_cast<double>(in.floats[i])),
    CASE("right-aligned \"%10s|%-8s|\"", "%10s|%-8s|", "{:>10}|{:8}|", in.names[i].c_str(), in.names[(i + 3) & (kInputs - 1)].c_str()),
    CASE("long text \"%s\\n\" (300 chars)", "%s\n", "{}\n", in.longText.c_str()),
};

int check_cases(const Inputs& in)
{
    int failures = 0;
    char a[1024];
    char b[1024];
    for (const Case& c : kCases) {
        for (int i = 0; i < kInputs; ++i) {
            c.oldSprintf(in, i, a, sizeof(a));
            c.newSprintf(in, i, std::span<char>(b));
            const std::string s = c.newVaString(in, i);
            const char* va1 = c.oldVa(in, i);
            const char* va2 = c.newVaStatic(in, i);
            const SmallText smallText = c.newVaSmall(in, i);
            if (std::strcmp(a, b) != 0 || s != a || std::strcmp(va1, a) != 0 || std::strcmp(va2, a) != 0 || std::strcmp(smallText.c_str(), a) != 0 ||
                smallText.size() != s.size()) {
                if (failures < 10)
                    std::printf("  DIFF %s input %d: [%s] [%s]\n", c.name, i, a, b);
                ++failures;
            }
        }
    }
    return failures;
}

// The floats: every float format of the game written by both, on values of every magnitude.
int check_floats()
{
    struct FloatFormat {
        const char* printfFormat;
        const char* fmtFormat;
    };
    const FloatFormat formats[] = {{"%f", "{:f}"},     {"%.1f", "{:.1f}"}, {"%.2f", "{:.2f}"}, {"%.3f", "{:.3f}"},
                                   {"%5.1f", "{:5.1f}"}, {"%g", "{:g}"},     {"%e", "{:e}"}};
    int failures = 0;
    char a[512];
    char b[512];
    for (const FloatFormat& f : formats) {
        int diffs = 0;
        g_seed = 777u;
        for (int i = 0; i < 1000000; ++i) {
            float v;
            if (i & 1) {
                // a finite float of any magnitude, its bits drawn
                uint32_t bits;
                do {
                    bits = (next_random() << 8) ^ next_random();
                } while (((bits >> 23) & 0xffu) == 0xffu);
                std::memcpy(&v, &bits, sizeof(v));
            }
            else {
                v = (static_cast<float>(next_random() % 20000000u) - 10000000.0f) / 1013.0f; // the game's range
            }
            const double d = v;
            std::snprintf(a, sizeof(a), f.printfFormat, d);
            const auto r = fmt::vformat_to_n(b, sizeof(b) - 1, fmt::string_view(f.fmtFormat), fmt::make_format_args(d));
            *r.out = '\0';
            if (std::strcmp(a, b) != 0) {
                if (diffs < 5)
                    std::printf("  DIFF %s: printf [%s] fmt [%s]\n", f.printfFormat, a, b);
                ++diffs;
            }
        }
        std::printf("  floats %-6s %d differences in 1000000\n", f.printfFormat, diffs);
        failures += diffs;
    }
    return failures;
}

volatile uint32_t g_sink = 0;

template <typename F> double time_one(int iters, F&& f)
{
    const auto start = std::chrono::steady_clock::now();
    uint32_t sink = 0;
    for (int i = 0; i < iters; ++i)
        sink += f(i & (kInputs - 1));
    const auto end = std::chrono::steady_clock::now();
    g_sink = g_sink + sink;
    return std::chrono::duration<double, std::nano>(end - start).count() / iters;
}

void pin_to_core(int core)
{
#ifdef _WIN32
    SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR{1} << core);
#else
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(core, &set);
    sched_setaffinity(0, sizeof(set), &set);
#endif
}

} // namespace

int main(int argc, char** argv)
{
    int runs = 9;
    int iters = 200000;
    bool checkOnly = false;
    for (int a = 1; a < argc; ++a) {
        if (!std::strcmp(argv[a], "--runs") && a + 1 < argc)
            runs = std::atoi(argv[++a]);
        else if (!std::strcmp(argv[a], "--iters") && a + 1 < argc)
            iters = std::atoi(argv[++a]);
        else if (!std::strcmp(argv[a], "--core") && a + 1 < argc)
            pin_to_core(std::atoi(argv[++a]));
        else if (!std::strcmp(argv[a], "--check-only"))
            checkOnly = true;
        else {
            std::fprintf(stderr, "usage: fmtbench [--runs N] [--iters N] [--core K] [--check-only]\n");
            return 2;
        }
    }

    const Inputs in = make_inputs();
    // the longest text a std::string holds without allocating (its small-string buffer), per standard library
    std::printf("std::string without allocation: up to %zu characters\n\n", std::string{}.capacity());
    std::printf("check: the same text from snprintf and {fmt}\n");
    const int caseFailures = check_cases(in);
    std::printf("  cases: %d differences\n", caseFailures);
    const int floatFailures = check_floats();
    if (checkOnly)
        return caseFailures + floatFailures ? 1 : 0;

    std::printf("\nwarm time per call, ns (fastest of %d runs of %d calls, the implementations alternated)\n", runs, iters);
    std::printf("%-36s %9s %9s %9s %9s %9s %9s %10s %10s %10s\n", "case", "sprintf", "fmt span", "va today", "va string", "va static",
                "va small", "span/old", "string/old", "small/old");
    char buf[1024];
    for (const Case& c : kCases) {
        double best[6] = {1e300, 1e300, 1e300, 1e300, 1e300, 1e300};
        for (int r = 0; r < runs; ++r) {
            const double t[6] = {
                time_one(iters,
                         [&](int i) {
                             c.oldSprintf(in, i, buf, sizeof(buf));
                             return static_cast<uint32_t>(buf[0]);
                         }),
                time_one(iters,
                         [&](int i) {
                             c.newSprintf(in, i, std::span<char>(buf));
                             return static_cast<uint32_t>(buf[0]);
                         }),
                time_one(iters, [&](int i) { return static_cast<uint32_t>(c.oldVa(in, i)[0]); }),
                time_one(iters, [&](int i) { return static_cast<uint32_t>(c.newVaString(in, i).size()); }),
                time_one(iters, [&](int i) { return static_cast<uint32_t>(c.newVaStatic(in, i)[0]); }),
                time_one(iters, [&](int i) { return static_cast<uint32_t>(c.newVaSmall(in, i).c_str()[0]); }),
            };
            for (int k = 0; k < 6; ++k)
                best[k] = t[k] < best[k] ? t[k] : best[k];
        }
        std::printf("%-36s %9.1f %9.1f %9.1f %9.1f %9.1f %9.1f %10.3f %10.3f %10.3f\n", c.name, best[0], best[1], best[2], best[3], best[4], best[5],
                    best[1] / best[0], best[3] / best[2], best[5] / best[2]);
    }
    return caseFailures + floatFailures ? 1 : 0;
}
