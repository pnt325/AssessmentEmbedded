# AssessmentEmbedded

## Requirements

- CMake 3.25 or newer
- MSYS2 UCRT64 toolchain (`gcc`, `mingw32-make`) installed at `C:/msys64/ucrt64`

The paths are set in [CMakePresets.json](CMakePresets.json). If your toolchain is installed elsewhere, change them there.

## Build and run the tests

From the project root:

```
cmake --workflow --preset test
```

This configures the project, builds the library and `test/main.c`, and runs the tests. It prints each failed check and ends with a summary such as:

```
0 checks, 0 passed, 0 failed -> PASSED
```

If any check fails, the command exits with a non-zero code.

Once the project has been configured, you can rebuild and rerun more quickly with:

```
cmake --build --preset test
```

Build output goes to `out/build/Default`.
