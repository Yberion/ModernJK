# Copies the EAX zone files (eagle/**/*.eal) out of a game's archives (base/assets*.pk3) into a folder, for
# eal_converter: the archives taken in the order the engine reads them, so that a later one's file replaces an earlier
# one's, as in the game. The maps whose names start with one of the prefixes given are left out (Jedi Outcast's
# multiplayer maps, which the project does not build: ctf_ duel_ ffa_).
# usage: python -I tools/eal_converter/extract.py <game>/base <folder> [prefix...]
import os
import sys
import zipfile


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: extract.py <game>/base <folder> [prefix of the maps to leave out...]")
    base, out, prefixes = sys.argv[1], sys.argv[2], tuple(p.lower() for p in sys.argv[3:])
    archives = sorted(f for f in os.listdir(base) if f.lower().startswith("assets") and f.lower().endswith(".pk3"))
    files = {}
    for archive in archives:
        with zipfile.ZipFile(os.path.join(base, archive)) as z:
            for name in z.namelist():
                low = name.lower()
                if not (low.startswith("eagle/") and low.endswith(".eal")):
                    continue
                if prefixes and os.path.basename(low).startswith(prefixes):
                    continue
                files[low] = (archive, z.read(name))
    for low, (archive, data) in sorted(files.items()):
        path = os.path.join(out, *low.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)
        print(f"{archive}: {low}")
    print(f"{len(files)} files")


main()
