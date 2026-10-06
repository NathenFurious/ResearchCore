#include "css.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSS_MAX_TOKEN 256
#define CSS_MAX_BACKGROUND_LAYERS 16

typedef enum {
    CSS_UNIT_NONE,
    CSS_UNIT_PX,
    CSS_UNIT_PERCENT,
    CSS_UNIT_EM,
    CSS_UNIT_REM,
    CSS_UNIT_VW,
    CSS_UNIT_VH,
    CSS_UNIT_VMIN,
    CSS_UNIT_VMAX
} CSSUnit;

typedef struct {
    float value;
    CSSUnit unit;
} CSSLength;

typedef struct {
    float r;
    float g;
    float b;
    float a;
} CSSColor;

typedef enum {
    CSS_DISPLAY_INLINE,
    CSS_DISPLAY_BLOCK,
    CSS_DISPLAY_INLINE_BLOCK,
    CSS_DISPLAY_NONE,
    CSS_DISPLAY_FLEX,
    CSS_DISPLAY_INLINE_FLEX,
    CSS_DISPLAY_GRID,
    CSS_DISPLAY_INLINE_GRID
} CSSDisplay;

typedef enum {
    CSS_POSITION_STATIC,
    CSS_POSITION_RELATIVE,
    CSS_POSITION_ABSOLUTE,
    CSS_POSITION_FIXED,
    CSS_POSITION_STICKY
} CSSPosition;

typedef enum {
    CSS_OVERFLOW_VISIBLE,
    CSS_OVERFLOW_HIDDEN,
    CSS_OVERFLOW_CLIP,
    CSS_OVERFLOW_SCROLL,
    CSS_OVERFLOW_AUTO
} CSSOverflow;

typedef enum {
    CSS_BOX_CONTENT_BOX,
    CSS_BOX_BORDER_BOX
} CSSBoxSizing;

typedef enum {
    CSS_BORDER_NONE,
    CSS_BORDER_HIDDEN,
    CSS_BORDER_DOTTED,
    CSS_BORDER_DASHED,
    CSS_BORDER_SOLID,
    CSS_BORDER_DOUBLE,
    CSS_BORDER_GROOVE,
    CSS_BORDER_RIDGE,
    CSS_BORDER_INSET,
    CSS_BORDER_OUTSET
} CSSBorderStyle;

typedef enum {
    CSS_BG_REPEAT,
    CSS_BG_NO_REPEAT,
    CSS_BG_REPEAT_X,
    CSS_BG_REPEAT_Y,
    CSS_BG_SPACE,
    CSS_BG_ROUND
} CSSBackgroundRepeat;

typedef enum {
    CSS_BG_SIZE_AUTO,
    CSS_BG_SIZE_COVER,
    CSS_BG_SIZE_CONTAIN,
    CSS_BG_SIZE_EXPLICIT
} CSSBackgroundSizeType;

typedef struct {
    CSSLength width;
    CSSLength height;
    CSSBackgroundSizeType type;
} CSSBackgroundSize;

typedef struct {
    CSSColor color;

    char image[CSS_MAX_TOKEN];

    CSSBackgroundRepeat repeat_x;
    CSSBackgroundRepeat repeat_y;

    CSSLength position_x;
    CSSLength position_y;

    CSSBackgroundSize size;
} CSSBackgroundLayer;

typedef struct {
    CSSLength width;
    CSSBorderStyle style;
    CSSColor color;
} CSSBorder;

typedef struct {
    CSSLength top;
    CSSLength right;
    CSSLength bottom;
    CSSLength left;
} CSSBoxValues;

typedef struct {
    CSSDisplay display;
    CSSPosition position;

    CSSOverflow overflow_x;
    CSSOverflow overflow_y;

    CSSBoxSizing box_sizing;

    CSSLength width;
    CSSLength height;

    CSSLength min_width;
    CSSLength min_height;

    CSSLength max_width;
    CSSLength max_height;

    CSSBoxValues margin;
    CSSBoxValues padding;

    CSSLength top;
    CSSLength right;
    CSSLength bottom;
    CSSLength left;

    int z_index;

    float opacity;

    int visibility;

    CSSColor color;

    CSSBorder border_top;
    CSSBorder border_right;
    CSSBorder border_bottom;
    CSSBorder border_left;

    CSSLength radius_top_left;
    CSSLength radius_top_right;
    CSSLength radius_bottom_right;
    CSSLength radius_bottom_left;

    CSSColor background_color;

    CSSBackgroundLayer backgrounds[CSS_MAX_BACKGROUND_LAYERS];
    size_t background_count;
} CSSComputedStyle;

static int css_ascii_case_equal(
    const char* a,
    const char* b
) {
    if (!a || !b) {
        return 0;
    }

    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = (unsigned char)(ca + 32);
        }

        if (cb >= 'A' && cb <= 'Z') {
            cb = (unsigned char)(cb + 32);
        }

        if (ca != cb) {
            return 0;
        }

        a++;
        b++;
    }

    return *a == '\0' && *b == '\0';
}

static int css_starts_with_ci(
    const char* value,
    const char* prefix
) {
    while (*prefix) {
        unsigned char a = (unsigned char)*value++;
        unsigned char b = (unsigned char)*prefix++;

        if (a >= 'A' && a <= 'Z') {
            a = (unsigned char)(a + 32);
        }

        if (b >= 'A' && b <= 'Z') {
            b = (unsigned char)(b + 32);
        }

        if (a != b) {
            return 0;
        }
    }

    return 1;
}

static void css_trim(
    const char** start,
    const char** end
) {
    while (*start < *end &&
           isspace((unsigned char)**start)) {
        (*start)++;
    }

    while (*end > *start &&
           isspace((unsigned char)(*end)[-1])) {
        (*end)--;
    }
}

