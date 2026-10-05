#ifndef DOM_H
#define DOM_H

#include <stddef.h>

typedef struct DOMNode {
    char tag_name[32];
    char* inner_text;
    struct DOMNode* parent;
    struct DOMNode* first_child;
    struct DOMNode* next_sibling;
} DOMNode;

// Function prototypes
DOMNode* dom_create_node(const char* tag_name);
void dom_free_node(DOMNode* node);
const char* parse_html_tag(const char* html_buffer, char* out_tag_name, size_t max_len);

#endif // DOM_H
