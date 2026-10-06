#include "dom.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static char* dom_strdup(const char* string) {
    if (!string) {
        return NULL;
    }

    size_t length = strlen(string);
    char* copy = malloc(length + 1);

    if (!copy) {
        return NULL;
    }

    memcpy(copy, string, length + 1);
    return copy;
}

static int is_html_space(char c) {
    return c == ' ' ||
           c == '\t' ||
           c == '\n' ||
           c == '\r' ||
           c == '\f';
}

DOMNode* dom_create_node(DOMNodeType type, const char* tag_name) {
    DOMNode* node = calloc(1, sizeof(*node));

    if (!node) {
        return NULL;
    }

    node->type = type;

    if (tag_name) {
        node->tag_name = dom_strdup(tag_name);

        if (!node->tag_name) {
            free(node);
            return NULL;
        }
    }

    return node;
}

DOMNode* dom_create_text(const char* text, size_t length) {
    DOMNode* node = calloc(1, sizeof(*node));

    if (!node) {
        return NULL;
    }

    node->type = DOM_TEXT;

    node->text = malloc(length + 1);

    if (!node->text) {
        free(node);
        return NULL;
    }

    memcpy(node->text, text, length);
    node->text[length] = '\0';

    return node;
}

void dom_append_child(DOMNode* parent, DOMNode* child) {
    if (!parent || !child) {
        return;
    }

    child->parent = parent;
    child->next_sibling = NULL;

    if (!parent->first_child) {
        parent->first_child = child;
        parent->last_child = child;
        return;
    }

    parent->last_child->next_sibling = child;
    parent->last_child = child;
}

void dom_free_node(DOMNode* node) {
    if (!node) {
        return;
    }

    DOMNode* child = node->first_child;

    while (child) {
        DOMNode* next = child->next_sibling;
        dom_free_node(child);
        child = next;
    }

    DOMAttribute* attribute = node->attributes;

    while (attribute) {
        DOMAttribute* next = attribute->next;

        free(attribute->name);
        free(attribute->value);
        free(attribute);

        attribute = next;
    }

    free(node->tag_name);
    free(node->text);

    free(node);
}

static const char* skip_html_space(const char* cursor) {
    while (*cursor && is_html_space(*cursor)) {
        cursor++;
    }

    return cursor;
}

static const char* skip_html_comment(const char* cursor) {
    if (cursor[0] != '<' ||
        cursor[1] != '!' ||
        cursor[2] != '-' ||
        cursor[3] != '-') {
        return cursor;
    }

    cursor += 4;

    while (*cursor) {
        if (cursor[0] == '-' &&
            cursor[1] == '-' &&
            cursor[2] == '>') {
            return cursor + 3;
        }

        cursor++;
    }

    return cursor;
}

const char* parse_html_tag(
    const char* cursor,
    char* out_tag_name,
    size_t max_len
) {
    if (!cursor || !out_tag_name || max_len == 0) {
        return NULL;
    }

    out_tag_name[0] = '\0';

    while (*cursor) {
        if (cursor[0] == '<') {
            break;
        }

        cursor++;
    }

    if (!*cursor) {
        return NULL;
    }

    /*
     * Skip comments instead of treating "<!--" as a tag.
     */
    if (cursor[1] == '!' &&
        cursor[2] == '-' &&
        cursor[3] == '-') {
        return skip_html_comment(cursor);
    }

    cursor++;

    /*
     * Ignore closing-tag slash.
     *
     * </body>
     *  ^
     */
    if (*cursor == '/') {
        cursor++;
    }

    cursor = skip_html_space(cursor);

    const char* start = cursor;

    /*
     * HTML tag names end at whitespace,
     * '/', or '>'.
     */
    while (*cursor &&
           !is_html_space(*cursor) &&
           *cursor != '/' &&
           *cursor != '>') {
        cursor++;
    }

    size_t length = (size_t)(cursor - start);

    if (length >= max_len) {
        length = max_len - 1;
    }

    memcpy(out_tag_name, start, length);
    out_tag_name[length] = '\0';

    /*
     * Scan to the end of the tag while respecting
     * quoted attribute values.
     */
    int quote = 0;

    while (*cursor) {
        char c = *cursor;

        if (quote) {
            if (c == quote) {
                quote = 0;
            }

            cursor++;
            continue;
        }

        if (c == '"' || c == '\'') {
            quote = c;
            cursor++;
            continue;
        }

        if (c == '>') {
            return cursor + 1;
        }

        cursor++;
    }

    return cursor;
}