static int css_parse_float(
    const char* text,
    float* result
) {
    char* end;
    double value;

    if (!text || !result) {
        return 0;
    }

    while (isspace((unsigned char)*text)) {
        text++;
    }

    value = strtod(text, &end);

    if (end == text) {
        return 0;
    }

    while (isspace((unsigned char)*end)) {
        end++;
    }

    if (*end != '\0') {
        return 0;
    }

    *result = (float)value;
    return 1;
}

static CSSLength css_length_auto(void) {
    CSSLength value;

    value.value = 0.0f;
    value.unit = CSS_UNIT_NONE;

    return value;
}

static CSSLength css_length_px(float value) {
    CSSLength result;

    result.value = value;
    result.unit = CSS_UNIT_PX;

    return result;
}

static int css_parse_length(
    const char* text,
    CSSLength* result
) {
    char* end;
    double value;

    if (!text || !result) {
        return 0;
    }

    while (isspace((unsigned char)*text)) {
        text++;
    }

    if (css_ascii_case_equal(text, "auto")) {
        *result = css_length_auto();
        return 1;
    }

    if (css_ascii_case_equal(text, "0")) {
        *result = css_length_px(0.0f);
        return 1;
    }

    value = strtod(text, &end);

    if (end == text) {
        return 0;
    }

    if (css_ascii_case_equal(end, "px")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_PX;
        return 1;
    }

    if (*end == '%') {
        result->value = (float)value;
        result->unit = CSS_UNIT_PERCENT;
        return 1;
    }

    if (css_ascii_case_equal(end, "em")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_EM;
        return 1;
    }

    if (css_ascii_case_equal(end, "rem")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_REM;
        return 1;
    }

    if (css_ascii_case_equal(end, "vw")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_VW;
        return 1;
    }

    if (css_ascii_case_equal(end, "vh")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_VH;
        return 1;
    }

    if (css_ascii_case_equal(end, "vmin")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_VMIN;
        return 1;
    }

    if (css_ascii_case_equal(end, "vmax")) {
        result->value = (float)value;
        result->unit = CSS_UNIT_VMAX;
        return 1;
    }

    return 0;
}

static CSSColor css_color(
    float r,
    float g,
    float b,
    float a
) {
    CSSColor result;

    result.r = r;
    result.g = g;
    result.b = b;
    result.a = a;

    return result;
}

static CSSColor css_color_black(void) {
    return css_color(0.0f, 0.0f, 0.0f, 1.0f);
}

static CSSColor css_color_transparent(void) {
    return css_color(0.0f, 0.0f, 0.0f, 0.0f);
}

static int css_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }

    return -1;
}

static int css_parse_hex_color(
    const char* text,
    CSSColor* result
) {
    size_t length;
    int r;
    int g;
    int b;
    int a = 255;

    if (!text || text[0] != '#') {
        return 0;
    }

    length = strlen(text + 1);

    if (length != 3 &&
        length != 4 &&
        length != 6 &&
        length != 8) {
        return 0;
    }

    if (length == 3 || length == 4) {
        r = css_hex_digit(text[1]);
        g = css_hex_digit(text[2]);
        b = css_hex_digit(text[3]);

        if (r < 0 || g < 0 || b < 0) {
            return 0;
        }

        r = r * 17;
        g = g * 17;
        b = b * 17;

        if (length == 4) {
            a = css_hex_digit(text[4]);

            if (a < 0) {
                return 0;
            }

            a *= 17;
        }
    } else {
        r = css_hex_digit(text[1]) * 16 +
            css_hex_digit(text[2]);

        g = css_hex_digit(text[3]) * 16 +
            css_hex_digit(text[4]);

        b = css_hex_digit(text[5]) * 16 +
            css_hex_digit(text[6]);

        if (r < 0 || g < 0 || b < 0) {
            return 0;
        }

        if (length == 8) {
            a = css_hex_digit(text[7]) * 16 +
                css_hex_digit(text[8]);

            if (a < 0) {
                return 0;
            }
        }
    }

    *result = css_color(
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        a / 255.0f
    );

    return 1;
}

static int css_parse_rgb_component(
    const char* text,
    float* result
) {
    float value;

    if (!css_parse_float(text, &value)) {
        return 0;
    }

    if (strchr(text, '%')) {
        *result = value / 100.0f;
    } else {
        *result = value / 255.0f;
    }

    if (*result < 0.0f) {
        *result = 0.0f;
    }

    if (*result > 1.0f) {
        *result = 1.0f;
    }

    return 1;
}

static int css_parse_rgb_function(
    const char* text,
    CSSColor* result
) {
    char buffer[512];
    char* open;
    char* close;
    char* parts[4];
    size_t count = 0;
    char* token;

    if (!css_starts_with_ci(text, "rgb(") &&
        !css_starts_with_ci(text, "rgba(")) {
        return 0;
    }

    open = strchr(text, '(');
    close = strrchr(text, ')');

    if (!open || !close || close <= open) {
        return 0;
    }

    size_t length = (size_t)(close - open - 1);

    if (length >= sizeof(buffer)) {
        return 0;
    }

    memcpy(buffer, open + 1, length);
    buffer[length] = '\0';

    token = strtok(buffer, ", /");

    while (token && count < 4) {
        parts[count++] = token;
        token = strtok(NULL, ", /");
    }

    if (count < 3) {
        return 0;
    }

    float r;
    float g;
    float b;
    float a = 1.0f;

    if (!css_parse_rgb_component(parts[0], &r) ||
        !css_parse_rgb_component(parts[1], &g) ||
        !css_parse_rgb_component(parts[2], &b)) {
        return 0;
    }

    if (count >= 4) {
        if (strchr(parts[3], '%')) {
            float alpha;

            if (!css_parse_float(parts[3], &alpha)) {
                return 0;
            }

            a = alpha / 100.0f;
        } else {
            if (!css_parse_float(parts[3], &a)) {
                return 0;
            }
        }

        if (a < 0.0f) {
            a = 0.0f;
        }

        if (a > 1.0f) {
            a = 1.0f;
        }
    }

    *result = css_color(r, g, b, a);
    return 1;
}

