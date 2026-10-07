cmake_minimum_required(VERSION 3.25)
project(shy_mqtt C)

set(CMAKE_C_STANDARD 99)

add_executable(generation_example 
	examples/example_create_packet.c
	examples/common/shy_preview.c
	src/shy_mqtt.c
)

add_executable(parse_example
	examples/example_parse_packet.c
	examples/common/shy_preview.c
	src/shy_mqtt.c
)

add_executable(tests
	tests/tests.c
	unity/unity_fixture.c
	unity/unity.c
	src/shy_mqtt.c
)
