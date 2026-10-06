#ifndef DOM_H
#define DOM_H

#include <stddef.h>

typedef enum {
    DOM_DOCUMENT,
    DOM_ELEMENT,
    DOM_TEXT,
    DOM_COMMENT,
    DOM_DOCTYPE
} DOMNodeType;

typedef struct DOMAttribute {
    char* name;
    char* value;
    struct DOMAttribute* next;
} DOMAttribute;

typedef struct DOMNode {
    DOMNodeType type;

    char* tag_name;
    char* text;

    DOMAttribute* attributes;

    struct DOMNode* parent;
    struct DOMNode* first_child;
    struct DOMNode* last_child;
    struct DOMNode* next_sibling;
} DOMNode;

DOMNode* dom_create_node(
    DOMNodeType type,
    const char* tag_name
);

DOMNode* dom_create_text(
    const char* text,
    size_t length
);

void dom_append_child(
    DOMNode* parent,
    DOMNode* child
);

void dom_free_node(
    DOMNode* node
);

const char* parse_html_tag(
    const char* cursor,
    char* out_tag_name,
    size_t max_len
);

#endif /* DOM_H */
