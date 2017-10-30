#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE(c) ((c) == ' ' || (c) == '\r' || (c) == '\t')

typedef enum {UNKNOWN, TYPE, DEF, FUNC_NAME, VAR_NAME} tok_type;
const char *LINE_SKIPS[] = {"#include", "#define"};
#define SHOULD_SKIP(token) (!strcmp((token), LINE_SKIPS[0]) || !strcmp((token), LINE_SKIPS[1]))

// This function is just used to parse and print the relevant pieces of function signatures
static int process_function_signature(char **token_ptrs, tok_type *types, int count) {
  int i;
  for (i = 0; i < count; i ++) {
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

static int process_input(FILE *header) {
  int rc, index, count;
  index = count = 0;
  char **token_ptrs = malloc(100 * sizeof(char*));
  tok_type *token_types = malloc(100 * sizeof(tok_type));
  char *tokens = malloc(5000 * sizeof(char));
  char cur;

  int i, j;
  int in_tok = 0, in_func = 0, in_braces = 0;
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

    if (cur == '\n' || (index > 0 && cur == '/' && tokens[index - 1] == '/')) {
      // We have probably reached an EOL or a comment. As we are only interested in functions and type
      // definitions, the previous non-whitespace character must be ';', '{', or '\'. All
      // will cause different behaviors.

      // This might be a blank line, or we may have just finished processing a function signature.
      if (index == 0) continue;
      // Consume all characters to EOL if we have encountered a comment
      if (cur == '/' && tokens[index - 1] == '/') {
        tokens[index - 1] = '\0';
        while((cur = fgetc(header)) != '\n') {}
      }

      // Ignore appropriate lines
      if (SHOULD_SKIP(token_ptrs[0])) {
        in_tok = in_func = in_braces = index = count = 0;
        continue;
      }

      // If we read an escape character or are between braces (in a type definition),
      // we should just continue as normal
      if (tokens[index - 1] == '\\' || in_braces > 0) {
        // Escape character
        continue;
      } else {
        // This was a line we did not care about: a global variable, just a comment, etc.
        in_tok = in_func = in_braces = index = count = 0;
        continue;
      }
    }

    // Analyze the current character
    if (cur == '(') {
      // If there was whitespace between function name and opening paren,
      // we won't be in_tok - this should standardize behavior
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
      // We'll assume that the token before this parenthesis is the function name.
      // Faulty assumption - parens for casts, etc
      token_types[count - 1] = FUNC_NAME;
      in_func = 1;
      // TODO untrue with inline, extern, etc - we should just check the first few tokens
      // before printing and skip them until we find a real TYPE. or can overwrite them
      // in the arrays here
      for (i = 0; i < count - 1; i ++) token_types[i] = TYPE;
      continue;
    }

    if (cur == ',' || cur == ')') {
      // If there was whitespace between this char and previous token,
      // we won't be in_tok - this should standardize behavior
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
      token_types[count - 1] = VAR_NAME;
      continue;
    }

    if (cur == '{') {
      in_braces ++;
      // Currently treating braces as part of token - probably not a great long-term solution
      tokens[index] = cur;
      index ++;
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
    }

    if (cur == '}') {
      in_braces --;
      // Currently treating braces as part of token - probably not a great long-term solution
      tokens[index] = cur;
      index ++;
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
    }

    if (cur == ';') {
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);

      // if the only param for a function is 'void'
      if (token_types[count - 2] == FUNC_NAME) token_types[count - 1] = TYPE;

      // TODO Always true?  I believe so.
      token_types[count - 1] = VAR_NAME;
      // We will probably need more conditionals on this case
      if (in_func) {
        rc = process_function_signature(token_ptrs, token_types, count);
        if (rc) fprintf(stderr, "Error processing a series of tokens.\n");
        in_tok = in_func = in_braces = index = count = 0;
      }
      if (in_braces > 0) {
        continue;
      } else if (!strcmp(token_ptrs[0], "typedef")
              || !strcmp(token_ptrs[0], "enum")
              || !strcmp(token_ptrs[0], "struct")) {
        for (j = 0; j < count - 1; j ++) {
          printf("%s", token_ptrs[j]);
          // TODO Separating elements of structs
          if (token_types[j] == VAR_NAME && j < count - 3) {
            printf(", ");
          } else {
            printf(" ");
          }
        }
        printf("%s\n", token_ptrs[count - 1]);
      }
      in_tok = in_func = in_braces = index = count = 0;
      continue;
    }

    // If we've gotten this far, we should create or append this character to a token
    if (!in_tok) {
      in_tok = !in_tok;
      token_ptrs[count] = tokens + index;
    }
    tokens[index] = cur;
    index ++;

  }

  free(token_ptrs);
  free(tokens);
  return 0;
}

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

  process_input(header);

  fclose(header);
  return 0;
}