typedef struct {
    const char* name;
    CSSColor color;
} CSSNamedColor;

static const CSSNamedColor css_named_colors[] = {
    {"black",       {0, 0, 0, 1}},
    {"silver",      {0.752941f, 0.752941f, 0.752941f, 1}},
    {"gray",        {0.501961f, 0.501961f, 0.501961f, 1}},
    {"grey",        {0.501961f, 0.501961f, 0.501961f, 1}},
    {"white",       {1, 1, 1, 1}},
    {"maroon",      {0.501961f, 0, 0, 1}},
    {"red",         {1, 0, 0, 1}},
    {"purple",      {0.501961f, 0, 0.501961f, 1}},
    {"fuchsia",     {1, 0, 1, 1}},
    {"magenta",     {1, 0, 1, 1}},
    {"green",       {0, 0.501961f, 0, 1}},
    {"lime",        {0, 1, 0, 1}},
    {"olive",       {0.501961f, 0.501961f, 0, 1}},
    {"yellow",      {1, 1, 0, 1}},
    {"navy",        {0, 0, 0.501961f, 1}},
    {"blue",        {0, 0, 1, 1}},
    {"teal",        {0, 0.501961f, 0.501961f, 1}},
    {"aqua",        {0, 1, 1, 1}},
    {"cyan",        {0, 1, 1, 1}},
    {"orange",      {1, 0.647059f, 0, 1}},
    {"pink",        {1, 0.752941f, 0.796078f, 1}},
    {"brown",       {0.647059f, 0.164706f, 0.164706f, 1}},
    {"transparent", {0, 0, 0, 0}},
    {"currentcolor", {0, 0, 0, 1}}
};

static int css_parse_color(
    const char* text,
    CSSColor* result
) {
    size_t i;

    if (!text || !result) {
        return 0;
    }

    while (isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '#') {
        return css_parse_hex_color(text, result);
    }

    if (css_starts_with_ci(text, "rgb(") ||
        css_starts_with_ci(text, "rgba(")) {
        return css_parse_rgb_function(text, result);
    }

    for (i = 0;
         i < sizeof(css_named_colors) /
             sizeof(css_named_colors[0]);
         i++) {

        if (css_ascii_case_equal(
                text,
                css_named_colors[i].name)) {
            *result = css_named_colors[i].color;
            return 1;
        }
    }

    return 0;
}

static int css_parse_display(
    const char* value,
    CSSDisplay* result
) {
    if (css_ascii_case_equal(value, "inline")) {
        *result = CSS_DISPLAY_INLINE;
        return 1;
    }

    if (css_ascii_case_equal(value, "block")) {
        *result = CSS_DISPLAY_BLOCK;
        return 1;
    }

    if (css_ascii_case_equal(value, "inline-block")) {
        *result = CSS_DISPLAY_INLINE_BLOCK;
        return 1;
    }

    if (css_ascii_case_equal(value, "none")) {
        *result = CSS_DISPLAY_NONE;
        return 1;
    }

    if (css_ascii_case_equal(value, "flex")) {
        *result = CSS_DISPLAY_FLEX;
        return 1;
    }

    if (css_ascii_case_equal(value, "inline-flex")) {
        *result = CSS_DISPLAY_INLINE_FLEX;
        return 1;
    }

    if (css_ascii_case_equal(value, "grid")) {
        *result = CSS_DISPLAY_GRID;
        return 1;
    }

    if (css_ascii_case_equal(value, "inline-grid")) {
        *result = CSS_DISPLAY_INLINE_GRID;
        return 1;
    }

    return 0;
}

static int css_parse_position(
    const char* value,
    CSSPosition* result
) {
    if (css_ascii_case_equal(value, "static")) {
        *result = CSS_POSITION_STATIC;
        return 1;
    }

    if (css_ascii_case_equal(value, "relative")) {
        *result = CSS_POSITION_RELATIVE;
        return 1;
    }

    if (css_ascii_case_equal(value, "absolute")) {
        *result = CSS_POSITION_ABSOLUTE;
        return 1;
    }

    if (css_ascii_case_equal(value, "fixed")) {
        *result = CSS_POSITION_FIXED;
        return 1;
    }

    if (css_ascii_case_equal(value, "sticky")) {
        *result = CSS_POSITION_STICKY;
        return 1;
    }

    return 0;
}

static int css_parse_overflow(
    const char* value,
    CSSOverflow* result
) {
    if (css_ascii_case_equal(value, "visible")) {
        *result = CSS_OVERFLOW_VISIBLE;
        return 1;
    }

    if (css_ascii_case_equal(value, "hidden")) {
        *result = CSS_OVERFLOW_HIDDEN;
        return 1;
    }

    if (css_ascii_case_equal(value, "clip")) {
        *result = CSS_OVERFLOW_CLIP;
        return 1;
    }

    if (css_ascii_case_equal(value, "scroll")) {
        *result = CSS_OVERFLOW_SCROLL;
        return 1;
    }

    if (css_ascii_case_equal(value, "auto")) {
        *result = CSS_OVERFLOW_AUTO;
        return 1;
    }

    return 0;
}

