#include "css.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
   Internal helpers
   ============================================================ */

static char* css_strdup(const char* s)
{
    if (!s)
        return NULL;

    size_t len = strlen(s);
    char* out = (char*)malloc(len + 1);

    if (!out)
        return NULL;

    memcpy(out, s, len + 1);
    return out;
}

static char* css_strndup(const char* s, size_t len)
{
    char* out = (char*)malloc(len + 1);

    if (!out)
        return NULL;

    memcpy(out, s, len);
    out[len] = '\0';

    return out;
}

static int css_ascii_tolower(int c)
{
    return tolower((unsigned char)c);
}

static int css_ieq(const char* a, const char* b)
{
    if (!a || !b)
        return 0;

    while (*a && *b)
    {
        if (css_ascii_tolower(*a) != css_ascii_tolower(*b))
            return 0;

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static int css_starts_with_i(const char* s, const char* prefix)
{
    while (*prefix)
    {
        if (!*s)
            return 0;

        if (css_ascii_tolower(*s) != css_ascii_tolower(*prefix))
            return 0;

        ++s;
        ++prefix;
    }

    return 1;
}

static const char* css_skip_whitespace(const char* p)
{
    while (p && *p)
    {
        if (isspace((unsigned char)*p))
        {
            ++p;
            continue;
        }

        if (p[0] == '/' && p[1] == '*')
        {
            p += 2;

            while (*p && !(p[0] == '*' && p[1] == '/'))
                ++p;

            if (*p)
                p += 2;

            continue;
        }

        break;
    }

    return p;
}

static void css_trim_inplace(char* s)
{
    if (!s)
        return;

    char* start = s;

    while (*start && isspace((unsigned char)*start))
        ++start;

    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);

    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

static void css_remove_comments(char* s)
{
    if (!s)
        return;

    char* read = s;
    char* write = s;
    int quote = 0;

    while (*read)
    {
        if (!quote && read[0] == '/' && read[1] == '*')
        {
            read += 2;

            while (*read && !(read[0] == '*' && read[1] == '/'))
                ++read;

            if (*read)
                read += 2;

            continue;
        }

        if (*read == '"' || *read == '\'')
        {
            if (!quote)
                quote = *read;
            else if (quote == *read)
                quote = 0;
        }

        *write++ = *read++;
    }

    *write = '\0';
}

static char* css_unquote_copy(const char* value)
{
    if (!value)
        return NULL;

    size_t len = strlen(value);

    if (len >= 2 &&
        ((value[0] == '"' && value[len - 1] == '"') ||
         (value[0] == '\'' && value[len - 1] == '\'')))
    {
        return css_strndup(value + 1, len - 2);
    }

    return css_strdup(value);
}

static int css_parse_float(const char* value, float* out)
{
    if (!value || !out)
        return 0;

    char* end = NULL;
    double number = strtod(value, &end);

    if (end == value)
        return 0;

    while (*end && isspace((unsigned char)*end))
        ++end;

    if (*end != '\0')
        return 0;

    *out = (float)number;
    return 1;
}

/* ============================================================
   Dynamic CSS rules
   ============================================================ */

CSSRule* css_create_rule(const char* selector)
{
    CSSRule* rule = (CSSRule*)calloc(1, sizeof(CSSRule));

    if (!rule)
        return NULL;

    rule->selector = css_strdup(selector ? selector : "");

    if (!rule->selector)
    {
        free(rule);
        return NULL;
    }

    return rule;
}

CSSDeclaration* css_add_declaration(
    CSSRule* rule,
    const char* property,
    const char* value)
{
    if (!rule || !property || !value)
        return NULL;

    CSSDeclaration* declaration =
        (CSSDeclaration*)calloc(1, sizeof(CSSDeclaration));

    if (!declaration)
        return NULL;

    declaration->property = css_strdup(property);
    declaration->value = css_strdup(value);

    if (!declaration->property || !declaration->value)
    {
        free(declaration->property);
        free(declaration->value);
        free(declaration);
        return NULL;
    }

    css_trim_inplace(declaration->property);
    css_trim_inplace(declaration->value);

    /* CSS values commonly arrive with !important. */
    size_t len = strlen(declaration->value);

    while (len > 0 && isspace((unsigned char)declaration->value[len - 1]))
        declaration->value[--len] = '\0';

    if (len >= 10)
    {
        char* important = declaration->value + len - 10;

        if (css_ieq(important, "!important"))
        {
            while (important > declaration->value &&
                   isspace((unsigned char)important[-1]))
            {
                --important;
            }

            *important = '\0';
        }
    }

    if (!rule->declarations)
    {
        rule->declarations = declaration;
    }
    else
    {
        CSSDeclaration* tail = rule->declarations;

        while (tail->next)
            tail = tail->next;

        tail->next = declaration;
    }

    return declaration;
}

void css_free_rule(CSSRule* rule)
{
    if (!rule)
        return;

    free(rule->selector);

    CSSDeclaration* declaration = rule->declarations;

    while (declaration)
    {
        CSSDeclaration* next = declaration->next;

        free(declaration->property);
        free(declaration->value);
        free(declaration);

        declaration = next;
    }

    free(rule);
}

void css_free_rules(CSSRule* head)
{
    while (head)
    {
        CSSRule* next = head->next;
        css_free_rule(head);
        head = next;
    }
}

/* ============================================================
   Length parsing
   ============================================================ */

CSSLength css_parse_length(const char* value)
{
    CSSLength result;
    result.value = 0.0f;
    result.unit = CSS_UNIT_NONE;

    if (!value)
        return result;

    char buffer[256];
    size_t len = strlen(value);

    if (len >= sizeof(buffer))
        return result;

    memcpy(buffer, value, len + 1);
    css_trim_inplace(buffer);

    if (css_ieq(buffer, "auto"))
    {
        result.unit = CSS_UNIT_AUTO;
        return result;
    }

    if (css_ieq(buffer, "none"))
    {
        result.unit = CSS_UNIT_NONE;
        return result;
    }

    char* end = NULL;
    double number = strtod(buffer, &end);

    if (end == buffer)
        return result;

    result.value = (float)number;

    if (*end == '\0')
    {
        result.unit = CSS_UNIT_NONE;
        return result;
    }

    if (css_ieq(end, "px"))
        result.unit = CSS_UNIT_PX;
    else if (css_ieq(end, "%"))
        result.unit = CSS_UNIT_PERCENT;
    else if (css_ieq(end, "em"))
        result.unit = CSS_UNIT_EM;
    else if (css_ieq(end, "rem"))
        result.unit = CSS_UNIT_REM;
    else if (css_ieq(end, "vw"))
        result.unit = CSS_UNIT_VW;
    else if (css_ieq(end, "vh"))
        result.unit = CSS_UNIT_VH;
    else if (css_ieq(end, "vmin"))
        result.unit = CSS_UNIT_VMIN;
    else if (css_ieq(end, "vmax"))
        result.unit = CSS_UNIT_VMAX;
    else if (css_ieq(end, "ch"))
        result.unit = CSS_UNIT_CH;
    else
        result.unit = CSS_UNIT_NONE;

    return result;
}

float css_length_to_px(
    CSSLength length,
    float reference,
    float font_size,
    float root_font_size,
    float viewport_width,
    float viewport_height)
{
    switch (length.unit)
    {
        case CSS_UNIT_PX:
            return length.value;

        case CSS_UNIT_PERCENT:
            return reference * length.value / 100.0f;

        case CSS_UNIT_EM:
            return font_size * length.value;

        case CSS_UNIT_REM:
            return root_font_size * length.value;

        case CSS_UNIT_VW:
            return viewport_width * length.value / 100.0f;

        case CSS_UNIT_VH:
            return viewport_height * length.value / 100.0f;

        case CSS_UNIT_VMIN:
        {
            float minimum =
                viewport_width < viewport_height
                    ? viewport_width
                    : viewport_height;

            return minimum * length.value / 100.0f;
        }

        case CSS_UNIT_VMAX:
        {
            float maximum =
                viewport_width > viewport_height
                    ? viewport_width
                    : viewport_height;

            return maximum * length.value / 100.0f;
        }

        case CSS_UNIT_CH:
            return font_size * 0.5f * length.value;

        default:
            return 0.0f;
    }
}

/* ============================================================
   Colors
   ============================================================ */

static unsigned char css_clamp_color(float value)
{
    if (value < 0.0f)
        value = 0.0f;

    if (value > 255.0f)
        value = 255.0f;

    return (unsigned char)(value + 0.5f);
}

static int css_parse_hex_digit(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';

    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;

    return -1;
}

static int css_parse_hex_color(
    const char* value,
    unsigned char* r,
    unsigned char* g,
    unsigned char* b,
    unsigned char* a)
{
    if (!value || value[0] != '#')
        return 0;

    size_t len = strlen(value);

    if (len == 4 || len == 5)
    {
        int rr = css_parse_hex_digit(value[1]);
        int gg = css_parse_hex_digit(value[2]);
        int bb = css_parse_hex_digit(value[3]);

        if (rr < 0 || gg < 0 || bb < 0)
            return 0;

        *r = (unsigned char)(rr * 17);
        *g = (unsigned char)(gg * 17);
        *b = (unsigned char)(bb * 17);

        if (len == 5)
        {
            int aa = css_parse_hex_digit(value[4]);

            if (aa < 0)
                return 0;

            *a = (unsigned char)(aa * 17);
        }
        else
        {
            *a = 255;
        }

        return 1;
    }

    if (len == 7 || len == 9)
    {
        int r1 = css_parse_hex_digit(value[1]);
        int r2 = css_parse_hex_digit(value[2]);
        int g1 = css_parse_hex_digit(value[3]);
        int g2 = css_parse_hex_digit(value[4]);
        int b1 = css_parse_hex_digit(value[5]);
        int b2 = css_parse_hex_digit(value[6]);

        if (r1 < 0 || r2 < 0 ||
            g1 < 0 || g2 < 0 ||
            b1 < 0 || b2 < 0)
        {
            return 0;
        }

        *r = (unsigned char)((r1 << 4) | r2);
        *g = (unsigned char)((g1 << 4) | g2);
        *b = (unsigned char)((b1 << 4) | b2);

        if (len == 9)
        {
            int a1 = css_parse_hex_digit(value[7]);
            int a2 = css_parse_hex_digit(value[8]);

            if (a1 < 0 || a2 < 0)
                return 0;

            *a = (unsigned char)((a1 << 4) | a2);
        }
        else
        {
            *a = 255;
        }

        return 1;
    }

    return 0;
}

typedef struct CSSNamedColor
{
    const char* name;
    unsigned char r;
    unsigned char g;
    unsigned char b;
} CSSNamedColor;

/*
 * Standard CSS named colors.
 * Keeping this table static makes lookup allocation-free.
 */
static const CSSNamedColor css_named_colors[] =
{
    {"transparent", 0, 0, 0},
    {"black", 0, 0, 0},
    {"silver", 192, 192, 192},
    {"gray", 128, 128, 128},
    {"grey", 128, 128, 128},
    {"white", 255, 255, 255},
    {"maroon", 128, 0, 0},
    {"red", 255, 0, 0},
    {"purple", 128, 0, 128},
    {"fuchsia", 255, 0, 255},
    {"magenta", 255, 0, 255},
    {"green", 0, 128, 0},
    {"lime", 0, 255, 0},
    {"olive", 128, 128, 0},
    {"yellow", 255, 255, 0},
    {"navy", 0, 0, 128},
    {"blue", 0, 0, 255},
    {"teal", 0, 128, 128},
    {"aqua", 0, 255, 255},
    {"cyan", 0, 255, 255},
    {"orange", 255, 165, 0},
    {"aliceblue", 240, 248, 255},
    {"antiquewhite", 250, 235, 215},
    {"aquamarine", 127, 255, 212},
    {"azure", 240, 255, 255},
    {"beige", 245, 245, 220},
    {"bisque", 255, 228, 196},
    {"blanchedalmond", 255, 235, 205},
    {"blueviolet", 138, 43, 226},
    {"brown", 165, 42, 42},
    {"burlywood", 222, 184, 135},
    {"cadetblue", 95, 158, 160},
    {"chartreuse", 127, 255, 0},
    {"chocolate", 210, 105, 30},
    {"coral", 255, 127, 80},
    {"cornflowerblue", 100, 149, 237},
    {"cornsilk", 255, 248, 220},
    {"crimson", 220, 20, 60},
    {"darkblue", 0, 0, 139},
    {"darkcyan", 0, 139, 139},
    {"darkgoldenrod", 184, 134, 11},
    {"darkgray", 169, 169, 169},
    {"darkgrey", 169, 169, 169},
    {"darkgreen", 0, 100, 0},
    {"darkkhaki", 189, 183, 107},
    {"darkmagenta", 139, 0, 139},
    {"darkolivegreen", 85, 107, 47},
    {"darkorange", 255, 140, 0},
    {"darkorchid", 153, 50, 204},
    {"darkred", 139, 0, 0},
    {"darksalmon", 233, 150, 122},
    {"darkseagreen", 143, 188, 143},
    {"darkslateblue", 72, 61, 139},
    {"darkslategray", 47, 79, 79},
    {"darkslategrey", 47, 79, 79},
    {"darkturquoise", 0, 206, 209},
    {"darkviolet", 148, 0, 211},
    {"deeppink", 255, 20, 147},
    {"deepskyblue", 0, 191, 255},
    {"dimgray", 105, 105, 105},
    {"dimgrey", 105, 105, 105},
    {"dodgerblue", 30, 144, 255},
    {"firebrick", 178, 34, 34},
    {"floralwhite", 255, 250, 240},
    {"forestgreen", 34, 139, 34},
    {"gainsboro", 220, 220, 220},
    {"ghostwhite", 248, 248, 255},
    {"gold", 255, 215, 0},
    {"goldenrod", 218, 165, 32},
    {"greenyellow", 173, 255, 47},
    {"honeydew", 240, 255, 240},
    {"hotpink", 255, 105, 180},
    {"indianred", 205, 92, 92},
    {"indigo", 75, 0, 130},
    {"ivory", 255, 255, 240},
    {"khaki", 240, 230, 140},
    {"lavender", 230, 230, 250},
    {"lavenderblush", 255, 240, 245},
    {"lawngreen", 124, 252, 0},
    {"lemonchiffon", 255, 250, 205},
    {"lightblue", 173, 216, 230},
    {"lightcoral", 240, 128, 128},
    {"lightcyan", 224, 255, 255},
    {"lightgoldenrodyellow", 250, 250, 210},
    {"lightgray", 211, 211, 211},
    {"lightgrey", 211, 211, 211},
    {"lightgreen", 144, 238, 144},
    {"lightpink", 255, 182, 193},
    {"lightsalmon", 255, 160, 122},
    {"lightseagreen", 32, 178, 170},
    {"lightskyblue", 135, 206, 250},
    {"lightslategray", 119, 136, 153},
    {"lightslategrey", 119, 136, 153},
    {"lightsteelblue", 176, 196, 222},
    {"lightyellow", 255, 255, 224},
    {"limegreen", 50, 205, 50},
    {"linen", 250, 240, 230},
    {"mediumaquamarine", 102, 205, 170},
    {"mediumblue", 0, 0, 205},
    {"mediumorchid", 186, 85, 211},
    {"mediumpurple", 147, 112, 219},
    {"mediumseagreen", 60, 179, 113},
    {"mediumslateblue", 123, 104, 238},
    {"mediumspringgreen", 0, 250, 154},
    {"mediumturquoise", 72, 209, 204},
    {"mediumvioletred", 199, 21, 133},
    {"midnightblue", 25, 25, 112},
    {"mintcream", 245, 255, 250},
    {"mistyrose", 255, 228, 225},
    {"moccasin", 255, 228, 181},
    {"navajowhite", 255, 222, 173},
    {"oldlace", 253, 245, 230},
    {"olivedrab", 107, 142, 35},
    {"orangered", 255, 69, 0},
    {"orchid", 218, 112, 214},
    {"palegoldenrod", 238, 232, 170},
    {"palegreen", 152, 251, 152},
    {"paleturquoise", 175, 238, 238},
    {"palevioletred", 219, 112, 147},
    {"papayawhip", 255, 239, 213},
    {"peachpuff", 255, 218, 185},
    {"peru", 205, 133, 63},
    {"pink", 255, 192, 203},
    {"plum", 221, 160, 221},
    {"powderblue", 176, 224, 230},
    {"rebeccapurple", 102, 51, 153},
    {"rosybrown", 188, 143, 143},
    {"royalblue", 65, 105, 225},
    {"saddlebrown", 139, 69, 19},
    {"salmon", 250, 128, 114},
    {"sandybrown", 244, 164, 96},
    {"seagreen", 46, 139, 87},
    {"seashell", 255, 245, 238},
    {"sienna", 160, 82, 45},
    {"skyblue", 135, 206, 235},
    {"slateblue", 106, 90, 205},
    {"slategray", 112, 128, 144},
    {"slategrey", 112, 128, 144},
    {"snow", 255, 250, 250},
    {"springgreen", 0, 255, 127},
    {"steelblue", 70, 130, 180},
    {"tan", 210, 180, 140},
    {"thistle", 216, 191, 216},
    {"tomato", 255, 99, 71},
    {"turquoise", 64, 224, 208},
    {"violet", 238, 130, 238},
    {"wheat", 245, 222, 179},
    {"whitesmoke", 245, 245, 245},
    {"yellowgreen", 154, 205, 50}
};

static int css_parse_named_color(
    const char* value,
    unsigned char* r,
    unsigned char* g,
    unsigned char* b,
    unsigned char* a)
{
    size_t count =
        sizeof(css_named_colors) / sizeof(css_named_colors[0]);

    for (size_t i = 0; i < count; ++i)
    {
        if (css_ieq(value, css_named_colors[i].name))
        {
            *r = css_named_colors[i].r;
            *g = css_named_colors[i].g;
            *b = css_named_colors[i].b;
            *a = 255;

            if (css_ieq(value, "transparent"))
                *a = 0;

            return 1;
        }
    }

    return 0;
}

static float css_parse_component(
    const char* text,
    float max_value,
    int* ok)
{
    char buffer[64];
    size_t len = strlen(text);

    if (len >= sizeof(buffer))
    {
        *ok = 0;
        return 0.0f;
    }

    memcpy(buffer, text, len + 1);
    css_trim_inplace(buffer);

    size_t blen = strlen(buffer);

    if (blen > 0 && buffer[blen - 1] == '%')
    {
        buffer[blen - 1] = '\0';

        float percentage;

        if (!css_parse_float(buffer, &percentage))
        {
            *ok = 0;
            return 0.0f;
        }

        *ok = 1;
        return percentage * max_value / 100.0f;
    }

    float number;

    if (!css_parse_float(buffer, &number))
    {
        *ok = 0;
        return 0.0f;
    }

    *ok = 1;
    return number;
}

static int css_split_function_args(
    const char* inside,
    char args[][64],
    int max_args)
{
    int count = 0;
    int depth = 0;
    int quote = 0;

    const char* start = inside;
    const char* p = inside;

    while (*p)
    {
        char c = *p;

        if (quote)
        {
            if (c == quote && (p == inside || p[-1] != '\\'))
                quote = 0;
        }
        else if (c == '"' || c == '\'')
        {
            quote = c;
        }
        else if (c == '(')
        {
            ++depth;
        }
        else if (c == ')')
        {
            if (depth > 0)
                --depth;
        }
        else if ((c == ',' || isspace((unsigned char)c)) && depth == 0)
        {
            if (p > start)
            {
                size_t len = (size_t)(p - start);

                if (len >= 64)
                    len = 63;

                memcpy(args[count], start, len);
                args[count][len] = '\0';
                css_trim_inplace(args[count]);

                if (args[count][0] != '\0')
                    ++count;

                if (count >= max_args)
                    return count;
            }

            while (isspace((unsigned char)*p))
                ++p;

            if (*p == ',')
                ++p;

            while (isspace((unsigned char)*p))
                ++p;

            start = p;
            continue;
        }

        ++p;
    }

    if (p > start && count < max_args)
    {
        size_t len = (size_t)(p - start);

        if (len >= 64)
            len = 63;

        memcpy(args[count], start, len);
        args[count][len] = '\0';
        css_trim_inplace(args[count]);

        if (args[count][0] != '\0')
            ++count;
    }

    return count;
}

CSSColor css_parse_color(const char* value)
{
    CSSColor result;

    result.r = 0;
    result.g = 0;
    result.b = 0;
    result.a = 255;

    if (!value)
        return result;

    char buffer[512];
    size_t len = strlen(value);

    if (len >= sizeof(buffer))
        return result;

    memcpy(buffer, value, len + 1);
    css_trim_inplace(buffer);

    if (css_parse_hex_color(
            buffer,
            &result.r,
            &result.g,
            &result.b,
            &result.a))
    {
        return result;
    }

    if (css_parse_named_color(
            buffer,
            &result.r,
            &result.g,
            &result.b,
            &result.a))
    {
        return result;
    }

    const char* open = strchr(buffer, '(');
    const char* close = strrchr(buffer, ')');

    if (!open || !close || close < open)
        return result;

    char function[32];
    size_t function_len = (size_t)(open - buffer);

    if (function_len >= sizeof(function))
        return result;

    memcpy(function, buffer, function_len);
    function[function_len] = '\0';
    css_trim_inplace(function);

    char inside[448];
    size_t inside_len = (size_t)(close - open - 1);

    if (inside_len >= sizeof(inside))
        return result;

    memcpy(inside, open + 1, inside_len);
    inside[inside_len] = '\0';

    char args[8][64];
    int count = css_split_function_args(
        inside,
        args,
        8);

    if (count < 3)
        return result;

    int ok_r;
    int ok_g;
    int ok_b;

    float r = css_parse_component(args[0], 255.0f, &ok_r);
    float g = css_parse_component(args[1], 255.0f, &ok_g);
    float b = css_parse_component(args[2], 255.0f, &ok_b);

    if (!ok_r || !ok_g || !ok_b)
        return result;

    result.r = css_clamp_color(r);
    result.g = css_clamp_color(g);
    result.b = css_clamp_color(b);

    if (count >= 4)
    {
        char alpha[64];
        strcpy(alpha, args[3]);

        size_t alpha_len = strlen(alpha);

        if (alpha_len > 0 && alpha[alpha_len - 1] == '%')
        {
            alpha[alpha_len - 1] = '\0';

            float percentage;

            if (css_parse_float(alpha, &percentage))
            {
                if (percentage < 0.0f)
                    percentage = 0.0f;

                if (percentage > 100.0f)
                    percentage = 100.0f;

                result.a =
                    (unsigned char)(percentage * 2.55f + 0.5f);
            }
        }
        else
        {
            float alpha_value;

            if (css_parse_float(alpha, &alpha_value))
            {
                if (alpha_value < 0.0f)
                    alpha_value = 0.0f;

                if (alpha_value > 1.0f)
                    alpha_value = 1.0f;

                result.a =
                    (unsigned char)(alpha_value * 255.0f + 0.5f);
            }
        }
    }

    (void)function;

    return result;
}

/* ============================================================
   Value parsing
   ============================================================ */

static int css_parse_display(
    const char* value,
    CSSDisplay* display)
{
    if (css_ieq(value, "inline"))
        *display = CSS_DISPLAY_INLINE;
    else if (css_ieq(value, "block"))
        *display = CSS_DISPLAY_BLOCK;
    else if (css_ieq(value, "inline-block"))
        *display = CSS_DISPLAY_INLINE_BLOCK;
    else if (css_ieq(value, "none"))
        *display = CSS_DISPLAY_NONE;
    else if (css_ieq(value, "flex"))
        *display = CSS_DISPLAY_FLEX;
    else if (css_ieq(value, "inline-flex"))
        *display = CSS_DISPLAY_INLINE_FLEX;
    else if (css_ieq(value, "grid"))
        *display = CSS_DISPLAY_GRID;
    else if (css_ieq(value, "inline-grid"))
        *display = CSS_DISPLAY_INLINE_GRID;
    else
        return 0;

    return 1;
}

static int css_parse_position(
    const char* value,
    CSSPosition* position)
{
    if (css_ieq(value, "static"))
        *position = CSS_POSITION_STATIC;
    else if (css_ieq(value, "relative"))
        *position = CSS_POSITION_RELATIVE;
    else if (css_ieq(value, "absolute"))
        *position = CSS_POSITION_ABSOLUTE;
    else if (css_ieq(value, "fixed"))
        *position = CSS_POSITION_FIXED;
    else if (css_ieq(value, "sticky"))
        *position = CSS_POSITION_STICKY;
    else
        return 0;

    return 1;
}

static int css_parse_overflow(
    const char* value,
    CSSOverflow* overflow)
{
    if (css_ieq(value, "visible"))
        *overflow = CSS_OVERFLOW_VISIBLE;
    else if (css_ieq(value, "hidden"))
        *overflow = CSS_OVERFLOW_HIDDEN;
    else if (css_ieq(value, "clip"))
        *overflow = CSS_OVERFLOW_CLIP;
    else if (css_ieq(value, "scroll"))
        *overflow = CSS_OVERFLOW_SCROLL;
    else if (css_ieq(value, "auto"))
        *overflow = CSS_OVERFLOW_AUTO;
    else
        return 0;

    return 1;
}

static int css_parse_box_sizing(
    const char* value,
    CSSBoxSizing* sizing)
{
    if (css_ieq(value, "content-box"))
        *sizing = CSS_BOX_SIZING_CONTENT_BOX;
    else if (css_ieq(value, "border-box"))
        *sizing = CSS_BOX_SIZING_BORDER_BOX;
    else
        return 0;

    return 1;
}

static int css_parse_border_style(
    const char* value,
    CSSBorderStyle* style)
{
    if (css_ieq(value, "none"))
        *style = CSS_BORDER_NONE;
    else if (css_ieq(value, "hidden"))
        *style = CSS_BORDER_HIDDEN;
    else if (css_ieq(value, "dotted"))
        *style = CSS_BORDER_DOTTED;
    else if (css_ieq(value, "dashed"))
        *style = CSS_BORDER_DASHED;
    else if (css_ieq(value, "solid"))
        *style = CSS_BORDER_SOLID;
    else if (css_ieq(value, "double"))
        *style = CSS_BORDER_DOUBLE;
    else if (css_ieq(value, "groove"))
        *style = CSS_BORDER_GROOVE;
    else if (css_ieq(value, "ridge"))
        *style = CSS_BORDER_RIDGE;
    else if (css_ieq(value, "inset"))
        *style = CSS_BORDER_INSET;
    else if (css_ieq(value, "outset"))
        *style = CSS_BORDER_OUTSET;
    else
        return 0;

    return 1;
}

/* ============================================================
   Shorthand parsing
   ============================================================ */

static int css_tokenize_space_values(
    const char* value,
    char tokens[][128],
    int max_tokens)
{
    int count = 0;
    int depth = 0;
    int quote = 0;

    const char* start = value;
    const char* p = value;

    while (*p)
    {
        char c = *p;

        if (quote)
        {
            if (c == quote && (p == value || p[-1] != '\\'))
                quote = 0;
        }
        else if (c == '"' || c == '\'')
        {
            quote = c;
        }
        else if (c == '(')
        {
            ++depth;
        }
        else if (c == ')')
        {
            if (depth > 0)
                --depth;
        }
        else if (isspace((unsigned char)c) && depth == 0)
        {
            if (p > start)
            {
                size_t len = (size_t)(p - start);

                if (len >= 128)
                    len = 127;

                memcpy(tokens[count], start, len);
                tokens[count][len] = '\0';
                css_trim_inplace(tokens[count]);

                if (tokens[count][0])
                    ++count;

                if (count >= max_tokens)
                    return count;
            }

            while (isspace((unsigned char)*p))
                ++p;

            start = p;
            continue;
        }

        ++p;
    }

    if (p > start && count < max_tokens)
    {
        size_t len = (size_t)(p - start);

        if (len >= 128)
            len = 127;

        memcpy(tokens[count], start, len);
        tokens[count][len] = '\0';
        css_trim_inplace(tokens[count]);

        if (tokens[count][0])
            ++count;
    }

    return count;
}

static void css_apply_box_shorthand(
    CSSLength* top,
    CSSLength* right,
    CSSLength* bottom,
    CSSLength* left,
    const char* value)
{
    char tokens[4][128];

    int count = css_tokenize_space_values(
        value,
        tokens,
        4);

    if (count <= 0)
        return;

    CSSLength values[4];

    for (int i = 0; i < count; ++i)
        values[i] = css_parse_length(tokens[i]);

    if (count == 1)
    {
        *top = values[0];
        *right = values[0];
        *bottom = values[0];
        *left = values[0];
    }
    else if (count == 2)
    {
        *top = values[0];
        *bottom = values[0];
        *right = values[1];
        *left = values[1];
    }
    else if (count == 3)
    {
        *top = values[0];
        *right = values[1];
        *left = values[1];
        *bottom = values[2];
    }
    else
    {
        *top = values[0];
        *right = values[1];
        *bottom = values[2];
        *left = values[3];
    }
}

static void css_apply_border_width_shorthand(
    CSSLength* top,
    CSSLength* right,
    CSSLength* bottom,
    CSSLength* left,
    const char* value)
{
    css_apply_box_shorthand(
        top,
        right,
        bottom,
        left,
        value);
}

static void css_apply_border_style_shorthand(
    CSSBorderStyle* top,
    CSSBorderStyle* right,
    CSSBorderStyle* bottom,
    CSSBorderStyle* left,
    const char* value)
{
    char tokens[4][128];

    int count = css_tokenize_space_values(
        value,
        tokens,
        4);

    if (count <= 0)
        return;

    CSSBorderStyle values[4];

    for (int i = 0; i < count; ++i)
    {
        if (!css_parse_border_style(tokens[i], &values[i]))
            return;
    }

    if (count == 1)
    {
        *top = values[0];
        *right = values[0];
        *bottom = values[0];
        *left = values[0];
    }
    else if (count == 2)
    {
        *top = values[0];
        *bottom = values[0];
        *right = values[1];
        *left = values[1];
    }
    else if (count == 3)
    {
        *top = values[0];
        *right = values[1];
        *left = values[1];
        *bottom = values[2];
    }
    else
    {
        *top = values[0];
        *right = values[1];
        *bottom = values[2];
        *left = values[3];
    }
}

static void css_apply_border_color_shorthand(
    CSSColor* top,
    CSSColor* right,
    CSSColor* bottom,
    CSSColor* left,
    const char* value)
{
    char tokens[4][128];

    int count = css_tokenize_space_values(
        value,
        tokens,
        4);

    if (count <= 0)
        return;

    CSSColor values[4];

    for (int i = 0; i < count; ++i)
        values[i] = css_parse_color(tokens[i]);

    if (count == 1)
    {
        *top = values[0];
        *right = values[0];
        *bottom = values[0];
        *left = values[0];
    }
    else if (count == 2)
    {
        *top = values[0];
        *bottom = values[0];
        *right = values[1];
        *left = values[1];
    }
    else if (count == 3)
    {
        *top = values[0];
        *right = values[1];
        *left = values[1];
        *bottom = values[2];
    }
    else
    {
        *top = values[0];
        *right = values[1];
        *bottom = values[2];
        *left = values[3];
    }
}

/* ============================================================
   Background parsing
   ============================================================ */

static void css_apply_background_value(
    CSSComputedStyle* style,
    const char* value)
{
    if (css_ieq(value, "none"))
    {
        style->background_image = NULL;
        return;
    }

    char tokens[16][128];
    int count = css_tokenize_space_values(
        value,
        tokens,
        16);

    for (int i = 0; i < count; ++i)
    {
        CSSColor color = css_parse_color(tokens[i]);

        /*
         * A token is treated as a color when it actually looks
         * like a color. This prevents "cover", "center", etc.
         * from becoming black accidentally.
         */
        if (tokens[i][0] == '#' ||
            strchr(tokens[i], '(') ||
            css_ieq(tokens[i], "transparent") ||
            css_ieq(tokens[i], "black") ||
            css_ieq(tokens[i], "white") ||
            css_ieq(tokens[i], "red") ||
            css_ieq(tokens[i], "green") ||
            css_ieq(tokens[i], "blue") ||
            css_ieq(tokens[i], "yellow") ||
            css_ieq(tokens[i], "orange") ||
            css_ieq(tokens[i], "purple") ||
            css_ieq(tokens[i], "gray") ||
            css_ieq(tokens[i], "grey"))
        {
            style->background_color = color;
            continue;
        }

        if (css_ieq(tokens[i], "repeat"))
        {
            style->background_repeat = CSS_BACKGROUND_REPEAT;
            continue;
        }

        if (css_ieq(tokens[i], "no-repeat"))
        {
            style->background_repeat =
                CSS_BACKGROUND_NO_REPEAT;
            continue;
        }

        if (css_ieq(tokens[i], "repeat-x"))
        {
            style->background_repeat =
                CSS_BACKGROUND_REPEAT_X;
            continue;
        }

        if (css_ieq(tokens[i], "repeat-y"))
        {
            style->background_repeat =
                CSS_BACKGROUND_REPEAT_Y;
            continue;
        }

        if (css_ieq(tokens[i], "cover"))
        {
            style->background_size.type =
                CSS_BACKGROUND_SIZE_COVER;
            continue;
        }

        if (css_ieq(tokens[i], "contain"))
        {
            style->background_size.type =
                CSS_BACKGROUND_SIZE_CONTAIN;
            continue;
        }

        if (css_ieq(tokens[i], "center"))
        {
            style->background_position_x =
                css_parse_length("50%");
            style->background_position_y =
                css_parse_length("50%");
            continue;
        }

        if (css_ieq(tokens[i], "left"))
        {
            style->background_position_x =
                css_parse_length("0%");
            continue;
        }

        if (css_ieq(tokens[i], "right"))
        {
            style->background_position_x =
                css_parse_length("100%");
            continue;
        }

        if (css_ieq(tokens[i], "top"))
        {
            style->background_position_y =
                css_parse_length("0%");
            continue;
        }

        if (css_ieq(tokens[i], "bottom"))
        {
            style->background_position_y =
                css_parse_length("100%");
            continue;
        }

        CSSLength length = css_parse_length(tokens[i]);

        if (length.unit != CSS_UNIT_NONE ||
            strchr(tokens[i], '.') ||
            isdigit((unsigned char)tokens[i][0]))
        {
            style->background_size.type =
                CSS_BACKGROUND_SIZE_EXPLICIT;

            style->background_size.width = length;

            if (i + 1 < count)
            {
                CSSLength second =
                    css_parse_length(tokens[i + 1]);

                if (second.unit != CSS_UNIT_NONE)
                {
                    style->background_size.height = second;
                    ++i;
                }
            }

            continue;
        }

        if (css_starts_with_i(tokens[i], "url(") ||
            css_starts_with_i(tokens[i], "linear-gradient(") ||
            css_starts_with_i(tokens[i], "radial-gradient(") ||
            css_starts_with_i(tokens[i], "conic-gradient("))
        {
            free(style->background_image);
            style->background_image =
                css_strdup(tokens[i]);

            continue;
        }
    }
}

/* ============================================================
   Initial computed style
   ============================================================ */

void css_style_initial(CSSComputedStyle* style)
{
    if (!style)
        return;

    memset(style, 0, sizeof(*style));

    style->display = CSS_DISPLAY_INLINE;
    style->position = CSS_POSITION_STATIC;

    style->overflow_x = CSS_OVERFLOW_VISIBLE;
    style->overflow_y = CSS_OVERFLOW_VISIBLE;

    style->box_sizing = CSS_BOX_SIZING_CONTENT_BOX;

    style->visibility = CSS_VISIBILITY_VISIBLE;
    style->opacity = 1.0f;

    style->width = css_parse_length("auto");
    style->height = css_parse_length("auto");

    style->min_width = css_parse_length("auto");
    style->min_height = css_parse_length("auto");

    style->max_width = css_parse_length("none");
    style->max_height = css_parse_length("none");

    style->margin_top = css_parse_length("0");
    style->margin_right = css_parse_length("0");
    style->margin_bottom = css_parse_length("0");
    style->margin_left = css_parse_length("0");

    style->padding_top = css_parse_length("0");
    style->padding_right = css_parse_length("0");
    style->padding_bottom = css_parse_length("0");
    style->padding_left = css_parse_length("0");

    style->top = css_parse_length("auto");
    style->right = css_parse_length("auto");
    style->bottom = css_parse_length("auto");
    style->left = css_parse_length("auto");

    style->z_index = 0;

    style->color.r = 0;
    style->color.g = 0;
    style->color.b = 0;
    style->color.a = 255;

    style->background_color.r = 0;
    style->background_color.g = 0;
    style->background_color.b = 0;
    style->background_color.a = 0;

    style->border_top.width = css_parse_length("medium");
    style->border_right.width = css_parse_length("medium");
    style->border_bottom.width = css_parse_length("medium");
    style->border_left.width = css_parse_length("medium");

    style->border_top.style = CSS_BORDER_NONE;
    style->border_right.style = CSS_BORDER_NONE;
    style->border_bottom.style = CSS_BORDER_NONE;
    style->border_left.style = CSS_BORDER_NONE;

    style->border_top.color = style->color;
    style->border_right.color = style->color;
    style->border_bottom.color = style->color;
    style->border_left.color = style->color;

    style->border_radius_top_left = css_parse_length("0");
    style->border_radius_top_right = css_parse_length("0");
    style->border_radius_bottom_right = css_parse_length("0");
    style->border_radius_bottom_left = css_parse_length("0");

    style->background_repeat = CSS_BACKGROUND_REPEAT;

    style->background_position_x =
        css_parse_length("0%");
    style->background_position_y =
        css_parse_length("0%");

    style->background_size.type =
        CSS_BACKGROUND_SIZE_AUTO;

    style->background_size.width =
        css_parse_length("auto");

    style->background_size.height =
        css_parse_length("auto");

    style->background_image = NULL;
}

/* ============================================================
   Property application
   ============================================================ */

static void css_apply_single_border(
    CSSBorder* border,
    const char* value)
{
    char tokens[3][128];

    int count = css_tokenize_space_values(
        value,
        tokens,
        3);

    for (int i = 0; i < count; ++i)
    {
        CSSBorderStyle parsed_style;

        if (css_parse_border_style(tokens[i], &parsed_style))
        {
            border->style = parsed_style;
            continue;
        }

        if (tokens[i][0] == '#' ||
            strchr(tokens[i], '(') ||
            css_parse_named_color(
                tokens[i],
                &border->color.r,
                &border->color.g,
                &border->color.b,
                &border->color.a))
        {
            border->color = css_parse_color(tokens[i]);
            continue;
        }

        CSSLength length = css_parse_length(tokens[i]);

        if (length.unit != CSS_UNIT_NONE ||
            css_ieq(tokens[i], "thin") ||
            css_ieq(tokens[i], "medium") ||
            css_ieq(tokens[i], "thick"))
        {
            if (css_ieq(tokens[i], "thin"))
                length = css_parse_length("1px");
            else if (css_ieq(tokens[i], "medium"))
                length = css_parse_length("3px");
            else if (css_ieq(tokens[i], "thick"))
                length = css_parse_length("5px");

            border->width = length;
        }
    }
}

static void css_apply_border_radius(
    CSSComputedStyle* style,
    const char* value)
{
    char tokens[4][128];

    int count = css_tokenize_space_values(
        value,
        tokens,
        4);

    if (count <= 0)
        return;

    CSSLength values[4];

    for (int i = 0; i < count; ++i)
        values[i] = css_parse_length(tokens[i]);

    if (count == 1)
    {
        style->border_radius_top_left = values[0];
        style->border_radius_top_right = values[0];
        style->border_radius_bottom_right = values[0];
        style->border_radius_bottom_left = values[0];
    }
    else if (count == 2)
    {
        style->border_radius_top_left = values[0];
        style->border_radius_bottom_right = values[0];

        style->border_radius_top_right = values[1];
        style->border_radius_bottom_left = values[1];
    }
    else if (count == 3)
    {
        style->border_radius_top_left = values[0];
        style->border_radius_top_right = values[1];
        style->border_radius_bottom_left = values[1];
        style->border_radius_bottom_right = values[2];
    }
    else
    {
        style->border_radius_top_left = values[0];
        style->border_radius_top_right = values[1];
        style->border_radius_bottom_right = values[2];
        style->border_radius_bottom_left = values[3];
    }
}

void css_apply_declaration(
    CSSComputedStyle* style,
    const char* property,
    const char* value)
{
    if (!style || !property || !value)
        return;

    char prop[128];
    char val[1024];

    if (strlen(property) >= sizeof(prop) ||
        strlen(value) >= sizeof(val))
    {
        return;
    }

    strcpy(prop, property);
    strcpy(val, value);

    css_trim_inplace(prop);
    css_trim_inplace(val);

    for (char* p = prop; *p; ++p)
        *p = (char)css_ascii_tolower(*p);

    if (prop[0] == '-' && prop[1] == '-')
    {
        /*
         * Custom properties are retained in CSSDeclaration but
         * aren't resolved here because CSSComputedStyle currently
         * has no custom-property storage.
         */
        return;
    }

    if (css_ieq(prop, "display"))
    {
        css_parse_display(val, &style->display);
        return;
    }

    if (css_ieq(prop, "position"))
    {
        css_parse_position(val, &style->position);
        return;
    }

    if (css_ieq(prop, "overflow"))
    {
        CSSOverflow overflow;

        if (css_parse_overflow(val, &overflow))
        {
            style->overflow_x = overflow;
            style->overflow_y = overflow;
        }

        return;
    }

    if (css_ieq(prop, "overflow-x"))
    {
        css_parse_overflow(val, &style->overflow_x);
        return;
    }

    if (css_ieq(prop, "overflow-y"))
    {
        css_parse_overflow(val, &style->overflow_y);
        return;
    }

    if (css_ieq(prop, "box-sizing"))
    {
        css_parse_box_sizing(val, &style->box_sizing);
        return;
    }

    if (css_ieq(prop, "width"))
    {
        style->width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "height"))
    {
        style->height = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "min-width"))
    {
        style->min_width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "min-height"))
    {
        style->min_height = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "max-width"))
    {
        style->max_width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "max-height"))
    {
        style->max_height = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "margin"))
    {
        css_apply_box_shorthand(
            &style->margin_top,
            &style->margin_right,
            &style->margin_bottom,
            &style->margin_left,
            val);
        return;
    }

    if (css_ieq(prop, "margin-top"))
    {
        style->margin_top = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "margin-right"))
    {
        style->margin_right = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "margin-bottom"))
    {
        style->margin_bottom = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "margin-left"))
    {
        style->margin_left = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "padding"))
    {
        css_apply_box_shorthand(
            &style->padding_top,
            &style->padding_right,
            &style->padding_bottom,
            &style->padding_left,
            val);
        return;
    }

    if (css_ieq(prop, "padding-top"))
    {
        style->padding_top = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "padding-right"))
    {
        style->padding_right = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "padding-bottom"))
    {
        style->padding_bottom = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "padding-left"))
    {
        style->padding_left = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "top"))
    {
        style->top = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "right"))
    {
        style->right = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "bottom"))
    {
        style->bottom = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "left"))
    {
        style->left = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "inset"))
    {
        css_apply_box_shorthand(
            &style->top,
            &style->right,
            &style->bottom,
            &style->left,
            val);
        return;
    }

    if (css_ieq(prop, "z-index"))
    {
        char* end = NULL;
        long z = strtol(val, &end, 10);

        if (end != val)
            style->z_index = (int)z;

        return;
    }

    if (css_ieq(prop, "opacity"))
    {
        float opacity;

        if (css_parse_float(val, &opacity))
        {
            if (opacity < 0.0f)
                opacity = 0.0f;

            if (opacity > 1.0f)
                opacity = 1.0f;

            style->opacity = opacity;
        }

        return;
    }

    if (css_ieq(prop, "visibility"))
    {
        if (css_ieq(val, "visible"))
            style->visibility = CSS_VISIBILITY_VISIBLE;
        else if (css_ieq(val, "hidden"))
            style->visibility = CSS_VISIBILITY_HIDDEN;
        else if (css_ieq(val, "collapse"))
            style->visibility = CSS_VISIBILITY_COLLAPSE;

        return;
    }

    if (css_ieq(prop, "color"))
    {
        style->color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "border"))
    {
        css_apply_single_border(&style->border_top, val);
        css_apply_single_border(&style->border_right, val);
        css_apply_single_border(&style->border_bottom, val);
        css_apply_single_border(&style->border_left, val);
        return;
    }

    if (css_ieq(prop, "border-top"))
    {
        css_apply_single_border(&style->border_top, val);
        return;
    }

    if (css_ieq(prop, "border-right"))
    {
        css_apply_single_border(&style->border_right, val);
        return;
    }

    if (css_ieq(prop, "border-bottom"))
    {
        css_apply_single_border(&style->border_bottom, val);
        return;
    }

    if (css_ieq(prop, "border-left"))
    {
        css_apply_single_border(&style->border_left, val);
        return;
    }

    if (css_ieq(prop, "border-width"))
    {
        css_apply_border_width_shorthand(
            &style->border_top.width,
            &style->border_right.width,
            &style->border_bottom.width,
            &style->border_left.width,
            val);
        return;
    }

    if (css_ieq(prop, "border-style"))
    {
        css_apply_border_style_shorthand(
            &style->border_top.style,
            &style->border_right.style,
            &style->border_bottom.style,
            &style->border_left.style,
            val);
        return;
    }

    if (css_ieq(prop, "border-color"))
    {
        css_apply_border_color_shorthand(
            &style->border_top.color,
            &style->border_right.color,
            &style->border_bottom.color,
            &style->border_left.color,
            val);
        return;
    }

    if (css_ieq(prop, "border-top-width"))
    {
        style->border_top.width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-right-width"))
    {
        style->border_right.width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-bottom-width"))
    {
        style->border_bottom.width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-left-width"))
    {
        style->border_left.width = css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-top-style"))
    {
        css_parse_border_style(
            val,
            &style->border_top.style);
        return;
    }

    if (css_ieq(prop, "border-right-style"))
    {
        css_parse_border_style(
            val,
            &style->border_right.style);
        return;
    }

    if (css_ieq(prop, "border-bottom-style"))
    {
        css_parse_border_style(
            val,
            &style->border_bottom.style);
        return;
    }

    if (css_ieq(prop, "border-left-style"))
    {
        css_parse_border_style(
            val,
            &style->border_left.style);
        return;
    }

    if (css_ieq(prop, "border-top-color"))
    {
        style->border_top.color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "border-right-color"))
    {
        style->border_right.color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "border-bottom-color"))
    {
        style->border_bottom.color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "border-left-color"))
    {
        style->border_left.color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "border-radius"))
    {
        css_apply_border_radius(style, val);
        return;
    }

    if (css_ieq(prop, "border-top-left-radius"))
    {
        style->border_radius_top_left =
            css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-top-right-radius"))
    {
        style->border_radius_top_right =
            css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-bottom-right-radius"))
    {
        style->border_radius_bottom_right =
            css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "border-bottom-left-radius"))
    {
        style->border_radius_bottom_left =
            css_parse_length(val);
        return;
    }

    if (css_ieq(prop, "background-color"))
    {
        style->background_color = css_parse_color(val);
        return;
    }

    if (css_ieq(prop, "background-image"))
    {
        free(style->background_image);
        style->background_image =
            css_unquote_copy(val);
        return;
    }

    if (css_ieq(prop, "background-repeat"))
    {
        if (css_ieq(val, "repeat"))
            style->background_repeat =
                CSS_BACKGROUND_REPEAT;
        else if (css_ieq(val, "no-repeat"))
            style->background_repeat =
                CSS_BACKGROUND_NO_REPEAT;
        else if (css_ieq(val, "repeat-x"))
            style->background_repeat =
                CSS_BACKGROUND_REPEAT_X;
        else if (css_ieq(val, "repeat-y"))
            style->background_repeat =
                CSS_BACKGROUND_REPEAT_Y;

        return;
    }

    if (css_ieq(prop, "background-position"))
    {
        char tokens[2][128];

        int count = css_tokenize_space_values(
            val,
            tokens,
            2);

        if (count >= 1)
        {
            if (css_ieq(tokens[0], "left"))
                style->background_position_x =
                    css_parse_length("0%");
            else if (css_ieq(tokens[0], "center"))
                style->background_position_x =
                    css_parse_length("50%");
            else if (css_ieq(tokens[0], "right"))
                style->background_position_x =
                    css_parse_length("100%");
            else
                style->background_position_x =
                    css_parse_length(tokens[0]);
        }

        if (count >= 2)
        {
            if (css_ieq(tokens[1], "top"))
                style->background_position_y =
                    css_parse_length("0%");
            else if (css_ieq(tokens[1], "center"))
                style->background_position_y =
                    css_parse_length("50%");
            else if (css_ieq(tokens[1], "bottom"))
                style->background_position_y =
                    css_parse_length("100%");
            else
                style->background_position_y =
                    css_parse_length(tokens[1]);
        }

        return;
    }

    if (css_ieq(prop, "background-size"))
    {
        if (css_ieq(val, "cover"))
        {
            style->background_size.type =
                CSS_BACKGROUND_SIZE_COVER;
        }
        else if (css_ieq(val, "contain"))
        {
            style->background_size.type =
                CSS_BACKGROUND_SIZE_CONTAIN;
        }
        else
        {
            char tokens[2][128];

            int count = css_tokenize_space_values(
                val,
                tokens,
                2);

            if (count >= 1)
            {
                style->background_size.type =
                    CSS_BACKGROUND_SIZE_EXPLICIT;

                style->background_size.width =
                    css_parse_length(tokens[0]);

                if (count >= 2)
                    style->background_size.height =
                        css_parse_length(tokens[1]);
            }
        }

        return;
    }

    if (css_ieq(prop, "background"))
    {
        css_apply_background_value(style, val);
        return;
    }
}

