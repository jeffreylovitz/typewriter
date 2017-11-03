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

// This function is just used to parse and print the relevant pieces of function signatures
static JsonNode* process_function_signature(char **token_ptrs, tok_type *types, int count) {
  JsonNode *func = json_mkobject();
  JsonNode *args = NULL;
  int i;
  int last_start = 0;
  char str[1000];
  for (i = 0; i < count; i ++) {
    if (types[i] == FUNC_NAME) {
      json_append_member(func, "Function", json_mkstring(token_ptrs[i]));
      str[0] = '\0';
      for (; last_start < i; last_start ++) {
        while (types[last_start] == TYPE) {
          // Parse a type, which may consist of multiple words
          if (!should_skip_token(token_ptrs[last_start])) {
            append(str, token_ptrs[last_start], last_start < i - 1);
            // strcat(str, token_ptrs[last_start]);
            // strcat(str, " ");
          }
          // Can combine this shit with the loop
          last_start ++;
        }
      }
      // Skip trailing spaces
      str[strlen(str)] = '\0';
      // Check the next token for leading asterisks
      // to print here, incrementing the token start as needed
      while (*token_ptrs[i + 1] == '*') {
        strcat(str, "*");
        token_ptrs[i + 1] ++;
      }
      json_append_member(func, "return_type", json_mkstring(str));
      break;
    }
  }

  i ++;
  int types_start = i;
  // Do we have arguments passed to this function?
  if (i < count - 1) args = json_mkarray();
  for (; i < count; i ++) {
    str[0] = '\0';
    if (types[i] == VAR_NAME) {
      for (; types_start < i; types_start ++) {
        append(str, token_ptrs[types_start], types_start < i - 1);
        // strcat(str, token_ptrs[types_start]);
        // strcat(str, " ");
      }
      // Check the next token for leading asterisks
      // to print here, incrementing the token start as needed
      while (*token_ptrs[i] == '*') {
        strcat(str, "*");
        token_ptrs[i] ++;
      }
      if (args) json_append_element(args, json_mkstring(str));
    }
  }
  if (args) json_append_member(func, "arguments", args);
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
      asterisk_ind = strlen(str) - 1;
      while(*token_ptrs[j] == '*') {
        str[asterisk_ind] = '*';
        asterisk_ind ++;
        token_ptrs[j] ++;
      }
      str[asterisk_ind] = '\0';

      // Can this be done outside?
      pair = json_mkobject();
      json_append_member(pair, token_ptrs[j], json_mkstring(str));
      json_append_element(args, pair);
      str[0] = '\0';
      continue;
    } else {
      if (should_skip_token(token_ptrs[j])) continue;
      // Accumulate the type pieces of the member
      strcat(str, token_ptrs[j]);
      strcat(str, " ");
    }
  }
  json_append_member(obj, "Type_Name", json_mkstring(token_ptrs[count - 1]));
  json_append_member(obj, "Members", args);
  str[0] = '\0';

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
        tokens[index - 1] = '\0';
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
        ret = process_function_signature(token_ptrs, token_types, count);
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
  /*
     json_append_member(json, "Function", json_mkstring("func_name"));

     JsonNode *types = json_mkarray();
     json_append_element(types, json_mkstring("int"));
     json_append_element(types, json_mkstring("char*"));
     json_append_element(types, json_mkstring("char*"));
     json_append_member(json, "Types", types);

*/
  // JsonNode *funcs = json_mkarray();



  process_input(header, json);

  char *json_out = json_encode(json);
  printf("%s\n", json_out);

  fclose(header);

  return 0;
}
