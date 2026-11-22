// The code {fmt} adds to a binary: the same three texts written into a buffer by vsnprintf (SIZE_PRINTF) or by
// {fmt} through a function taking packed arguments (SIZE_FMT), as the project's header does; the two executables'
// sizes, stripped, compared by run.sh and run.bat.
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <span>

#ifdef SIZE_FMT
#include <fmt/format.h>

namespace {
size_t vwrite(std::span<char> dest, fmt::string_view format, fmt::format_args args)
{
    const auto result = fmt::vformat_to_n(dest.data(), dest.size() - 1, format, args);
    *result.out = '\0';
    return result.size;
}
template <typename... T> size_t write(std::span<char> dest, fmt::format_string<T...> format, T&&... args)
{
    return vwrite(dest, format.get(), fmt::make_format_args(args...));
}
} // namespace

int main(int argc, char** argv)
{
    char buf[256];
    write(buf, "{} players\n", argc);
    std::fputs(buf, stdout);
    write(buf, "print \"{}\n\"", argv[0]);
    std::fputs(buf, stdout);
    write(buf, "{:.2f}\n", std::atof(argc > 1 ? argv[1] : "1.5"));
    std::fputs(buf, stdout);
    return 0;
}
#else
namespace {
int write(char* dest, int size, const char* format, ...)
{
    va_list ap;
    va_start(ap, format);
    const int len = std::vsnprintf(dest, static_cast<size_t>(size), format, ap);
    va_end(ap);
    return len;
}
} // namespace

int main(int argc, char** argv)
{
    char buf[256];
    write(buf, sizeof(buf), "%i players\n", argc);
    std::fputs(buf, stdout);
    write(buf, sizeof(buf), "print \"%s\n\"", argv[0]);
    std::fputs(buf, stdout);
    write(buf, sizeof(buf), "%.2f\n", std::atof(argc > 1 ? argv[1] : "1.5"));
    std::fputs(buf, stdout);
    return 0;
}
#endif
