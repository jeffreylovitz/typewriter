#ifndef CONVERT_H
#define CONVERT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lib/json.h"
#define WHITESPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\t')

typedef enum {UNKNOWN, TYPE, DEF, FUNC_NAME, VAR_NAME} tok_type;
const char *LINE_SKIPS[] = {"#include", "#define"};
#define SHOULD_SKIP_LINE(token) (!strcmp((token), LINE_SKIPS[0]) || !strcmp((token), LINE_SKIPS[1]))

#define TOK_SKIP_COUNT 3
const char *TOKEN_SKIPS[] = {"static", "inline", "extern"};

#endif