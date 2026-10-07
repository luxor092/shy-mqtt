# SHY-MQTT — MQTT Parser/Generator Library

This repository contains the source code, usage examples, and tests for the **shy-mqtt** library.

The library is intended to serialize and deserialize MQTT packets on the client side and **does not handle network connections or communication**. Generated packets can be sent using any transport layer chosen by the application.

The programmer can provide custom functions for memory allocation and release. This makes it possible to avoid using the standard dynamic memory allocation functions, which is convenient for microcontrollers and other resource-constrained devices.

## Usage and API

Copy the files from the `src` directory of the repository to your project's folder.

Include the `shy_mqtt.h` header in your application.

The following files can be used as usage examples:

* `examples/example_create_packet.c`
* `examples/example_parse_packet.c`

## Build Examples and Tests

From the repository root:

```
cmake -B build 
cmake --build build
```

Tests use the **Unity Test Framework** by [ThrowTheSwitch](https://github.com/ThrowTheSwitch), 
distributed under the MIT License.

## Generate Documentation with Doxygen

Doxygen can generate HTML, LaTeX, and PDF documentation.

From the repository root:

```
doxygen Doxyfile 
cd doc/latex 
make pdf
```

The generated PDF documentation can be found at:

```
shy-mqtt/doc/latex/refman.pdf
```
## Contributing

Bug reports, feature requests, and pull requests are welcome.
Please, use the GitHub Issues and Pull Requests sections to contribute to the project.
