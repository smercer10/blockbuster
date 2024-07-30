# Blockbuster

Blockbuster is a WIP collection of efficient and non-blocking (lock-free, not wait-free) data structures for C++. It is by no means production-ready, and probably never will be, as it is primarily a learning exercise.

[![MIT License](https://img.shields.io/badge/License-MIT-green.svg)](https://github.com/smercer10/blockbuster/blob/main/LICENSE)
[![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/smercer10/blockbuster/test.yml?label=CI)](https://github.com/smercer10/blockbuster/actions/workflows/test.yml)
[![Documentation](https://img.shields.io/badge/Documentation-Doxygen-blue)](https://smercer10.github.io/blockbuster)

## Progress

The following data structures are currently implemented (documentation can be found [here](https://smercer10.github.io/blockbuster)):

### Single-Producer, Single-Consumer (SPSC)

- Queue (generic, fixed capacity)

### Multi-Producer, Multi-Consumer (MPMC)

- Queue (generic, fixed capacity)

## Build Locally

### Prerequisites

- C++17 compiler (currently uses some 17-specific features, but seems to build fine without any compiler restrictions)
- CMake 3.14+
- Make (any recent version should be fine)

### Commands

- Build the test executables:

```bash
make
```

- Run the tests (also builds the test executables if necessary):

```bash
make test
```

- Clean the build directory:

```bash
make clean
```
