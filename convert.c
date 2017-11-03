#include "convert.h"

static int should_skip_token(char *token) {
  int i;
  for (i = 0; i < TOK_SKIP_COUNT; i ++) {
    if (!strcmp(token, TOKEN_SKIPS[i])) return 1;
  }
  return 0;
}

static void append(char *line, char *token, int needs_space) {
  strcat(line, token);
  if (needs_space) {
    int len = strlen(line);
    line[len] = ' ';
    line[len + 1] = '\0';
  }
}

// This function *directly modifies* its inputs.
// It returns 1 if the 'next' token was modified and 0 otherwise.
static int fix_asterisks(char *current, char **next) {
  // We are pointing at the last character in the string
  int tok_len = strlen(current);
  int asterisk_index = tok_len - 1;
  char *modified = *next;
  // Check the current token for trailing asterisks
  // to print here
  while (current[asterisk_index] == '*') {
    current[tok_len - 1] = ' ';
    current[tok_len] = '*';
    current[tok_len + 1] = '\0';
    tok_len += 2;
    asterisk_index --;
  }

  // Check the function name for leading asterisks
  // to print here, incrementing the token start as needed
  while (**next == '*') {
    current[tok_len ++] = ' ';
    current[tok_len ++] = '*';
    (*next) ++;
  }
  current[tok_len] = '\0';

  return (modified != *next);
}
// This function is just used to parse and print the relevant pieces of function signatures
static JsonNode* parse_function_signature(char **token_ptrs, tok_type *types, int count) {
  JsonNode *func = json_mkobject();
  int last_start, i = 0;
  char str[1000];
  str[0] = '\0';

  while (types[i] != FUNC_NAME) i ++;

  // We iterate from the 1st token up to the FUNC_NAME (all return type tokens)
  for (last_start = 0; last_start < i; last_start ++) {
    if (types[last_start] != TYPE) {
      fprintf(stderr, "Error in parsing function `%s` - expected `%s` to be a type",
          token_ptrs[i], token_ptrs[last_start]);
    }

    // Parse a type, which may consist of multiple words
    if (!should_skip_token(token_ptrs[last_start])) {
      append(str, token_ptrs[last_start], last_start < i - 1);
    }
  }

  // Retrieve any leading asterisks from the function name to append to our return
  fix_asterisks(str, token_ptrs + i);
  json_append_member(func, "function", json_mkstring(token_ptrs[i]));
  json_append_member(func, "return_type", json_mkstring(str));

  last_start = ++ i;
  // If there is only one token in the arguments, it must be `void`, which we can
  // safely ignore
  if (i >= count - 1) return func;

  JsonNode *args = json_mkarray();

  for (; i < count; i ++) {
    if (types[i] != VAR_NAME) continue;

    str[0] = '\0';

    // Concatenate all the types for this argument
    for (; last_start < i; last_start ++) {
      append(str, token_ptrs[last_start], last_start < i - 1);
    }
    // Check the next token for leading asterisks
    // to print here, incrementing the token start as needed
    // (though in this case, we do not actually care about the next token)
    fix_asterisks(str, token_ptrs + i);

    // We increment `last_start` so that it is not pointing to this token
    // (a VAR_NAME)
    last_start ++;
    json_append_element(args, json_mkstring(str));
  }
  json_append_member(func, "arguments", args);
  return func;
}

// TODO non-structs are not set up right now
static JsonNode* parse_struct(char **token_ptrs, tok_type *token_types, int count) {
  JsonNode *obj, *args, *pair;
  obj = json_mkobject();
  args = json_mkarray();
  int asterisk_ind, j = 0;
  char str[1000];
  str[0] = '\0';
  while (*token_ptrs[j] != '{') j ++;
  // TODO can improve this
  j ++;
  for (; j < count - 3; j ++) {

    // This member is fully described, save possible asterisks
    if (token_types[j] == VAR_NAME) {
      // We initialize this to the last character to overwrite
      // the trailing space
      // TODO update this to use the `fix_asterisks` function
      asterisk_ind = strlen(str) - 1;
      if(*token_ptrs[j] == '*') {
        // But if we actually have asterisks, we want that space - they will
        // then print out as though they are separate tokens in a type
        // (although if we want to manipulate each type token, this is not sufficient)
        while(*token_ptrs[j] == '*') {
          str[++ asterisk_ind]= '*';
          str[++ asterisk_ind] = ' ';
          token_ptrs[j] ++;
        }
      }
      str[asterisk_ind] = '\0';

      // TODO Can this be done outside?
      pair = json_mkobject();
      json_append_member(pair, token_ptrs[j], json_mkstring(str));
      json_append_element(args, pair);
      str[0] = '\0';
    } else {
      if (should_skip_token(token_ptrs[j])) continue;
      // Accumulate the type pieces of the member
      strcat(str, token_ptrs[j]);
      strcat(str, " ");
    }
  }
  json_append_member(obj, "type_name", json_mkstring(token_ptrs[count - 1]));
  json_append_member(obj, "members", args);

  return obj;
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

static int process_input(FILE *header, JsonNode *json) {
  int index = 0, count = 0;
  char **token_ptrs = malloc(100 * sizeof(char*));
  tok_type *token_types = malloc(100 * sizeof(tok_type));
  char *tokens = malloc(10000 * sizeof(char));
  char cur;

  JsonNode *ret, *new_struct;
  JsonNode *funcs = NULL, *structs = NULL;

  int i;
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
        tokens[-- index] = '\0';
        in_tok = 0;
        while((cur = fgetc(header)) != '\n') {}
      }

      // Ignore appropriate lines
      if (SHOULD_SKIP_LINE(token_ptrs[0])) {
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
      // Not necessarily true, but we have a list of tokens (extern, inline, etc) that will be skipped later
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

    // These cases need to be separated if nested braces are possible
    if (cur == '{' || cur == '}') {
      in_braces = !in_braces;
      tokens[index] = cur;
      index ++;
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);
    }

    if (cur == ';') {
      if (in_tok) increment_tokens(tokens, &index, &count, &in_tok);

      // if the only param for a function is 'void'
      if (token_types[count - 2] == FUNC_NAME) token_types[count - 1] = TYPE;

      // TODO Always true? I believe so.
      token_types[count - 1] = VAR_NAME;
      // We will probably need more conditionals on this case
      if (in_func) {
        ret = parse_function_signature(token_ptrs, token_types, count);
        if (funcs == NULL) {
          funcs = json_mkarray();
        }
        json_append_element(funcs, ret);
        in_tok = in_func = in_braces = index = count = 0;
      }
      if (in_braces > 0) {
        continue;
      } else if (!strcmp(token_ptrs[0], "typedef")
          || !strcmp(token_ptrs[0], "enum")
          || !strcmp(token_ptrs[0], "struct")) {
        // This is a user definition
        new_struct = parse_struct(token_ptrs, token_types, count);
        // printf("%s\n", token_ptrs[count - 1]);
        if (structs == NULL) structs = json_mkarray();
        json_append_element(structs, new_struct);
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
  if (funcs) json_append_member(json, "Functions", funcs);
  if (structs) json_append_member(json, "Structs", structs);

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

  JsonNode *json = json_mkobject();

  process_input(header, json);

  char *json_out = json_encode(json);

  // Prints tradtional compressed JSON
  printf("%s\n", json_out);

  // Prints JSON with whitespace
  printf("%s\n", json_stringify(json, " "));

  fclose(header);

  return 0;
}
