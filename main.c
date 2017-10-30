#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE(c) ((c) == ' ' || (c) == '\n' || (c) == '\r' || (c) == '\t')

typedef enum {UNKNOWN, TYPE, FUNC_NAME, VAR_NAME} tok_type;

static int scan_tokens(char **token_ptrs, tok_type *types, int count) {
  int i;

  for (i = 0; i < count; i ++) {
    // printf("tok %d: %s\n", i, token_ptrs[i]);
    if (types[i] == TYPE) {
      printf("%s", token_ptrs[i]);
      if ((i < count - 1) && (types[i + 1] != TYPE)) {
        // Check the next token for leading asterisks
        // to print here, incrementing the token start as needed
        while (token_ptrs[i + 1][0] == '*') {
          printf("*");
          token_ptrs[i + 1] ++;
        }
      }
      // The last type will not need a trailing space
      if (i < count - 2) {
        if (types[i + 1] == VAR_NAME) printf(",");
        printf(" ");
      }
    } else if (types[i] == FUNC_NAME) {
      printf("%s(", token_ptrs[i]);
    }
  }
  printf(")\n");

  return 0;
}

/*
 * Read the file stream (which may be stdin or a passed argument)
 * character by character and add all tokens to a `\0`-delimited
 * string, updating an array of sequential char pointers as well,
 * until the end of a declaration has been reached.
 * Struct declarations end when their outermost curly brackets
 * have been closed AND a semicolon is encountered;
 * function declarations end after a semicolon is encountered.
 *
 * Iterate over all tokens and build the appropriate output.
 */

static inline void increment_tokens(char *tokens, int *index, int *count, int *in_tok) {
  tokens[*index] = '\0';
  (*index) ++;
  (*count) ++;
  *in_tok = 0;
}
// Going to start by just trying to convert function declarations

// Skip all commented code
// Deal with type-var distinction of expressions like `char *x`
int main(int argc, char **argv) {
  FILE *header;
  if (argc == 2) {
    header = fopen(argv[1], "r");
  } else if (argc == 1) {
    header = stdin;
  }
  if (header == NULL || argc > 2) {
    fprintf(stderr, "usage: %s [headerfile] or %s < [headerfile]\n", argv[0], argv[0]);
    exit(1);
  }
  int rc, index, count;
  index = count = 0;
  char **token_ptrs = malloc(100 * sizeof(char*));
  tok_type *token_types = malloc(100 * sizeof(tok_type));
  char *tokens = malloc(5000 * sizeof(char));
  char cur;

  /* enum tok_type{TYPE, FUNC_NAME, VAR_NAME} tok_types; */
  int i;
  int in_tok = 0;
  while ((cur = fgetc(header)) != EOF) {

    if (WHITESPACE(cur)) {
      if (in_tok) {
        // We will correct this assumption for names when we encounter a comma or paren
        token_types[count] = TYPE;
        increment_tokens(tokens, &index, &count, &in_tok);
      }
      // Whitespace does not matter between tokens
      continue;
    };

    if (cur == '(') {
      // If there was whitespace between function name and opening paren,
      // we won't be in_tok - this should standardize behavior
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
      // We'll assume that the token before this parenthesis is the function name.
      // Faulty assumption - parens for casts, etc
      token_types[count - 1] = FUNC_NAME;
      // untrue with inline, extern, etc
      for (i = 0; i < count - 1; i ++) token_types[i] = TYPE;
      continue;
    }

    if (cur == ',' || cur == ')') {
      // If there was whitespace between this char and previous token,
      // we won't be in_tok - this should standardize behavior
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
      token_types[count - 1] = VAR_NAME;
      // untrue with inline, extern, etc
      continue;
    }

    if (cur == ';') {
      // This should never be the case for function signatures or structs,
      // but will happen when we deal with variables
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);

      // if the only param for a function is 'void'
      if (token_types[count - 2] == FUNC_NAME) token_types[count - 1] = TYPE;
      // This case will be more complex when we're looking at nested structures
      rc = scan_tokens(token_ptrs, token_types, count);
      if (rc) fprintf(stderr, "Error processing a series of tokens.\n");
      index = count = 0;
      continue;
    }

    // Part of valid token
    if (!in_tok) {
      in_tok = !in_tok;
      token_ptrs[count] = tokens + index;
    }
    tokens[index] = cur;
    index ++;

  }

  fclose(header);
  free(token_ptrs);
  free(tokens);
  return 0;
}