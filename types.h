#ifndef TYPES_H
#define TYPES_H

// Forward declaration of ast_t to avoid circular dependencies
// Actual definition is in parser.h
struct ast_t;

// Defines a function pointer type that takes an ast_t* and a void* context,
// and returns an ast_t*. This is used by the map_with_context combinator.
typedef struct ast_t* (*map_with_context_func)(struct ast_t* ast, void* context);

#endif // TYPES_H
