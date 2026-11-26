#ifndef PARSE_H_
#define PARSE_H_

#include "ast.h"

// Returns NULL on failure
ast_node *parse_from_string(arena *a, const char *text);

// Get the error that stopped the most recent parse attempt, if any
const char *parse_error();

#endif
