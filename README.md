## Overview
Cthulhu is a low-level, statically typed, functional programming language.
It is designed to be suitable for program transformation, testing, and verification.
This repository implements the compiler toolchain for the JavaScript dialect of Cthulhu.

Currently, two compilers are being implemented:
- **jsc** - compiles Javascript into Cthulhu
- **cthuc** - lowers Cthulhu into QuickJS bytecode

```text
JavaScript
     │
     ▼
    jsc
     │
     ▼
Cthulhu IR
     │
     ▼
   cthuc
     │
     ▼
QuickJS Bytecode
```

## Repository Structure
```text
src/
├── asm/         # Low-level assembly IR used for QuickJS bytecode emission
├── bytecode/    # QuickJS bytecode definitions and utilities
├── common/      # Shared utilities and infrastructure
├── cthuc/       # Cthulhu → QuickJS compiler
├── cthu_core/   # Core definitions for the Cthulhu JavaScript dialect
├── jsc/         # JavaScript → Cthulhu compiler
└── toy/         # Deprecated experimental compiler
```

> **Status:** Active development. Both compilers are currently under implementation.