static int css_parse_border_style(
    const char* value,
    CSSBorderStyle* result
) {
    if (css_ascii_case_equal(value, "none")) {
        *result = CSS_BORDER_NONE;
        return 1;
    }

    if (css_ascii_case_equal(value, "hidden")) {
        *result = CSS_BORDER_HIDDEN;
        return 1;
    }

    if (css_ascii_case_equal(value, "dotted")) {
        *result = CSS_BORDER_DOTTED;
        return 1;
    }

    if (css_ascii_case_equal(value, "dashed")) {
        *result = CSS_BORDER_DASHED;
        return 1;
    }

    if (css_ascii_case_equal(value, "solid")) {
        *result = CSS_BORDER_SOLID;
        return 1;
    }

    if (css_ascii_case_equal(value, "double")) {
        *result = CSS_BORDER_DOUBLE;
        return 1;
    }

    if (css_ascii_case_equal(value, "groove")) {
        *result = CSS_BORDER_GROOVE;
        return 1;
    }

    if (css_ascii_case_equal(value, "ridge")) {
        *result = CSS_BORDER_RIDGE;
        return 1;
    }

    if (css_ascii_case_equal(value, "inset")) {
        *result = CSS_BORDER_INSET;
        return 1;
    }

    if (css_ascii_case_equal(value, "outset")) {
        *result = CSS_BORDER_OUTSET;
        return 1;
    }

    return 0;
}

static char* css_next_word(
    char** cursor
) {
    char* start;

    while (**cursor &&
           isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }

    if (!**cursor) {
        return NULL;
    }

    start = *cursor;

    while (**cursor &&
           !isspace((unsigned char)**cursor)) {
        (*cursor)++;
    }

    if (**cursor) {
        **cursor = '\0';
        (*cursor)++;
    }

    return start;
}

static int css_parse_box_shorthand(
    const char* value,
    CSSBoxValues* box
) {
    char buffer[512];
    char* cursor;
    char* values[4];
    int count = 0;

    snprintf(buffer, sizeof(buffer), "%s", value);

    cursor = buffer;

    while (count < 4) {
        char* word = css_next_word(&cursor);

        if (!word) {
            break;
        }

        values[count++] = word;
    }

    if (count == 0) {
        return 0;
    }

    CSSLength parsed[4];

    for (int i = 0; i < count; i++) {
        if (!css_parse_length(values[i], &parsed[i])) {
            return 0;
        }
    }

    switch (count) {
        case 1:
            box->top =
            box->right =
            box->bottom =
            box->left = parsed[0];
            break;

        case 2:
            box->top =
            box->bottom = parsed[0];

            box->right =
            box->left = parsed[1];
            break;

        case 3:
            box->top = parsed[0];

            box->right =
            box->left = parsed[1];

            box->bottom = parsed[2];
            break;

        case 4:
            box->top = parsed[0];
            box->right = parsed[1];
            box->bottom = parsed[2];
            box->left = parsed[3];
            break;
    }

    return 1;
}

static void css_border_initial(
    CSSBorder* border
) {
    border->width = css_length_px(0.0f);
    border->style = CSS_BORDER_NONE;
    border->color = css_color_black();
}

static int css_parse_border_shorthand(
    const char* value,
    CSSBorder* border
) {
    char buffer[512];
    char* cursor;
    char* word;

    CSSLength width;
    CSSBorderStyle style;
    CSSColor color;

    int have_width = 0;
    int have_style = 0;
    int have_color = 0;

    snprintf(buffer, sizeof(buffer), "%s", value);

    cursor = buffer;

    while ((word = css_next_word(&cursor)) != NULL) {
        if (!have_width &&
            css_parse_length(word, &width)) {
            have_width = 1;
            continue;
        }

        if (!have_style &&
            css_parse_border_style(word, &style)) {
            have_style = 1;
            continue;
        }

        if (!have_color &&
            css_parse_color(word, &color)) {
            have_color = 1;
            continue;
        }

        return 0;
    }

    if (have_width) {
        border->width = width;
    }

    if (have_style) {
        border->style = style;
    }

    if (have_color) {
        border->color = color;
    }

    return have_width || have_style || have_color;
}

static void css_background_initial(
    CSSBackgroundLayer* background
) {
    background->color = css_color_transparent();
    background->image[0] = '\0';

    background->repeat_x = CSS_BG_REPEAT;
    background->repeat_y = CSS_BG_REPEAT;

    background->position_x = css_length_px(0.0f);
    background->position_y = css_length_px(0.0f);

    background->size.type = CSS_BG_SIZE_AUTO;
    background->size.width = css_length_auto();
    background->size.height = css_length_auto();
}

static int css_parse_background_repeat(
    const char* value,
    CSSBackgroundLayer* background
) {
    if (css_ascii_case_equal(value, "repeat")) {
        background->repeat_x = CSS_BG_REPEAT;
        background->repeat_y = CSS_BG_REPEAT;
        return 1;
    }

    if (css_ascii_case_equal(value, "no-repeat")) {
        background->repeat_x = CSS_BG_NO_REPEAT;
        background->repeat_y = CSS_BG_NO_REPEAT;
        return 1;
    }

    if (css_ascii_case_equal(value, "repeat-x")) {
        background->repeat_x = CSS_BG_REPEAT_X;
        background->repeat_y = CSS_BG_NO_REPEAT;
        return 1;
    }

    if (css_ascii_case_equal(value, "repeat-y")) {
        background->repeat_x = CSS_BG_NO_REPEAT;
        background->repeat_y = CSS_BG_REPEAT_Y;
        return 1;
    }

    if (css_ascii_case_equal(value, "space")) {
        background->repeat_x = CSS_BG_SPACE;
        background->repeat_y = CSS_BG_SPACE;
        return 1;
    }

    if (css_ascii_case_equal(value, "round")) {
        background->repeat_x = CSS_BG_ROUND;
        background->repeat_y = CSS_BG_ROUND;
        return 1;
    }

    return 0;
}

static int css_parse_background_size(
    const char* value,
    CSSBackgroundLayer* background
) {
    if (css_ascii_case_equal(value, "cover")) {
        background->size.type = CSS_BG_SIZE_COVER;
        return 1;
    }

    if (css_ascii_case_equal(value, "contain")) {
        background->size.type = CSS_BG_SIZE_CONTAIN;
        return 1;
    }

    CSSLength width;

    if (css_parse_length(value, &width)) {
        background->size.type = CSS_BG_SIZE_EXPLICIT;
        background->size.width = width;
        background->size.height = css_length_auto();
        return 1;
    }

    return 0;
}

