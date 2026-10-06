#ifndef CSS_H
#define CSS_H

#include <stddef.h>

typedef struct CSSDeclaration {
    char* property;
    char* value;
    struct CSSDeclaration* next;
} CSSDeclaration;

typedef struct CSSRule {
    char* selector;
    CSSDeclaration* declarations;
    struct CSSRule* next;
} CSSRule;

CSSRule* css_create_rule(const char* selector);

CSSDeclaration* css_add_declaration(
    CSSRule* rule,
    const char* property,
    const char* value
);

void css_free_rule(CSSRule* rule);

void css_free_rules(CSSRule* head);

const char* css_parse_rule(
    const char* source,
    CSSRule** out_rule
);

#endif /* CSS_H */
