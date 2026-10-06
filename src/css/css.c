#include "css.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CSS_MAX_TOKEN 256

static int css_is_whitespace(char c) {
    return c == ' ' ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\f';
}

static const char* css_skip_whitespace(const char* p) {
    while (*p && css_is_whitespace(*p)) {
        p++;
    }

    return p;
}

static const char* css_skip_comment(const char* p) {
    if (p[0] != '/' || p[1] != '*') {
        return p;
    }

    p += 2;

    while (*p) {
        if (p[0] == '*' && p[1] == '/') {
            return p + 2;
        }

        p++;
    }

    return p;
}

static const char* css_skip_space_and_comments(const char* p) {
    for (;;) {
        const char* next = css_skip_whitespace(p);

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

static size_t css_trim_length(const char* start, const char* end) {
    while (start < end && css_is_whitespace(*start)) {
        start++;
    }

    while (end > start && css_is_whitespace(end[-1])) {
        end--;
    }

    return (size_t)(end - start);
}

static void css_copy_range(
    char* destination,
    size_t destination_size,
    const char* start,
    const char* end
) {
    if (!destination || destination_size == 0) {
        return;
    }

    size_t length = css_trim_length(start, end);

    if (length >= destination_size) {
        length = destination_size - 1;
    }

    memcpy(destination, start, length);
    destination[length] = '\0';
}

static const char* css_find_block_start(const char* p) {
    int quote = 0;
    int escape = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    while (*p) {
        char c = *p;

        if (escape) {
            escape = 0;
            p++;
            continue;
        }

        if (c == '\\') {
            escape = 1;
            p++;
            continue;
        }

        if (quote) {
            if (c == quote) {
                quote = 0;
            }

            p++;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            p++;
            continue;
        }

        if (c == '/' && p[1] == '*') {
            p = css_skip_comment(p);
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
        } else if (
            c == '{' &&
            paren_depth == 0 &&
            bracket_depth == 0
        ) {
            return p;
        }

        p++;
    }

    return NULL;
}

static const char* css_find_declaration_end(const char* p) {
    int quote = 0;
    int escape = 0;
    int paren_depth = 0;
    int bracket_depth = 0;

    while (*p) {
        char c = *p;

        if (escape) {
            escape = 0;
            p++;
            continue;
        }

        if (c == '\\') {
            escape = 1;
            p++;
            continue;
        }

        if (quote) {
            if (c == quote) {
                quote = 0;
            }

            p++;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            p++;
            continue;
        }

        if (c == '/' && p[1] == '*') {
            p = css_skip_comment(p);
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
                if (paren_depth == 0 && bracket_depth == 0) {
                    return p;
                }
                break;

            case '}':
                if (paren_depth == 0 && bracket_depth == 0) {
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

static const char* css_find_colon(const char* start, const char* end) {
    int quote = 0;
    int escape = 0;
    int bracket_depth = 0;
    int paren_depth = 0;

    for (const char* p = start; p < end && *p; p++) {
        char c = *p;

        if (escape) {
            escape = 0;
            continue;
        }

        if (c == '\\') {
            escape = 1;
            continue;
        }

        if (quote) {
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

        if (c == ')' && paren_depth > 0) {
            paren_depth--;
            continue;
        }

        if (c == '[') {
            bracket_depth++;
            continue;
        }

        if (c == ']' && bracket_depth > 0) {
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

static void css_remove_important(char* value) {
    size_t length = strlen(value);

    while (length > 0 &&
           css_is_whitespace(value[length - 1])) {
        value[--length] = '\0';
    }

    const char* important = "!important";
    size_t important_length = strlen(important);

    if (length < important_length) {
        return;
    }

    size_t start = length - important_length;

    if (strncasecmp(
            value + start,
            important,
            important_length
        ) == 0) {

        if (start == 0 ||
            css_is_whitespace(value[start - 1])) {

            value[start] = '\0';

            while (start > 0 &&
                   css_is_whitespace(value[start - 1])) {
                value[--start] = '\0';
            }
        }
    }
}

CSSRule* css_create_rule(
    const char* selector,
    const char* property,
    const char* value
) {
    CSSRule* rule = malloc(sizeof(*rule));

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

void css_free_rules(CSSRule* head) {
    while (head) {
        CSSRule* next = head->next;
        free(head);
        head = next;
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

    cursor = css_skip_space_and_comments(cursor);

    if (!*cursor) {
        return NULL;
    }

    /*
     * Read selector / at-rule prelude.
     */
    const char* selector_start = cursor;
    const char* block_start = css_find_block_start(cursor);

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

    /*
     * Read the first declaration.
     */
    cursor = css_skip_space_and_comments(cursor);

    if (!*cursor || *cursor == '}') {
        return (*cursor == '}') ? cursor + 1 : cursor;
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
        /*
         * Not a normal declaration.
         * Advance safely through the block.
         */
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

    css_remove_important(out_val);

    if (*declaration_end == ';') {
        return declaration_end + 1;
    }

    if (*declaration_end == '}') {
        return declaration_end + 1;
    }

    return declaration_end;
}