void css_apply_rule(
    CSSComputedStyle* style,
    const CSSRule* rule)
{
    if (!style || !rule)
        return;

    CSSDeclaration* declaration = rule->declarations;

    while (declaration)
    {
        css_apply_declaration(
            style,
            declaration->property,
            declaration->value);

        declaration = declaration->next;
    }
}

/* ============================================================
   CSS declaration parser
   ============================================================ */

static const char* css_find_top_level_char(
    const char* start,
    char target)
{
    int parentheses = 0;
    int brackets = 0;
    int quote = 0;

    for (const char* p = start; *p; ++p)
    {
        if (quote)
        {
            if (*p == quote && (p == start || p[-1] != '\\'))
                quote = 0;

            continue;
        }

        if (*p == '"' || *p == '\'')
        {
            quote = *p;
            continue;
        }

        if (*p == '(')
        {
            ++parentheses;
            continue;
        }

        if (*p == ')' && parentheses > 0)
        {
            --parentheses;
            continue;
        }

        if (*p == '[')
        {
            ++brackets;
            continue;
        }

        if (*p == ']' && brackets > 0)
        {
            --brackets;
            continue;
        }

        if (*p == target && parentheses == 0 && brackets == 0)
            return p;
    }

    return NULL;
}

static const char* css_find_block_end(
    const char* open)
{
    int depth = 1;
    int quote = 0;

    for (const char* p = open + 1; *p; ++p)
    {
        if (quote)
        {
            if (*p == quote && p[-1] != '\\')
                quote = 0;

            continue;
        }

        if (*p == '"' || *p == '\'')
        {
            quote = *p;
            continue;
        }

        if (*p == '/' && p[1] == '*')
        {
            p += 2;

            while (*p && !(p[0] == '*' && p[1] == '/'))
                ++p;

            if (!*p)
                return NULL;

            ++p;
            continue;
        }

        if (*p == '{')
        {
            ++depth;
        }
        else if (*p == '}')
        {
            --depth;

            if (depth == 0)
                return p;
        }
    }

    return NULL;
}

