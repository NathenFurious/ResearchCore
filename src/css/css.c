#include "css.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CSSRule* css_create_rule(const char* selector, const char* property, const char* value) {
    CSSRule* rule = (CSSRule*)malloc(sizeof(CSSRule));
    if (rule == NULL) return NULL;

    strncpy(rule->selector, selector, sizeof(rule->selector) - 1);
    rule->selector[sizeof(rule->selector) - 1] = '\0';

    strncpy(rule->property, property, sizeof(rule->property) - 1);
    rule->property[sizeof(rule->property) - 1] = '\0';

    strncpy(rule->value, value, sizeof(rule->value) - 1);
    rule->value[sizeof(rule->value) - 1] = '\0';

    rule->next = NULL;
    return rule;
}

void css_free_rules(CSSRule* head) {
    while (head != NULL) {
        CSSRule* temp = head;
        head = head->next;
        free(temp);
    }
}

const char* parse_css_rule(const char* cursor, char* out_selector, char* out_prop, char* out_val) {
    // Skip leading whitespace
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') cursor++;
    if (*cursor == '\0') return NULL;

    // 1. Read selector up to '{'
    const char* start = cursor;
    while (*cursor != '\0' && *cursor != '{' && *cursor != ' ') cursor++;
    size_t len = (size_t)(cursor - start);
    if (len >= 32) len = 31;
    strncpy(out_selector, start, len);
    out_selector[len] = '\0';

    // Move past '{'
    while (*cursor != '\0' && *cursor != '{') cursor++;
    if (*cursor == '{') cursor++;

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') cursor++;

    // 2. Read property up to ':'
    start = cursor;
    while (*cursor != '\0' && *cursor != ':' && *cursor != ' ') cursor++;
    len = (size_t)(cursor - start);
    if (len >= 32) len = 31;
    strncpy(out_prop, start, len);
    out_prop[len] = '\0';

    // Move past ':'
    while (*cursor != '\0' && *cursor != ':') cursor++;
    if (*cursor == ':') cursor++;

    // Skip whitespace
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n') cursor++;

    // 3. Read value up to ';' or '}'
    start = cursor;
    while (*cursor != '\0' && *cursor != ';' && *cursor != '}') cursor++;
    len = (size_t)(cursor - start);
    if (len >= 32) len = 31;
    strncpy(out_val, start, len);
    out_val[len] = '\0';

    // Advance past closing delimiter
    while (*cursor != '\0' && *cursor != '}') cursor++;
    return (*cursor == '}') ? cursor + 1 : cursor;
}
