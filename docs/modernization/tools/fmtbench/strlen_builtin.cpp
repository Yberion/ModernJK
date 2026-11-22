// Does the compiler accept __builtin_strlen, at run time and in a constant expression? {fmt} takes the length of a C
// string with it when its detection finds it (__has_builtin, or GCC and clang named), and otherwise with a loop that
// counts one character at a time; MSVC defines no __has_builtin, so it gets the loop, though its own standard library
// calls __builtin_strlen in char_traits<char>::length. Plan step 22's check before proposing a fix of the detection to
// {fmt}'s authors. Nothing of the project or of {fmt}: written in C++14, so that every /std mode of MSVC and every
// compiler of Compiler Explorer can compile it.
//
// One case per build, chosen with -DCASE=<n> (/DCASE=<n>), compiled only (-c, /c); a case that compiles passes:
//   1  at run time, on a pointer the compiler cannot see
//   2  in a constant expression, on a string literal
//   3  in a constant expression, on the array of a constexpr object ({fmt}'s issue 4423: GCC refuses it)
//   4  in a constexpr function called in a constant expression (the shape of {fmt}'s basic_string_view constructor)
#include <stddef.h>

#ifndef CASE
#error "define CASE as 1, 2, 3 or 4"
#endif

#if CASE == 1
size_t length_at_run_time(const char* s)
{
    return __builtin_strlen(s);
}
#elif CASE == 2
constexpr size_t literal_length = __builtin_strlen("x={}");
static_assert(literal_length == 4, "the length of the literal");
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
static_assert(__builtin_strlen(format_text.data) == 4, "the length of the object's array");
#elif CASE == 4
constexpr size_t length_in_function(const char* s)
{
    return __builtin_strlen(s);
}
static_assert(length_in_function("abcdef") == 6, "the length through a constexpr function");
#else
#error "CASE must be 1, 2, 3 or 4"
#endif
