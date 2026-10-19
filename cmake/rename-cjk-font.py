"""Normalize the instantiated Noto Sans JP static font's names.

fontTools' instancer keeps the variable font's family names ("Noto
Sans JP Thin") while flipping the subfamily to Regular, leaving a
contradictory "Noto Sans JP / Thin" pair that some matchers prefer
over real installed families. Rewrite the family as plain
"Noto Sans JP" + "Regular" (RIBBI and typographic entries alike).

Usage: rename-cjk-font.py <ttf-path>  (edits in place)
"""
import sys

from fontTools.ttLib import TTFont

NAMES = {
    1: "Noto Sans JP",  # Typographic Family
    2: "Regular",  # Typographic Subfamily
    4: "Noto Sans JP Regular",  # Full name
    6: "NotoSansJP-Regular",  # PostScript (ASCII, no spaces)
    16: "Noto Sans JP",  # Preferred Family
    17: "Regular",  # Preferred Subfamily
}


def main() -> None:
    path = sys.argv[1]
    font = TTFont(path)
    table = font["name"]
    for name_id, text in NAMES.items():
        table.setName(text, name_id, 3, 1, 0x409)
        table.setName(text, name_id, 1, 0, 0)
    font.save(path)


if __name__ == "__main__":
    main()
