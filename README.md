## Overview
Cthulhu is a low-level, statically typed, functional programming language.
It is designed to be suitable for program transformation, testing, and verification.
This repository implements the compiler toolchain for the JavaScript dialect of Cthulhu.

Currently, two compilers are being implemented:
- **js2ct** - compiles Javascript into Cthulhu
- **cthuc** - lowers Cthulhu into QuickJS bytecode

```text
JavaScript
     │
     ▼
    js2ct
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
├── js2ct/       # JavaScript → Cthulhu compiler
```

> **Status:** Active development. Both compilers are currently under implementation.