static int css_parse_background_image(
    const char* value,
    CSSBackgroundLayer* background
) {
    const char* start;

    if (!value) {
        return 0;
    }

    start = value;

    while (isspace((unsigned char)*start)) {
        start++;
    }

    if (css_ascii_case_equal(start, "none")) {
        background->image[0] = '\0';
        return 1;
    }

    if (css_starts_with_ci(start, "url(")) {
        snprintf(
            background->image,
            sizeof(background->image),
            "%s",
            start
        );

        return 1;
    }

    if (css_starts_with_ci(start, "linear-gradient(") ||
        css_starts_with_ci(start, "radial-gradient(") ||
        css_starts_with_ci(start, "conic-gradient(") ||
        css_starts_with_ci(start, "repeating-linear-gradient(") ||
        css_starts_with_ci(start, "repeating-radial-gradient(") ||
        css_starts_with_ci(start, "repeating-conic-gradient(")) {

        snprintf(
            background->image,
            sizeof(background->image),
            "%s",
            start
        );

        return 1;
    }

    return 0;
}

static int css_parse_background_position(
    const char* value,
    CSSBackgroundLayer* background
) {
    char buffer[256];
    char* cursor;
    char* first;
    char* second;

    snprintf(buffer, sizeof(buffer), "%s", value);

    cursor = buffer;

    first = css_next_word(&cursor);
    second = css_next_word(&cursor);

    if (!first) {
        return 0;
    }

    if (css_ascii_case_equal(first, "left")) {
        background->position_x = css_length_px(0.0f);
    } else if (css_ascii_case_equal(first, "center")) {
        background->position_x.value = 50.0f;
        background->position_x.unit = CSS_UNIT_PERCENT;
    } else if (css_ascii_case_equal(first, "right")) {
        background->position_x.value = 100.0f;
        background->position_x.unit = CSS_UNIT_PERCENT;
    } else if (!css_parse_length(
                   first,
                   &background->position_x)) {
        return 0;
    }

    if (!second) {
        background->position_y.value = 50.0f;
        background->position_y.unit = CSS_UNIT_PERCENT;
        return 1;
    }

    if (css_ascii_case_equal(second, "top")) {
        background->position_y = css_length_px(0.0f);
    } else if (css_ascii_case_equal(second, "center")) {
        background->position_y.value = 50.0f;
        background->position_y.unit = CSS_UNIT_PERCENT;
    } else if (css_ascii_case_equal(second, "bottom")) {
        background->position_y.value = 100.0f;
        background->position_y.unit = CSS_UNIT_PERCENT;
    } else if (!css_parse_length(
                   second,
                   &background->position_y)) {
        return 0;
    }

    return 1;
}

