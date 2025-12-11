# Minimal Read-Only BCD Parser

This project is a small, clean-room C99 implementation of a read-only Boot Configuration Data (BCD) parser and `bcdedit`-style command-line tool. It avoids Windows-specific APIs and only relies on standard C library facilities. Alongside on-screen enumeration, the tool can also export a text rendering of the store for offline inspection.

## Components
- **bcd.c / bcd.h**: Fixed-size in-memory model for BCD stores, objects, and elements with helper utilities for parsing and formatting object identifiers.
- **regf.c / regf.h**: Minimal, bounds-checked reader for registry hive (regf) files used by BCD stores.
- **bcd_parser.c / bcd_parser.h**: Maps regf hive data into the BCD model while tolerating malformed entries.
- **bcdedit.c**: CLI front end supporting `/store <path> /enum` with optional object filtering and `/help` usage text.

## Building
Compile the tool with a standard C99 compiler. Example using GCC:

```sh
gcc -std=c99 -Wall -Wextra -pedantic bcdedit.c bcd.c regf.c bcd_parser.c -o bcdedit
```

## Usage
- Show help: `./bcdedit /?` or `./bcdedit /help`
- Enumerate all objects from a hive: `./bcdedit /store /path/to/BCD /enum`
- Enumerate a single object by identifier: `./bcdedit /store /path/to/BCD /enum {<guid>}`
- Export the full store (or a single object) to a text file: `./bcdedit /store /path/to/BCD /export /tmp/store.txt [{<guid>}]`

Output lists each object’s identifier, type, and known elements. Unknown elements are still displayed with raw identifiers to aid inspection.

## Design Notes and Limits
- Read-only: no write or modify operations are implemented.
- Fixed capacities: store, object, and element counts are bounded by macros in `bcd.h`.
- Hive parsing is intentionally minimal: transaction logs, security data, and advanced registry features are not supported.
- Assumes the hive root corresponds to the BCD store; subkeys represent objects and values represent elements.

## Repository Layout
- `bcd.h`, `bcd.c`: BCD in-memory structures and helpers
- `regf.h`, `regf.c`: registry hive reader
- `bcd_parser.h`, `bcd_parser.c`: regf-to-BCD loader
- `bcdedit.c`: CLI entry point
- `LICENSE`: project license