static void css_parse_declaration_block(
    CSSRule* rule,
    const char* begin,
    const char* end)
{
    const char* p = begin;

    while (p < end)
    {
        p = css_skip_whitespace(p);

        if (p >= end)
            break;

        const char* semicolon = NULL;
        const char* colon = NULL;

        int parentheses = 0;
        int brackets = 0;
        int quote = 0;

        for (const char* scan = p; scan < end; ++scan)
        {
            char c = *scan;

            if (quote)
            {
                if (c == quote && (scan == p || scan[-1] != '\\'))
                    quote = 0;

                continue;
            }

            if (c == '"' || c == '\'')
            {
                quote = c;
                continue;
            }

            if (c == '(')
            {
                ++parentheses;
                continue;
            }

            if (c == ')' && parentheses > 0)
            {
                --parentheses;
                continue;
            }

            if (c == '[')
            {
                ++brackets;
                continue;
            }

            if (c == ']' && brackets > 0)
            {
                --brackets;
                continue;
            }

            if (c == ':' &&
                parentheses == 0 &&
                brackets == 0 &&
                !colon)
            {
                colon = scan;
                continue;
            }

            if (c == ';' &&
                parentheses == 0 &&
                brackets == 0)
            {
                semicolon = scan;
                break;
            }
        }

        const char* piece_end =
            semicolon ? semicolon : end;

        if (colon && colon < piece_end)
        {
            const char* prop_begin = p;
            const char* prop_end = colon;

            while (prop_begin < prop_end &&
                   isspace((unsigned char)*prop_begin))
                ++prop_begin;

            while (prop_end > prop_begin &&
                   isspace((unsigned char)prop_end[-1]))
                --prop_end;

            const char* value_begin = colon + 1;
            const char* value_end = piece_end;

            while (value_begin < value_end &&
                   isspace((unsigned char)*value_begin))
                ++value_begin;

            while (value_end > value_begin &&
                   isspace((unsigned char)value_end[-1]))
                --value_end;

            if (prop_end > prop_begin &&
                value_end >= value_begin)
            {
                size_t prop_len =
                    (size_t)(prop_end - prop_begin);

                size_t value_len =
                    (size_t)(value_end - value_begin);

                char* property =
                    css_strndup(prop_begin, prop_len);

                char* value =
                    css_strndup(value_begin, value_len);

                if (property && value)
                {
                    css_trim_inplace(property);
                    css_trim_inplace(value);

                    css_add_declaration(
                        rule,
                        property,
                        value);
                }

                free(property);
                free(value);
            }
        }

        if (!semicolon)
            break;

        p = semicolon + 1;
    }
}

