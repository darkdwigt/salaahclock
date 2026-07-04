#include "display.h"
#include "config.h"
#include <MD_MAX72xx.h>
#include <MD_Parola.h>

static MD_Parola P(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void displayInit() {
    P.begin();
    P.setIntensity(DISPLAY_INTENSITY);
    P.displayClear();
}

void displayShowStatic(const String &text) {
    P.displayClear();
    P.setTextAlignment(PA_CENTER);
    P.print(text);
}

void displayShowScrolling(const String &text) {
    P.displayClear();
    P.displayText(text.c_str(), PA_LEFT, 40, 0, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
}

bool displayAnimateScroll() {
    return P.displayAnimate();
}
