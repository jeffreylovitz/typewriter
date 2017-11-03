usage:

```
typewriter [header_file]

typewriter < [header_file]
```

Typewriter is intended to take in an arbitrary C header and extract certain type-oriented information from it.

One JSON object is returned, containing "Functions" and "Structs" as keys when either has at least 1 definition.

A sample output (with whitespace added for clarity) could look like;

```
{
  "Functions": [
    {
      "Function": "parse_type",
        "return_type": "INPUT_TYPE",
        "arguments": [
        "char*"
        ]
    },
    {
      "Function": "read_line",
      "return_type": "TOKENS*",
      "arguments": [
        "char*"
      ]
    },
    {
      "Function": "parse_delimiter",
      "return_type": "char",
      "arguments": [
        "char*"
      ]
    }
  ],
  "Structs": [
    {
      "Type_Name": "INPUT_PROGRESSS",
      "Members": [
      {
        "num_lines": "int"
      },
      {
        "num_bytes": "unsigned long"
      }
      ]
    }
  ]
}
```

A) Function signatures that are stripped of variable names - `double average(int *values, int value_count)`, for example, becomes `double average(int *, int)`

B) Struct definitions, which are returned with nothing elided (as names are required for accessing struct members)

All output is printed without adornment to STDOUT, and may be piped to file or into another function as desired.

The fundamental purpose of this transformation is to act as an interim step in automating the construction of Ruby FFI bindings for C programs.