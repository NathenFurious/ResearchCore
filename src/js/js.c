#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

JSEnv* js_create_env(void) {
    JSEnv* env = (JSEnv*)malloc(sizeof(JSEnv));
    if (!env) return NULL;
    env->vars = NULL;
    return env;
}

void js_free_env(JSEnv* env) {
    if (!env) return;
    JSVariable* current = env->vars;
    while (current != NULL) {
        JSVariable* next = current->next;
        free(current);
        current = next;
    }
    free(env);
}

void js_set_var(JSEnv* env, const char* name, JSValue val) {
    if (!env) return;

    // Check if variable already exists in environment
    JSVariable* current = env->vars;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            current->value = val;
            return;
        }
        current = current->next;
    }

    // Allocate new variable node
    JSVariable* new_var = (JSVariable*)malloc(sizeof(JSVariable));
    if (!new_var) return;

    strncpy(new_var->name, name, sizeof(new_var->name) - 1);
    new_var->name[sizeof(new_var->name) - 1] = '\0';
    new_var->value = val;
    new_var->next = env->vars;
    env->vars = new_var;
}

JSValue js_get_var(JSEnv* env, const char* name) {
    JSValue undefined_val = { .type = JS_TYPE_UNDEFINED };
    if (!env) return undefined_val;

    JSVariable* current = env->vars;
    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current->value;
        }
        current = current->next;
    }
    return undefined_val;
}

void js_eval(JSEnv* env, const char* code) {
    if (!env || !code) return;

    const char* cursor = code;

    // Skip leading whitespace
    while (isspace(*cursor)) cursor++;

    // Look for 'var' or 'let' keywords
    if (strncmp(cursor, "var ", 4) == 0 || strncmp(cursor, "let ", 4) == 0) {
        cursor += 4;
        while (isspace(*cursor)) cursor++;

        // Read variable name
        char var_name[32];
        size_t len = 0;
        while (*cursor != '\0' && !isspace(*cursor) && *cursor != '=' && *cursor != ';') {
            if (len < sizeof(var_name) - 1) {
                var_name[len++] = *cursor;
            }
            cursor++;
        }
        var_name[len] = '\0';

        // Move cursor to '=' assignment
        while (*cursor != '\0' && *cursor != '=') cursor++;
        if (*cursor == '=') cursor++;
        while (isspace(*cursor)) cursor++;

        // Parse assigned value (Number or String)
        JSValue val;
        if (*cursor == '"' || *cursor == '\'') {
            char quote = *cursor++;
            val.type = JS_TYPE_STRING;
            size_t str_len = 0;
            while (*cursor != '\0' && *cursor != quote) {
                if (str_len < sizeof(val.as.string_val) - 1) {
                    val.as.string_val[str_len++] = *cursor;
                }
                cursor++;
            }
            val.as.string_val[str_len] = '\0';
        } else if (isdigit(*cursor) || *cursor == '-') {
            val.type = JS_TYPE_NUMBER;
            val.as.number_val = atof(cursor);
        } else {
            val.type = JS_TYPE_UNDEFINED;
        }

        js_set_var(env, var_name, val);
    }
}