static void css_apply_property(
    CSSComputedStyle* style,
    const char* property,
    const char* value
) {
    CSSLength length;

    if (!style || !property || !value) {
        return;
    }

    if (css_ascii_case_equal(property, "display")) {
        css_parse_display(value, &style->display);
        return;
    }

    if (css_ascii_case_equal(property, "position")) {
        css_parse_position(value, &style->position);
        return;
    }

    if (css_ascii_case_equal(property, "overflow")) {
        CSSOverflow overflow;

        if (css_parse_overflow(value, &overflow)) {
            style->overflow_x = overflow;
            style->overflow_y = overflow;
        }

        return;
    }

    if (css_ascii_case_equal(property, "overflow-x")) {
        css_parse_overflow(value, &style->overflow_x);
        return;
    }

    if (css_ascii_case_equal(property, "overflow-y")) {
        css_parse_overflow(value, &style->overflow_y);
        return;
    }

    if (css_ascii_case_equal(property, "box-sizing")) {
        if (css_ascii_case_equal(value, "border-box")) {
            style->box_sizing = CSS_BOX_BORDER_BOX;
        } else if (css_ascii_case_equal(value, "content-box")) {
            style->box_sizing = CSS_BOX_CONTENT_BOX;
        }

        return;
    }

    if (css_ascii_case_equal(property, "width")) {
        if (css_parse_length(value, &length)) {
            style->width = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "height")) {
        if (css_parse_length(value, &length)) {
            style->height = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "min-width")) {
        if (css_parse_length(value, &length)) {
            style->min_width = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "min-height")) {
        if (css_parse_length(value, &length)) {
            style->min_height = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "max-width")) {
        if (css_parse_length(value, &length)) {
            style->max_width = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "max-height")) {
        if (css_parse_length(value, &length)) {
            style->max_height = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "margin")) {
        css_parse_box_shorthand(value, &style->margin);
        return;
    }

    if (css_ascii_case_equal(property, "padding")) {
        css_parse_box_shorthand(value, &style->padding);
        return;
    }

    if (css_ascii_case_equal(property, "margin-top")) {
        if (css_parse_length(value, &length)) {
            style->margin.top = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "margin-right")) {
        if (css_parse_length(value, &length)) {
            style->margin.right = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "margin-bottom")) {
        if (css_parse_length(value, &length)) {
            style->margin.bottom = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "margin-left")) {
        if (css_parse_length(value, &length)) {
            style->margin.left = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "padding-top")) {
        if (css_parse_length(value, &length)) {
            style->padding.top = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "padding-right")) {
        if (css_parse_length(value, &length)) {
            style->padding.right = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "padding-bottom")) {
        if (css_parse_length(value, &length)) {
            style->padding.bottom = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "padding-left")) {
        if (css_parse_length(value, &length)) {
            style->padding.left = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "top")) {
        if (css_parse_length(value, &length)) {
            style->top = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "right")) {
        if (css_parse_length(value, &length)) {
            style->right = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "bottom")) {
        if (css_parse_length(value, &length)) {
            style->bottom = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "left")) {
        if (css_parse_length(value, &length)) {
            style->left = length;
        }

        return;
    }

    if (css_ascii_case_equal(property, "inset")) {
        CSSBoxValues inset;

        if (css_parse_box_shorthand(value, &inset)) {
            style->top = inset.top;
            style->right = inset.right;
            style->bottom = inset.bottom;
            style->left = inset.left;
        }

        return;
    }

    if (css_ascii_case_equal(property, "z-index")) {
        if (css_ascii_case_equal(value, "auto")) {
            style->z_index = 0;
        } else {
            style->z_index = (int)strtol(value, NULL, 10);
        }

        return;
    }

    if (css_ascii_case_equal(property, "opacity")) {
        float opacity;

        if (css_parse_float(value, &opacity)) {
            if (opacity < 0.0f) {
                opacity = 0.0f;
            }

            if (opacity > 1.0f) {
                opacity = 1.0f;
            }

            style->opacity = opacity;
        }

        return;
    }

    if (css_ascii_case_equal(property, "visibility")) {
        style->visibility =
            css_ascii_case_equal(value, "hidden") ? 0 : 1;

        return;
    }

    if (css_ascii_case_equal(property, "color")) {
        css_parse_color(value, &style->color);
        return;
    }

    if (css_ascii_case_equal(property, "border")) {
        css_parse_border_shorthand(value, &style->border_top);
        style->border_right = style->border_top;
        style->border_bottom = style->border_top;
        style->border_left = style->border_top;
        return;
    }

    if (css_ascii_case_equal(property, "border-width")) {
        CSSBoxValues borders;

        if (css_parse_box_shorthand(value, &borders)) {
            style->border_top.width = borders.top;
            style->border_right.width = borders.right;
            style->border_bottom.width = borders.bottom;
            style->border_left.width = borders.left;
        }

        return;
    }

    if (css_ascii_case_equal(property, "border-color")) {
        char buffer[512];
        char* cursor;
        char* word;
        CSSColor colors[4];
        int count = 0;

        snprintf(buffer, sizeof(buffer), "%s", value);
        cursor = buffer;

        while (count < 4 &&
               (word = css_next_word(&cursor)) != NULL) {
            if (!css_parse_color(word, &colors[count])) {
                break;
            }

            count++;
        }

        if (count == 1) {
            style->border_top.color =
            style->border_right.color =
            style->border_bottom.color =
            style->border_left.color = colors[0];
        } else if (count == 2) {
            style->border_top.color =
            style->border_bottom.color = colors[0];

            style->border_right.color =
            style->border_left.color = colors[1];
        } else if (count == 3) {
            style->border_top.color = colors[0];

            style->border_right.color =
            style->border_left.color = colors[1];

            style->border_bottom.color = colors[2];
        } else if (count == 4) {
            style->border_top.color = colors[0];
            style->border_right.color = colors[1];
            style->border_bottom.color = colors[2];
            style->border_left.color = colors[3];
        }

        return;
    }

    if (css_ascii_case_equal(property, "border-style")) {
        char buffer[512];
        char* cursor;
        char* word;
        CSSBorderStyle styles[4];
        int count = 0;

        snprintf(buffer, sizeof(buffer), "%s", value);
        cursor = buffer;

        while (count < 4 &&
               (word = css_next_word(&cursor)) != NULL) {
            if (!css_parse_border_style(
                    word,
                    &styles[count])) {
                break;
            }

            count++;
        }

        if (count == 1) {
            style->border_top.style =
            style->border_right.style =
            style->border_bottom.style =
            style->border_left.style = styles[0];
        } else if (count == 2) {
            style->border_top.style =
            style->border_bottom.style = styles[0];

            style->border_right.style =
            style->border_left.style = styles[1];
        } else if (count == 3) {
            style->border_top.style = styles[0];

            style->border_right.style =
            style->border_left.style = styles[1];

            style->border_bottom.style = styles[2];
        } else if (count == 4) {
            style->border_top.style = styles[0];
            style->border_right.style = styles[1];
            style->border_bottom.style = styles[2];
            style->border_left.style = styles[3];
        }

        return;
    }

    if (css_ascii_case_equal(property, "border-radius")) {
        CSSBoxValues radius;

        if (css_parse_box_shorthand(value, &radius)) {
            style->radius_top_left = radius.top;
            style->radius_top_right = radius.right;
            style->radius_bottom_right = radius.bottom;
            style->radius_bottom_left = radius.left;
        }

        return;
    }

    if (css_ascii_case_equal(property, "background-color")) {
        css_parse_color(value, &style->background_color);
        return;
    }

    if (css_ascii_case_equal(property, "background-image")) {
        if (style->background_count == 0) {
            style->background_count = 1;
            css_background_initial(
                &style->backgrounds[0]
            );
        }

        css_parse_background_image(
            value,
            &style->backgrounds[0]
        );

        return;
    }

    if (css_ascii_case_equal(property, "background-repeat")) {
        if (style->background_count == 0) {
            style->background_count = 1;
            css_background_initial(
                &style->backgrounds[0]
            );
        }

        css_parse_background_repeat(
            value,
            &style->backgrounds[0]
        );

        return;
    }

    if (css_ascii_case_equal(property, "background-position")) {
        if (style->background_count == 0) {
            style->background_count = 1;
            css_background_initial(
                &style->backgrounds[0]
            );
        }

        css_parse_background_position(
            value,
            &style->backgrounds[0]
        );

        return;
    }

    if (css_ascii_case_equal(property, "background-size")) {
        if (style->background_count == 0) {
            style->background_count = 1;
            css_background_initial(
                &style->backgrounds[0]
            );
        }

        css_parse_background_size(
            value,
            &style->backgrounds[0]
        );

        return;
    }

    if (css_ascii_case_equal(property, "background")) {
        CSSColor background_color;

        if (css_parse_color(value, &background_color)) {
            style->background_color = background_color;
            return;
        }

        if (style->background_count == 0) {
            style->background_count = 1;
            css_background_initial(
                &style->backgrounds[0]
            );
        }

        if (css_parse_background_image(
                value,
                &style->backgrounds[0])) {
            return;
        }

        if (css_parse_background_repeat(
                value,
                &style->backgrounds[0])) {
            return;
        }

        if (css_parse_background_position(
                value,
                &style->backgrounds[0])) {
            return;
        }

        if (css_parse_background_size(
                value,
                &style->backgrounds[0])) {
            return;
        }

        return;
    }

    /*
     * Unknown properties are deliberately ignored.
     * This is important for forward compatibility:
     * unsupported CSS should not destroy the rest
     * of a stylesheet.
     */
}

