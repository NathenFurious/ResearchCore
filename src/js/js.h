#ifndef JS_H
#define JS_H

#include <stddef.h>

// Represents variable types in our basic JS engine
typedef enum {
    JS_TYPE_UNDEFINED,
    JS_TYPE_NUMBER,
    JS_TYPE_STRING
} JSType;

// Container for a runtime variable (e.g., let x = 10;)
typedef struct JSValue {
    JSType type;
    union {
        double number_val;
        char string_val[64];
    } as;
} JSValue;

// Environment state mapping variable names to values
typedef struct JSVariable {
    char name[32];
    JSValue value;
    struct JSVariable* next;
} JSVariable;

typedef struct JSEnv {
    JSVariable* vars;
} JSEnv;

// Function prototypes
JSEnv* js_create_env(void);
void js_free_env(JSEnv* env);

void js_set_var(JSEnv* env, const char* name, JSValue val);
JSValue js_get_var(JSEnv* env, const char* name);

// Evaluates simple expressions/statements (e.g., "let x = 42;", "x = x + 5;")
void js_eval(JSEnv* env, const char* code);

#endif // JS_H
