#include "dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DOMNode* dom_create_node(const char* tag_name) {
    // Allocate heap space for a single DOMNode
    DOMNode* node = (DOMNode*)malloc(sizeof(DOMNode));
    if (node == NULL) {
        return NULL; // Memory allocation check
    }

    // Initialize fields
    strncpy(node->tag_name, tag_name, sizeof(node->tag_name) - 1);
    node->tag_name[sizeof(node->tag_name) - 1] = '\0';
    node->inner_text = NULL;
    node->parent = NULL;
    node->first_child = NULL;
    node->next_sibling = NULL;

    return node;
}

void dom_free_node(DOMNode* node) {
    if (node == NULL) return;

    if (node->inner_text != NULL) {
        free(node->inner_text);
    }
    
    // Recursively free child and sibling nodes
    dom_free_node(node->first_child);
    dom_free_node(node->next_sibling);

    free(node);
}

const char* parse_html_tag(const char* cursor, char* out_tag_name, size_t max_len) {
    // Scan until we find the opening '<'
    while (*cursor != '\0' && *cursor != '<') {
        cursor++;
    }

    if (*cursor == '\0') return NULL; // End of string reached

    cursor++; // Move past '<'
    const char* start = cursor;

    // Scan forward to find the closing '>'
    while (*cursor != '\0' && *cursor != '>' && *cursor != ' ') {
        cursor++;
    }

    // Calculate length using pointer subtraction
    size_t length = (size_t)(cursor - start);
    if (length >= max_len) length = max_len - 1;

    // Copy tag name into output buffer
    strncpy(out_tag_name, start, length);
    out_tag_name[length] = '\0';

    // Return the updated cursor address sitting at '>'
    while (*cursor != '\0' && *cursor != '>') {
        cursor++;
    }
    return (*cursor == '>') ? cursor + 1 : cursor;
}
