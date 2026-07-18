#pragma once
#include <MD_MAX72xx.h>

// Fonts for the clock face, based on the Trip5 "Matrix Light" BDF fonts in
// fonts/ (CC-BY, github.com/trip5/Matrix-Fonts):
//   - smallFontDay:    weekday label (day) — see the width note below
//   - timeFontNarrow:  MatrixLight8X, straight conversion, used for the time
//
// Both use the version-1 header ('F', 1, firstASCII, lastASCII, height): a
// width byte then that many column bytes per character. Bit 0 is the top
// row. Glyphs are bottom-aligned to a shared baseline at the last row, so
// the (shorter) day letters sit on the same bottom line as the time:
//   - day letters are 6px tall (4px for the x-height 'o'), rows 2..7
//   - time digits (MatrixLight8X) are 8px tall, occupying rows 0..7
// Only the glyphs used by the weekday abbreviations (SUN MON TUE WED THU FRI
// SAT) and the clock digits (0-9, colon) are defined.
//
// The day label is constrained to a uniform 3px letter width so it lines up
// under the overline drawn by displayShowDayTime(). MatrixLight6X's own
// glyphs are proportional, so the day set is a mix per letter:
//   - MatrixLight6X uppercase, already 3px: A D E F R S T U
//   - MatrixLight6X lowercase (uppercase was >3px, lowercase is 3px):
//       H -> h (full 6px height), O -> o (x-height, 4px, sits lower)
//   - hand-redrawn 3px, uppercase height (uppercase and lowercase were both
//     wider than 3px, or too thin): I M N W
// So e.g. MON renders as "Mon" and THU as "Thu". The time font is unmodified.

// Weekday letters (A..W, others width-0); see width note above.
static const MD_MAX72XX::fontType_t smallFontDay[] PROGMEM = {
    'F', 1, 'A', 'W', 8,
    3, 248, 20, 248,          // A
    0,                        // B
    0,                        // C
    3, 252, 132, 120,         // D
    3, 252, 148, 148,         // E
    3, 252, 20, 20,           // F
    0,                        // G
    3, 252, 16, 224,          // H (lowercase h form, 3px)
    3, 132, 252, 132,         // I (redrawn 3px, serifed)
    0,                        // J
    0,                        // K
    0,                        // L
    3, 252, 8, 252,           // M (redrawn 3px)
    3, 252, 4, 248,           // N (redrawn 3px)
    3, 96, 144, 96,           // O (lowercase o form, 3px)
    0,                        // P
    0,                        // Q
    3, 252, 20, 232,          // R
    3, 136, 148, 100,         // S
    3, 4, 252, 4,             // T
    3, 124, 128, 124,         // U
    0,                        // V
    3, 252, 64, 252,          // W (redrawn 3px)
};

// MatrixLight8X — clock digits and colon ('0'..':').
static const MD_MAX72XX::fontType_t timeFontNarrow[] PROGMEM = {
    'F', 1, '0', ':', 8,
    3, 126, 129, 126,         // 0
    2, 2, 255,                // 1
    3, 242, 137, 134,         // 2
    3, 66, 137, 118,          // 3
    3, 12, 10, 255,           // 4
    3, 143, 137, 113,         // 5
    3, 126, 137, 114,         // 6
    3, 1, 241, 15,            // 7
    3, 118, 137, 118,         // 8
    3, 70, 137, 126,          // 9
    1, 20,                    // :
};
