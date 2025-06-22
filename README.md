# sqlite-search-cpp

A simple C++ console program that connects to a SQLite `.db` file, lists tables and fields, and allows searching by one or more fields. Matching records are printed with all available data.

## Features

- Lists all tables and fields in a `.db` file
- Supports multi-field search (e.g. name + surname)
- Handles fields with special characters or extra quotes
- Written using the SQLite C API

## Requirements

```
Requires SQLite3 and a C++ compiler.
```