static void css_style_initial(
    CSSComputedStyle* style
) {
    memset(style, 0, sizeof(*style));

    style->display = CSS_DISPLAY_INLINE;
    style->position = CSS_POSITION_STATIC;

    style->overflow_x = CSS_OVERFLOW_VISIBLE;
    style->overflow_y = CSS_OVERFLOW_VISIBLE;

    style->box_sizing = CSS_BOX_CONTENT_BOX;

    style->width = css_length_auto();
    style->height = css_length_auto();

    style->min_width = css_length_px(0.0f);
    style->min_height = css_length_px(0.0f);

    style->max_width = css_length_auto();
    style->max_height = css_length_auto();

    style->margin.top =
    style->margin.right =
    style->margin.bottom =
    style->margin.left =
        css_length_px(0.0f);

    style->padding.top =
    style->padding.right =
    style->padding.bottom =
    style->padding.left =
        css_length_px(0.0f);

    style->top =
    style->right =
    style->bottom =
    style->left =
        css_length_auto();

    style->z_index = 0;
    style->opacity = 1.0f;
    style->visibility = 1;

    style->color = css_color_black();
    style->background_color = css_color_transparent();

    css_border_initial(&style->border_top);
    css_border_initial(&style->border_right);
    css_border_initial(&style->border_bottom);
    css_border_initial(&style->border_left);

    style->radius_top_left =
    style->radius_top_right =
    style->radius_bottom_right =
    style->radius_bottom_left =
        css_length_px(0.0f);

    style->background_count = 0;
}

static int css_is_valid_property_name(
    const char* property
) {
    if (!property || !*property) {
        return 0;
    }

    if (property[0] == '-' &&
        property[1] == '-') {
        return 1;
    }

    if (!(isalpha((unsigned char)property[0]) ||
          property[0] == '-')) {
        return 0;
    }

    for (const char* p = property + 1; *p; p++) {
        if (!(isalnum((unsigned char)*p) ||
              *p == '-')) {
            return 0;
        }
    }

    return 1;
}

static void css_strip_comments(
    char* value
) {
    char* read = value;
    char* write = value;

    while (*read) {
        if (read[0] == '/' &&
            read[1] == '*') {

            read += 2;

            while (*read &&
                   !(read[0] == '*' &&
                     read[1] == '/')) {
                read++;
            }

            if (*read) {
                read += 2;
            }

            *write++ = ' ';
            continue;
        }

        *write++ = *read++;
    }

    *write = '\0';
}

CSSRule* css_create_rule(
    const char* selector,
    const char* property,
    const char* value
) {
    CSSRule* rule;

    rule = malloc(sizeof(*rule));

    if (!rule) {
        return NULL;
    }

    snprintf(
        rule->selector,
        sizeof(rule->selector),
        "%s",
        selector ? selector : ""
    );

    snprintf(
        rule->property,
        sizeof(rule->property),
        "%s",
        property ? property : ""
    );

    snprintf(
        rule->value,
        sizeof(rule->value),
        "%s",
        value ? value : ""
    );

    rule->next = NULL;

    return rule;
}

void css_free_rules(
    CSSRule* head
) {
    while (head) {
        CSSRule* next = head->next;

        free(head);

        head = next;
    }
}

static int css_is_whitespace(char c) {
    return c == ' ' ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\f';
}

static const char* css_skip_whitespace(
    const char* p
) {
    while (*p &&
           css_is_whitespace(*p)) {
        p++;
    }

    return p;
}

static const char* css_skip_comment(
    const char* p
) {
    if (p[0] != '/' ||
        p[1] != '*') {
        return p;
    }

    p += 2;

    while (*p) {
        if (p[0] == '*' &&
            p[1] == '/') {
            return p + 2;
        }

        p++;
    }

    return p;
}

static const char* css_skip_space_and_comments(
    const char* p
) {
    for (;;) {
        const char* next =
            css_skip_whitespace(p);

        if (next != p) {
            p = next;
            continue;
        }

        next = css_skip_comment(p);

        if (next != p) {
            p = next;
            continue;
        }

        return p;
    }
}

static const char* css_skip_string(
    const char* p,
    char quote
) {
    p++;

    while (*p) {
        if (*p == '\\' && p[1]) {
            p += 2;
            continue;
        }

        if (*p == quote) {
            return p + 1;
        }

        p++;
    }

    return p;
}

static const char* css_find_block_start(
    const char* p
) {
    int quote = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    while (*p) {
        char c = *p;

        if (c == '/' && p[1] == '*') {
            p = css_skip_comment(p);
            continue;
        }

        if (quote) {
            p = css_skip_string(p, (char)quote);
            quote = 0;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            p++;
            continue;
        }

        if (c == '\\' && p[1]) {
            p += 2;
            continue;
        }

        if (c == '(') {
            paren_depth++;
        } else if (c == ')' && paren_depth > 0) {
            paren_depth--;
        } else if (c == '[') {
            bracket_depth++;
        } else if (c == ']' && bracket_depth > 0) {
            bracket_depth--;
        } else if (c == '{' &&
                   paren_depth == 0 &&
                   bracket_depth == 0) {
            return p;
        }

        p++;
    }

    return NULL;
}

