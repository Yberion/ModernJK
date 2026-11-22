"""The project's to_underlying replaced by C++23's std::to_underlying: every call (commented-out ones too) written
std::to_underlying, the copy in q_enum.h removed and <utility> included there. usage: python -I stdunderlying.py <repo>"""
import re
import subprocess
import sys
import time

root = sys.argv[1]
CALL = re.compile(r"(?<![\w:])to_underlying\(")
files = subprocess.run(["git", "-C", root, "grep", "-l", "-E", r"\bto_underlying\(", "--", "src", "test"],
                       capture_output=True, text=True).stdout.split()


def write(path, text):
    for attempt in range(5):
        try:
            open(path, "w", encoding="latin-1", newline="").write(text)
            return
        except OSError:
            time.sleep(0.5)
    raise RuntimeError(path)


qenum = f"{root}/src/shared/qcommon/q_enum.h"
text = open(qenum, encoding="latin-1", newline="").read()
e = "\r\n" if "\r\n" in text else "\n"
definition = ("// C++23's std::to_underlying, until the project moves to C++23" + e + "template <typename E>" + e
              + "    requires std::is_enum_v<E>" + e
              + "[[nodiscard]] constexpr std::underlying_type_t<E> to_underlying(E e) noexcept" + e + "{" + e
              + "    return static_cast<std::underlying_type_t<E>>(e);" + e + "}" + e + e)
assert text.count(definition) == 1
text = text.replace(definition, "")
assert text.count("#include <type_traits>" + e) == 1
text = text.replace("#include <type_traits>" + e, "#include <type_traits>" + e + "#include <utility>" + e)
write(qenum, text)

total = 0
for path in files:
    full = f"{root}/{path}"
    text = open(full, encoding="latin-1", newline="").read()
    new, n = CALL.subn("std::to_underlying(", text)
    if n:
        total += n
        write(full, new)
print("calls:", total)
