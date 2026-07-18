#include "display.h"
#include "config.h"
#include "scroll_font.h"
#include "small_font.h"
#include <MD_MAX72xx.h>
#include <MD_Parola.h>

static MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Width in columns of `s` in the currently-selected font, including the
// 1-column gaps between characters.
static int textWidth(MD_MAX72XX *mx, const char *s) {
    uint8_t buf[8];
    int total = 0, n = 0;
    for (const char *p = s; *p; ++p) {
        total += mx->getChar((uint8_t)*p, sizeof(buf), buf);
        n++;
    }
    return total + (n > 0 ? n - 1 : 0);
}

// Draw `s` in the current font with its leftmost column at `startCol`
// (column indices decrease left-to-right). Returns the start column for
// whatever comes next (one blank column of gap already applied).
static int drawText(MD_MAX72XX *mx, int startCol, const char *s) {
    int col = startCol;
    for (const char *p = s; *p; ++p) {
        uint8_t w = mx->setChar(col, (uint8_t)*p);
        if (w) col -= (w + 1);
    }
    return col;
}

// The word "loading" in MatrixLight6 lowercase (rows 0..5), for the boot
// splash. Only the letters l o a d i n g are defined (codes 'a'..'o').
static const MD_MAX72XX::fontType_t loadingFont[] PROGMEM = {
    'F', 1, 97, 111, 8,
    3, 8, 20, 28,   // a
    0,              // b
    0,              // c
    3, 8, 20, 31,   // d
    0,              // e
    0,              // f
    3, 4, 42, 30,   // g
    0,              // h
    1, 29,          // i
    0,              // j
    0,              // k
    1, 31,          // l
    0,              // m
    3, 30, 2, 28,   // n
    3, 12, 18, 12,  // o
};

void displayInit() {
    P.begin();
    P.setIntensity(DISPLAY_INTENSITY);
    P.displayClear();
}

// Boot splash: the word "loading" (rows 0..5) with a progress bar on rows
// 6..7 filled to `percent` (0..100). The bar has a full-width 1px track
// (bottom row) and the fill grows from the left along the row above it.
void displayLoading(uint8_t percent) {
    MD_MAX72XX *mx = P.getGraphicObject();
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();

    const int cols = mx->getColumnCount();
    const int maxCol = cols - 1;

    const int wordW = 23; // width of "loading" in loadingFont
    mx->setFont((MD_MAX72XX::fontType_t *)loadingFont);
    drawText(mx, maxCol - (cols - wordW) / 2, "loading");
    mx->setFont(nullptr);

    if (percent > 100) percent = 100;
    int fill = ((int)percent * cols + 50) / 100; // columns filled (rounded)
    for (int c = 0; c <= maxCol; c++) mx->setPoint(7, c, true); // track
    for (int i = 0; i < fill; i++) mx->setPoint(6, maxCol - i, true); // fill

    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void displayShowStatic(const String &text) {
    P.displayClear();
    P.setTextAlignment(PA_CENTER);
    P.print(text);
}

void displayShowDayTime(const String &day, const String &time) {
    MD_MAX72XX *mx = P.getGraphicObject();
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
    mx->clear();

    const int maxCol = mx->getColumnCount() - 1;

    // Measure both parts. Aim for a 3-column gap between the day and the
    // time (as in the reference frame), but shrink it as needed when a wide
    // day (e.g. MON) meets a wide time so nothing clips off the right edge.
    mx->setFont((MD_MAX72XX::fontType_t *)smallFontDay);
    int wDay = textWidth(mx, day.c_str());
    mx->setFont((MD_MAX72XX::fontType_t *)timeFontNarrow);
    int wTime = textWidth(mx, time.c_str());
    int gap = (maxCol + 1) - wDay - wTime; // free columns after both parts
    if (gap > 3) gap = 3;
    if (gap < 0) gap = 0;

    // Centre the whole "DAY  HH:MM" block: split the leftover columns evenly
    // so there's an equal margin on each side (odd leftovers put the extra
    // column on the right). The leftmost column is the highest index, since
    // setChar draws right-to-left.
    int leftMargin = ((maxCol + 1) - wDay - gap - wTime) / 2;
    if (leftMargin < 0) leftMargin = 0;
    int startCol = maxCol - leftMargin;

    int col = startCol;
    mx->setFont((MD_MAX72XX::fontType_t *)smallFontDay);
    col = drawText(mx, col, day.c_str());

    // Horizontal line across the top of the day label: a solid overline on
    // the top row spanning the day's full width (including the gaps between
    // letters). The day letters sit in rows 2..7, leaving row 0 for this.
    for (int c = startCol - wDay + 1; c <= startCol; c++) {
        mx->setPoint(0, c, true);
    }

    col -= (gap - 1); // drawText already left one gap column
    mx->setFont((MD_MAX72XX::fontType_t *)timeFontNarrow);
    drawText(mx, col, time.c_str());

    mx->setFont(nullptr); // restore default for the scrolling modes
    mx->control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}

void displayShowScrolling(const String &text) {
    // Scrolling messages (prayer countdown, weather) use the full
    // MatrixLight8X font. Re-applied on every call because the clock face
    // swaps the underlying font out via the low-level graphics object.
    P.setFont((MD_MAX72XX::fontType_t *)scrollFont8X);
    P.displayClear();
    P.displayText(text.c_str(), PA_LEFT, 40, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

bool displayAnimateScroll() {
    return P.displayAnimate();
}
