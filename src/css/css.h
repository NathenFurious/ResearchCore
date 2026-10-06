#ifndef CSS_H
#define CSS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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

    CSSBackgroundLayer backgrounds[
        CSS_MAX_BACKGROUND_LAYERS
    ];

    size_t background_count;
} CSSComputedStyle;

/*
 * Parsed CSS declaration.
 */
typedef struct CSSDeclaration {
    char* property;
    char* value;

    struct CSSDeclaration* next;
} CSSDeclaration;

/*
 * Parsed CSS rule.
 *
 * Example:
 *
 *     .box {
 *         width: 100px;
 *         color: red;
 *     }
 *
 * selector:
 *     ".box"
 *
 * declarations:
 *     width -> 100px
 *     color -> red
 */
typedef struct CSSRule {
    char* selector;
    CSSDeclaration* declarations;

    struct CSSRule* next;
} CSSRule;

/*
 * Create an empty CSS rule.
 */
CSSRule* css_create_rule(
    const char* selector
);

/*
 * Add a declaration to a rule.
 */
CSSDeclaration* css_add_declaration(
    CSSRule* rule,
    const char* property,
    const char* value
);

/*
 * Free one rule and every declaration belonging
 * to that rule.
 */
void css_free_rule(
    CSSRule* rule
);

/*
 * Free an entire linked list of rules.
 */
void css_free_rules(
    CSSRule* head
);

/*
 * Parse one CSS rule from source.
 *
 * The parser advances through the stylesheet and
 * returns the position where the next rule begins.
 *
 * out_rule receives a newly allocated CSSRule.
 *
 * Example:
 *
 *     .box {
 *         width: 100px;
 *         color: red;
 *     }
 *
 *     #main {
 *         height: 50vh;
 *     }
 *
 * The first call returns a pointer to "#main".
 */
const char* css_parse_rule(
    const char* source,
    CSSRule** out_rule
);

/*
 * Initialize a computed style with CSS initial values.
 */
void css_style_initial(
    CSSComputedStyle* style
);

/*
 * Apply one parsed CSS declaration to a computed style.
 */
void css_apply_declaration(
    CSSComputedStyle* style,
    const char* property,
    const char* value
);

/*
 * Apply every declaration in a CSS rule.
 */
void css_apply_rule(
    CSSComputedStyle* style,
    const CSSRule* rule
);

/*
 * Parse a complete stylesheet into a linked list.
 *
 * Returns the first rule, or NULL if no rules
 * could be parsed.
 */
CSSRule* css_parse_stylesheet(
    const char* source
);

/*
 * Parse a CSS length such as:
 *
 *     10px
 *     50%
 *     2em
 *     1.5rem
 *     10vw
 *     20vh
 *     auto
 */
int css_parse_length(
    const char* text,
    CSSLength* result
);

/*
 * Parse CSS colors:
 *
 *     red
 *     #fff
 *     #ffffff
 *     #ffffffff
 *     rgb(...)
 *     rgba(...)
 *     transparent
 */
int css_parse_color(
    const char* text,
    CSSColor* result
);

/*
 * Convert a CSS length into pixels.
 *
 * containing_size is used for percentage values.
 * font_size is used for em.
 * root_font_size is used for rem.
 * viewport_width / viewport_height are used for
 * viewport units.
 */
float css_length_to_px(
    CSSLength length,
    float containing_size,
    float font_size,
    float root_font_size,
    float viewport_width,
    float viewport_height
);

#ifdef __cplusplus
}
#endif

#endif /* CSS_H */