static const char* css_find_declaration_end(
    const char* p
) {
    int quote = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    while (*p) {
        char c = *p;

        if (c == '/' && p[1] == '*') {
            p = css_skip_comment(p);
            continue;
        }

        if (quote) {
            p = css_skip_string(p, (char)quote);
            quote = 0;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            p++;
            continue;
        }

        if (c == '\\' && p[1]) {
            p += 2;
            continue;
        }

        switch (c) {
            case '(':
                paren_depth++;
                break;

            case ')':
                if (paren_depth > 0) {
                    paren_depth--;
                }
                break;

            case '[':
                bracket_depth++;
                break;

            case ']':
                if (bracket_depth > 0) {
                    bracket_depth--;
                }
                break;

            case ';':
                if (paren_depth == 0 &&
                    bracket_depth == 0) {
                    return p;
                }
                break;

            case '}':
                if (paren_depth == 0 &&
                    bracket_depth == 0) {
                    return p;
                }
                break;

            default:
                break;
        }

        p++;
    }

    return p;
}

static const char* css_find_colon(
    const char* start,
    const char* end
) {
    int quote = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    for (const char* p = start;
         p < end;
         p++) {

        char c = *p;

        if (c == '/' &&
            p + 1 < end &&
            p[1] == '*') {
            p = css_skip_comment(p);

            if (p >= end) {
                break;
            }

            p--;
            continue;
        }

        if (quote) {
            if (c == '\\' &&
                p + 1 < end) {
                p++;
                continue;
            }

            if (c == quote) {
                quote = 0;
            }

            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            continue;
        }

        if (c == '(') {
            paren_depth++;
            continue;
        }

        if (c == ')' &&
            paren_depth > 0) {
            paren_depth--;
            continue;
        }

        if (c == '[') {
            bracket_depth++;
            continue;
        }

        if (c == ']' &&
            bracket_depth > 0) {
            bracket_depth--;
            continue;
        }

        if (c == ':' &&
            paren_depth == 0 &&
            bracket_depth == 0) {
            return p;
        }
    }

    return NULL;
}

static void css_copy_range(
    char* destination,
    size_t destination_size,
    const char* start,
    const char* end
) {
    size_t length;

    if (!destination ||
        destination_size == 0) {
        return;
    }

    if (!start ||
        !end ||
        end < start) {
        destination[0] = '\0';
        return;
    }

    while (start < end &&
           css_is_whitespace(*start)) {
        start++;
    }

    while (end > start &&
           css_is_whitespace(end[-1])) {
        end--;
    }

    length = (size_t)(end - start);

    if (length >= destination_size) {
        length = destination_size - 1;
    }

    memcpy(
        destination,
        start,
        length
    );

    destination[length] = '\0';

    css_strip_comments(destination);
}

static void css_remove_important(
    char* value
) {
    size_t length;
    const char* important = "!important";

    if (!value) {
        return;
    }

    length = strlen(value);

    while (length > 0 &&
           css_is_whitespace(value[length - 1])) {
        value[--length] = '\0';
    }

    if (length < 10) {
        return;
    }

    size_t start = length - 10;

    if (!css_ascii_case_equal(
            value + start,
            important)) {
        return;
    }

    if (start > 0 &&
        !css_is_whitespace(value[start - 1])) {
        return;
    }

    value[start] = '\0';

    while (start > 0 &&
           css_is_whitespace(value[start - 1])) {
        value[--start] = '\0';
    }
}

const char* parse_css_rule(
    const char* cursor,
    char* out_selector,
    char* out_prop,
    char* out_val
) {
    if (!cursor ||
        !out_selector ||
        !out_prop ||
        !out_val) {
        return NULL;
    }

    out_selector[0] = '\0';
    out_prop[0] = '\0';
    out_val[0] = '\0';

    cursor =
        css_skip_space_and_comments(cursor);

    if (!*cursor) {
        return NULL;
    }

    while (*cursor == '}') {
        cursor++;

        cursor =
            css_skip_space_and_comments(cursor);

        if (!*cursor) {
            return NULL;
        }
    }

    const char* selector_start = cursor;

    const char* block_start =
        css_find_block_start(cursor);

    if (!block_start) {
        return NULL;
    }

    css_copy_range(
        out_selector,
        sizeof(((CSSRule*)0)->selector),
        selector_start,
        block_start
    );

    cursor = block_start + 1;

    cursor =
        css_skip_space_and_comments(cursor);

    if (!*cursor) {
        return cursor;
    }

    if (*cursor == '}') {
        return cursor + 1;
    }

    const char* declaration_start = cursor;

    const char* declaration_end =
        css_find_declaration_end(cursor);

    const char* colon =
        css_find_colon(
            declaration_start,
            declaration_end
        );

    if (!colon) {
        if (*declaration_end == '}') {
            return declaration_end + 1;
        }

        return declaration_end + 1;
    }

    css_copy_range(
        out_prop,
        sizeof(((CSSRule*)0)->property),
        declaration_start,
        colon
    );

    css_copy_range(
        out_val,
        sizeof(((CSSRule*)0)->value),
        colon + 1,
        declaration_end
    );

    if (!css_is_valid_property_name(out_prop)) {
        out_prop[0] = '\0';

        if (*declaration_end == '}') {
            return declaration_end + 1;
        }

        return declaration_end + 1;
    }

    css_remove_important(out_val);

    if (*declaration_end == ';') {
        return declaration_end + 1;
    }

    if (*declaration_end == '}') {
        return declaration_end + 1;
    }

    return declaration_end;
}
