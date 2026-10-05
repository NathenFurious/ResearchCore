#ifndef CSS_H
#define CSS_H

#include <stddef.h>

// Represents a CSS rule (e.g., h1 { color: red; })
typedef struct CSSRule {
    char selector[32];
    char property[32];
    char value[32];
    struct CSSRule* next;
} CSSRule;

// Function declarations
CSSRule* css_create_rule(const char* selector, const char* property, const char* value);
void css_free_rules(CSSRule* head);
const char* parse_css_rule(const char* cursor, char* out_selector, char* out_prop, char* out_val);

#endif // CSS_H
