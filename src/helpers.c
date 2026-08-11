#include <stdio.h>
#include <stdlib.h>
#include "bane.h"

void ensure_fail(const char *file, int line, const char *func, const char *expr) {
    fprintf(stderr, "%s:%i %s ENSURE (%s) failed.\n", file, line, func, expr);
    abort();
}

void rect_fit_box(Rectangle box, Rectangle *fit, Rectangle *linked_fit) {
    // transform fit to fit box.
    // if linked_fit is not NULL, transformations applied to fit also apply to linked_fit, but linked_fit isn't evaluated to fit box
    Rectangle temp = {0}; // store transformations in temp. apply to fit and to linked_fit if applicable
    if (fit->x < box.x) { temp.width += fit->x - box.x; temp.x += box.x - fit->x; }
    if (fit->y < box.y) { temp.height += fit->y - box.y; temp.y += box.y - fit->y; }
    if (fit->x + fit->width > box.x + box.width) temp.width += box.x + box.width - (fit->x + fit->width);
    if (fit->y + fit->height > box.y + box.height) temp.height += box.y + box.height - (fit->y + fit->height);

    fit->x += temp.x; fit->y += temp.y; fit->width += temp.width; fit->height += temp.height;
    if (linked_fit != NULL) { linked_fit->x += temp.x; linked_fit->y += temp.y; linked_fit->width += temp.width; linked_fit->height += temp.height; }
}