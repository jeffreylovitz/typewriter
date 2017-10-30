usage:
typewriter [header_file]
typewriter < [header_file]

Typewriter is intended to take in an arbitrary C header and extract certain type-oriented information from it. Specifically, it returns:
A) Function signatures that are stripped of variable names - `double average(int *values, int value_count)`, for example, becomes `double average(int *, int)`
B) Struct definitions, which are returned with nothing elided (as names are required for accessing struct members)

All output is printed without adornment to STDOUT, and may be piped to file or into another function as desired.

The fundamental purpose of this transformation is to act as an interim step in automating the construction of Ruby FFI bindings for C programs.