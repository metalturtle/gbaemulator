# Game Boy Advance Emulator

This project is a from-scratch Game Boy Advance emulator targeting `pokemonemerald.gba`.

Current implementation status:

- Native C++ core scaffold.
- ROM header parsing and cartridge loading.
- Initial GBA memory map and bus routing.
- Reset state and frame stepping hooks.
- Native tests for ROM loading, memory behavior, reset state, and Pokemon ROM smoke loading.

The final goal is to pass GBA test ROMs and run Pokemon Emerald. This repository is not there yet; the current code establishes the core boundaries and verification harness needed to continue.

## Build And Test

```sh
make test
```

## Architecture

```text
core/
  include/gba/
    gba.h              C ABI for native/WASM frontend integration
    emulator.hpp       C++ core facade
  src/
    bus.cpp
    cartridge.cpp
    cpu.cpp
    emulator.cpp
    ...
tests/
  test_main.cpp
```

The C++ core is platform-independent. Browser and WebAssembly integration should call the C ABI and must not leak DOM/WebGL concerns into the core.