/* ============================================================
   Rule parser
   ============================================================ */

const char* css_parse_rule(
    const char* source,
    CSSRule** out_rule)
{
    if (!source || !out_rule)
        return NULL;

    *out_rule = NULL;

    const char* p = css_skip_whitespace(source);

    if (!*p)
        return NULL;

    const char* open =
        css_find_top_level_char(p, '{');

    if (!open)
        return NULL;

    const char* close =
        css_find_block_end(open);

    if (!close)
        return NULL;

    const char* selector_begin = p;
    const char* selector_end = open;

    while (selector_begin < selector_end &&
           isspace((unsigned char)*selector_begin))
        ++selector_begin;

    while (selector_end > selector_begin &&
           isspace((unsigned char)selector_end[-1]))
        --selector_end;

    if (selector_end <= selector_begin)
        return close + 1;

    char* selector =
        css_strndup(
            selector_begin,
            (size_t)(selector_end - selector_begin));

    if (!selector)
        return NULL;

    css_trim_inplace(selector);

    CSSRule* rule = css_create_rule(selector);

    free(selector);

    if (!rule)
        return NULL;

    css_parse_declaration_block(
        rule,
        open + 1,
        close);

    *out_rule = rule;

    return close + 1;
}

/* ============================================================
   Stylesheet parser
   ============================================================ */

CSSRule* css_parse_stylesheet(const char* source)
{
    if (!source)
        return NULL;

    CSSRule* head = NULL;
    CSSRule* tail = NULL;

    const char* cursor = source;

    while (*cursor)
    {
        cursor = css_skip_whitespace(cursor);

        if (!*cursor)
            break;

        /*
         * Handle standalone @charset / @import / similar
         * statements that don't have a declaration block.
         */
        if (*cursor == '@')
        {
            const char* semicolon =
                css_find_top_level_char(cursor, ';');

            const char* brace =
                css_find_top_level_char(cursor, '{');

            if (semicolon &&
                (!brace || semicolon < brace))
            {
                cursor = semicolon + 1;
                continue;
            }
        }

        CSSRule* rule = NULL;

        const char* next =
            css_parse_rule(cursor, &rule);

        if (!next)
            break;

        cursor = next;

        if (!rule)
            continue;

        if (!head)
        {
            head = rule;
            tail = rule;
        }
        else
        {
            tail->next = rule;
            tail = rule;
        }
    }

    return head;
}
