/** @file tests.c
 *
 * @brief Shy-mqtt functions tests.
 *
 * @copyright Copyright (c) 2026 Pavel Salnikov.
 * @license This project is licensed under the MIT License.
 *  		See the LICENSE file in the root directory for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../unity/unity_fixture.h"
#include "../src/shy_mqtt.h"

#define PACKET_TYPE 0
#define MSG_DIFFERENT_SIZE "Reference and sample packets have different size."
#define MSG_DIFFERENT_PACKETS "The reference and the sample packets are not equal."
#define MSG_SAMPLE_PACKET_ERROR "Couldn't prepare sample packet."
#define MSG_REFERENCE_PACKET_ERROR "Couldn't prepare reference packet."
#define MSG_PARSED_WRONG_TYPE "Parsed wrong packet type."
#define MSG_PARSED_WRONG_TOPIC "Parsed wrong topic."
#define MSG_PARSED_WRONG_QOS "Parsed wrong QoS."
#define MSG_WRONG_PACKET_ID "Parsed wrong packet ID."
#define MSG_WRONG_PAYLOAD_LENGTH "Parsed wrong length of payload."
#define MSG_WRONG_PAYLOAD "Parsed wrong payload."
#define MSG_ERROR_IGNORED "Parser/generator should have returned an error."
#define MSG_ERROR_NOT_SET "Parser/generator didn't set correct value of the mqtt_error variable."

#define TEST_VS_REFERENCE(p_reference, p_target)\
	if (p_reference == NULL)\
		TEST_FAIL_MESSAGE("Couldn't prepare reference packet.");\
	TEST_ASSERT_NOT_NULL_MESSAGE(p_sample, MSG_SAMPLE_PACKET_ERROR);\
	TEST_ASSERT_EQUAL_MESSAGE(g_reference_size, g_size, MSG_DIFFERENT_SIZE);\
	TEST_ASSERT_EQUAL_MEMORY_MESSAGE(p_reference, p_sample, g_size, MSG_DIFFERENT_PACKETS);

size_t g_size, g_reference_size;
uint8_t *p_reference;
uint8_t *p_sample;

void *unity_malloc(size_t size)
{
	return __builtin_malloc(size);
}

void unity_free(void* p) 
{
	return __builtin_free(p);
}

static uint8_t *hex_to_bytes(FILE *p_file, size_t *p_size);

static void print_bytes(uint8_t *p_data, size_t n_bytes);

static void print_bytes_ascii(uint8_t *p_data, size_t n_bytes);

static void print_packets_pair(
		const char *title1,
		uint8_t *p_packet1,
		size_t packet1_size,
		const char *title2,
		uint8_t *p_packet2,
		size_t packet2_size
		);

static void print_packets_pair_ascii(
		const char *title1,
		uint8_t *p_packet1,
		size_t packet1_size,
		const char *title2,
		uint8_t *p_packet2,
		size_t packet2_size
		);

static uint8_t *prepare_reference(const char* directory, size_t *p_size);

TEST_GROUP(packet_type);

TEST_SETUP(packet_type)
{
	mqtt_set_custom_allocator(unity_malloc);
}

TEST_TEAR_DOWN(packet_type)
{
	if (p_sample != NULL)
	{
		free(p_sample);
		p_sample = NULL;
	}
}

TEST(packet_type, connect)
{
	mqtt_conn_flags_t conn_flags = mqtt_create_connect_flags(false, false, false, 0, false, false);
	mqtt_conn_credentials_t conn_credentials = mqtt_create_connect_credentials("cliendID", 
			NULL, NULL);
	p_sample = mqtt_create_connect_packet(conn_flags, conn_credentials,
			1200, "lw_topic", "lw_message", &g_size);
	
	TEST_ASSERT_EQUAL(0b00010000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, publish)
{
	mqtt_ctrl_flags_t ctrl_flags = mqtt_create_control_flags(false, 0, false);
	const size_t PAYLOAD_SIZE = 10;
	uint8_t *p_payload = malloc(PAYLOAD_SIZE);
	p_sample = mqtt_create_publish_packet(ctrl_flags, "topic", p_payload, PAYLOAD_SIZE,
			1, &g_size);
	free(p_payload);
	TEST_ASSERT_EQUAL(0b00110000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, puback)
{
	p_sample = mqtt_create_puback_packet(1, &g_size);
	TEST_ASSERT_EQUAL(0b01000000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, pubrec)
{
	p_sample = mqtt_create_pubrec_packet(1, &g_size);
	TEST_ASSERT_EQUAL(0b01010000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, pubrel)
{
	p_sample = mqtt_create_pubrel_packet(1, &g_size);
	TEST_ASSERT_EQUAL(0b01100010, p_sample[PACKET_TYPE]);
}

TEST(packet_type, pubcomp)
{
	p_sample = mqtt_create_pubcomp_packet(1, &g_size);
	TEST_ASSERT_EQUAL(0b01110000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, subscribe)
{
	p_sample = mqtt_create_subscribe_packet(
			1,
			(const char *[]){"topic"},
			1,
			(uint8_t []){1},
			&g_size
			);
	TEST_ASSERT_EQUAL(0b10000010, p_sample[PACKET_TYPE]);
}

TEST(packet_type, unsubscribe)
{
	p_sample = mqtt_create_unsubscribe_packet(
			1,
			(const char *[]){"topic"},
			1,
			&g_size
			);
	TEST_ASSERT_EQUAL(0b10100010, p_sample[PACKET_TYPE]);
}

TEST(packet_type, pingreq)
{
	p_sample = mqtt_create_pingreq_packet(&g_size);
	TEST_ASSERT_EQUAL(0b11000000, p_sample[PACKET_TYPE]);
}

TEST(packet_type, disconnect)
{
	p_sample = mqtt_create_disconnect_packet(&g_size);
	TEST_ASSERT_EQUAL(0b11100000, p_sample[PACKET_TYPE]);
}

TEST_GROUP_RUNNER(packet_type)
{
	RUN_TEST_CASE(packet_type, connect);
	RUN_TEST_CASE(packet_type, publish);
	RUN_TEST_CASE(packet_type, puback);
	RUN_TEST_CASE(packet_type, pubrec);
	RUN_TEST_CASE(packet_type, pubrel);
	RUN_TEST_CASE(packet_type, pubcomp);
	RUN_TEST_CASE(packet_type, subscribe);
	RUN_TEST_CASE(packet_type, unsubscribe);
	RUN_TEST_CASE(packet_type, pingreq);
	RUN_TEST_CASE(packet_type, disconnect);
}

TEST_GROUP(compare_to_reference);

TEST_SETUP(compare_to_reference) 
{
	mqtt_set_custom_allocator(unity_malloc);
}

TEST_TEAR_DOWN(compare_to_reference)
{
	if (p_reference != NULL)
	{
		free(p_reference);
		p_reference = NULL;
	}
	
	if (p_sample != NULL)
	{
		free(p_sample);
		p_sample = NULL;
	}
}

TEST(compare_to_reference, connect)
{
	p_reference = prepare_reference("../tests/reference/connect.txt", &g_reference_size);

	mqtt_conn_flags_t connect_flags = mqtt_create_connect_flags(
			true,	// username
			true, 	// password
			false, 	// will_retain
			0,		// will QoS
			false,  // will send
			true	// clean session
			);

	mqtt_conn_credentials_t connect_credentials = mqtt_create_connect_credentials(
			"developer0",		 // client id
			"developer", 		 // username
			"sneakpeak007");	 // password

	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials,
			600,	// keepalive
			NULL,	// last will topic
			NULL, 	// last will message
			&g_size
			);
}

TEST(compare_to_reference, publish1) 
{
	p_reference = prepare_reference("../tests/reference/publish1.txt", &g_reference_size);
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"command/1",
			(const uint8_t *)"hello world",
			strlen("hello world"),
			19430,
			&g_size
			);	
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, publish2)
{
	p_reference = prepare_reference("../tests/reference/publish2.txt", &g_reference_size);
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(0, 2, 0);
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"command/1",
			(const uint8_t *)"hello world",
			strlen("hello world"),
			19431,
			&g_size
			);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, puback)
{
	p_reference = prepare_reference("../tests/reference/puback.txt", &g_reference_size);
	p_sample =	mqtt_create_puback_packet(19430, &g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, pubrec)
{
	p_reference = prepare_reference("../tests/reference/pubrec.txt", &g_reference_size);
	p_sample = mqtt_create_pubrec_packet(15260, &g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, pubrel)
{
	p_reference = prepare_reference("../tests/reference/pubrel.txt", &g_reference_size);
	p_sample = mqtt_create_pubrel_packet(19431, &g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, pubcomp)
{
	p_reference = prepare_reference("../tests/reference/pubcomp.txt", &g_reference_size);
	p_sample = mqtt_create_pubcomp_packet(19431, &g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, subscribe)
{
	p_reference = prepare_reference("../tests/reference/subscribe.txt", &g_reference_size);
	p_sample = mqtt_create_subscribe_packet(
			19428,
			(const char *[]){"reg"},
			1,
			(uint8_t []){0},
			&g_size
	 	);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, unsubscribe)
{
	TEST_IGNORE_MESSAGE("UNSUBSCRIBE reference packet not yet provided.");
	p_reference = prepare_reference("../tests/reference/unsubscribe.txt", &g_reference_size);
	p_sample = mqtt_create_unsubscribe_packet(
			19428,
			(const char*[]){"topic1", "topic2"},	// Array of topics to unsubscribe.
			2,										// Number of topics in the array.
			&g_size
			);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, pingreq)
{
	TEST_IGNORE_MESSAGE("PINGREQ reference packet not yet provided.");
	p_reference = prepare_reference("../tests/reference/pingreq.txt", &g_reference_size);
	p_sample = mqtt_create_pingreq_packet(&g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST(compare_to_reference, disconnect)
{
	p_reference = prepare_reference("../tests/reference/disconnect.txt", &g_reference_size);
	p_sample = mqtt_create_disconnect_packet(&g_size);
	TEST_VS_REFERENCE(p_reference, p_sample);
}

TEST_GROUP_RUNNER(compare_to_reference)
{
	// Compare generated mqtt packets to reference packets provided in .txt files.
	RUN_TEST_CASE(compare_to_reference, connect);
	RUN_TEST_CASE(compare_to_reference, publish1);
	RUN_TEST_CASE(compare_to_reference, publish2);
	RUN_TEST_CASE(compare_to_reference, puback);
	RUN_TEST_CASE(compare_to_reference, pubrec);
	RUN_TEST_CASE(compare_to_reference, pubrel);
	RUN_TEST_CASE(compare_to_reference, pubcomp);
	RUN_TEST_CASE(compare_to_reference, subscribe);	
	RUN_TEST_CASE(compare_to_reference, unsubscribe);
	RUN_TEST_CASE(compare_to_reference, pingreq);
	RUN_TEST_CASE(compare_to_reference, disconnect);
}

TEST_GROUP(parse_reference);

TEST_SETUP(parse_reference) 
{
	mqtt_set_custom_allocator(unity_malloc);
	mqtt_set_custom_free(unity_free);
}

TEST_TEAR_DOWN(parse_reference)
{
	if (p_reference != NULL)
	{
		free(p_reference);
		p_reference = NULL;
	} 
}

TEST(parse_reference, connack)
{
	p_reference = prepare_reference("../tests/reference/connack.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_CONNACK, packet_type,
			MSG_PARSED_WRONG_TYPE);

	_Bool b_session_present;
	mqtt_parse_connack(p_reference, &b_session_present, g_reference_size);
	TEST_ASSERT_EQUAL(0, b_session_present);
}

TEST(parse_reference, publish)
{
	p_reference = prepare_reference("../tests/reference/publish1.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);

	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_PUBLISH, packet_type,
			MSG_PARSED_WRONG_TYPE);

	uint8_t QoS;
	mqtt_packet_id_t packet_id;
	char *p_topic;
	uint8_t *p_payload;
	size_t payload_length;

	p_payload = mqtt_parse_publish(p_reference, &QoS, &packet_id, &p_topic,	&payload_length, g_reference_size);

	// Test QoS.
	TEST_ASSERT_EQUAL_MESSAGE(1, QoS, MSG_PARSED_WRONG_QOS);

	// Test packet_id.
	TEST_ASSERT_EQUAL_MESSAGE(19430, packet_id, MSG_WRONG_PACKET_ID);

	// Test parsed topic.
	TEST_ASSERT_EQUAL_STRING_MESSAGE("command/1", p_topic, MSG_PARSED_WRONG_TOPIC);

	// Test payload length.
	TEST_ASSERT_EQUAL_MESSAGE(strlen("hello world"), payload_length,
			MSG_WRONG_PAYLOAD_LENGTH);

	// Test payload.
	TEST_ASSERT_EQUAL_MEMORY_MESSAGE("hello world", p_payload, strlen("hello world"),
			MSG_WRONG_PAYLOAD);

	/* 
	 * XXX: Heap memory by the p_topic pointer will NOT be deleted in the case of test failed.
	 * (As the topic doesn't takes much memory, this is just ignored).
	 */
}

TEST(parse_reference, puback)
{
	p_reference = prepare_reference("../tests/reference/puback.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_PUBACK, packet_type, MSG_PARSED_WRONG_TYPE);
	mqtt_packet_id_t packet_id = mqtt_parse_puback(p_reference, g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(19430, packet_id, MSG_WRONG_PACKET_ID);
}

TEST(parse_reference, pubrec) 
{
	p_reference = prepare_reference("../tests/reference/pubrec.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_PUBREC, packet_type, MSG_PARSED_WRONG_TYPE);
	mqtt_packet_id_t packet_id = mqtt_parse_pubrec(p_reference, g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(15260, packet_id, MSG_WRONG_PACKET_ID);
}

TEST(parse_reference, pubrel) 
{
	p_reference = prepare_reference("../tests/reference/pubrel.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_PUBREL, packet_type, MSG_PARSED_WRONG_TYPE);
	mqtt_packet_id_t packet_id = mqtt_parse_pubrel(p_reference, g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(19431, packet_id, MSG_WRONG_PACKET_ID);
}

TEST(parse_reference, pubcomp)
{
	p_reference = prepare_reference("../tests/reference/pubcomp.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_PUBCOMP, packet_type, MSG_PARSED_WRONG_TYPE);
	mqtt_packet_id_t packet_id = mqtt_parse_pubcomp(p_reference, g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(19431, packet_id, MSG_WRONG_PACKET_ID);
}

TEST(parse_reference, suback) 
{
	TEST_IGNORE_MESSAGE("SUBACK reference packet not yet provided.");

	p_reference = prepare_reference("../tests/reference/suback.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL(MQTT_PACKET_TYPE_SUBACK, packet_type);	
	const unsigned int N_TOPICS = 3;
	mqtt_return_code_suback_t return_codes[N_TOPICS];
	mqtt_packet_id_t packet_id = mqtt_parse_suback(p_reference, return_codes, ARRAY_SIZE(return_codes),
			g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(19430, packet_id, MSG_WRONG_PACKET_ID);

	TEST_ASSERT_EQUAL(return_codes[0], MQTT_SUBACK_SUCCESS_QOS_1);
	TEST_ASSERT_EQUAL(return_codes[1], MQTT_SUBACK_SUCCESS_QOS_1);
	TEST_ASSERT_EQUAL(return_codes[2], MQTT_SUBACK_FAILURE);
}

TEST(parse_reference, unsuback) 
{
	TEST_IGNORE_MESSAGE("UNSUBACK reference packet not yet provided.");

	p_reference = prepare_reference("../tests/reference/unsuback.txt", &g_reference_size);	
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR); 
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL(MQTT_PACKET_TYPE_UNSUBACK, packet_type);
	mqtt_packet_id_t packet_id = mqtt_parse_unsuback(p_reference, g_reference_size);
	TEST_ASSERT_EQUAL_MESSAGE(19430, packet_id, MSG_WRONG_PACKET_ID);
}

TEST_GROUP_RUNNER(parse_reference)
{
	RUN_TEST_CASE(parse_reference, connack);
	RUN_TEST_CASE(parse_reference, publish);
	RUN_TEST_CASE(parse_reference, puback);
	RUN_TEST_CASE(parse_reference, pubrec);
	RUN_TEST_CASE(parse_reference, pubrel);
	RUN_TEST_CASE(parse_reference, pubcomp);
	RUN_TEST_CASE(parse_reference, suback);
	RUN_TEST_CASE(parse_reference, unsuback);
}

TEST_GROUP(parse_invalid_packet);

TEST_SETUP(parse_invalid_packet) 
{
	mqtt_set_custom_allocator(unity_malloc);
	mqtt_set_custom_free(unity_free);
}

TEST_TEAR_DOWN(parse_invalid_packet) 
{
	// In this test group "p_sample" is used to store a generated packet to be parsed.
	if (p_sample != NULL)
	{
		free(p_sample);
		p_sample = NULL;
	} 

	if (p_reference != NULL)
	{
		free(p_reference);
		p_reference = NULL;
	}

	mqtt_reset_error();
}

TEST(parse_invalid_packet, big_remaining_length) 
{
	// For PUBLISH.
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);

	// Create a sample PUBLISH packet.
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"tests/big_remaining_length",
			(const uint8_t*)"hello",
			strlen("hello"), 1,
			&g_size
			);

	/* 
	 * Parse the packet, but decrease number of bytes available for reading (g_size) so that
	 * remaining length claimed in the packet becomes too big. 
	 */
	uint8_t *p_payload = mqtt_parse_publish(p_sample, NULL, NULL, NULL, &g_reference_size,
			g_size - 1);

	// Funtion must return NULL as indicator of an error.
	TEST_ASSERT_NULL_MESSAGE(p_payload, MSG_ERROR_IGNORED);

	// Make sure the function threw proper error.
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_OUTSIDE_BOUNDARY, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	free(p_sample);
	mqtt_reset_error();


	// For PUBACK.
	// NOTE: For current implementation the test also applies to PUBREC, PUBREL, PUBCOMP, UNSUBACK.
	p_sample = mqtt_create_puback_packet(1, &g_size);
	mqtt_packet_id_t packet_id = mqtt_parse_puback(p_sample, g_size - 1);

	// Function must return 0 as indicator of an error.
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, packet_id, MSG_ERROR_IGNORED);

	// Make sure the function threw proper error.
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_OUTSIDE_BOUNDARY, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	free(p_sample);
	p_sample = NULL;
	mqtt_reset_error();

	
	// For CONNACK.
	p_reference = prepare_reference("../tests/reference/connack.txt", &g_reference_size);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, MSG_REFERENCE_PACKET_ERROR);
	
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_reference);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_PACKET_TYPE_CONNACK, packet_type,
			"Wrong type of reference packet.");

	mqtt_return_code_connack_t return_code = mqtt_parse_connack(
			p_reference, 
			NULL, 
			g_reference_size - 1
			);

	TEST_ASSERT_EQUAL_MESSAGE(MQTT_CONNACK_FUNCTION_ERROR, return_code, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_OUTSIDE_BOUNDARY, mqtt_get_error(), MSG_ERROR_NOT_SET);

	free(p_reference);
	p_reference = NULL;
	mqtt_reset_error();

	//TODO: for SUBACK.
}

TEST(parse_invalid_packet, remaining_length_cut) 
{
	// Create 130 bytes of payload. PUBLISH packet will use two bytes to encode
	// the remaining length when payload is 130 bytes.
	uint8_t payload[130];

	// Create control flags for PUBLISH packet.
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);

	// Create a PUBLISH packet where remaining length encoded with two bytes.
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"test/remaining_length_cut",
			payload,
			ARRAY_SIZE(payload), 1,
			&g_size);

	// Parse the PUBLISH packet, but provide fake size of the packet so that
	// the second byte of remaining length field will be denied for reading.
	size_t payload_size;
	uint8_t *p_payload = mqtt_parse_publish(p_sample, NULL, NULL, NULL, &payload_size, 2);

	// Make sure the function threw proper error.
	TEST_ASSERT_NULL_MESSAGE(p_payload, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_OUTSIDE_BOUNDARY, mqtt_get_error(),
			MSG_ERROR_NOT_SET);	
}

TEST(parse_invalid_packet, big_topic_length) 
{
	// Create control flags for PUBLISH packet.
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 0, false);

	// Create publish packet.
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"test/big_topic_length",
			(const uint8_t*)"hello",
			strlen("hello"), 1,
			&g_size
			);

	// Manually change the topic length of the sample to 100;
	p_sample[3] = 100; // Topic length LSB.
	
	// Parse the PUBLISH packet.
	size_t payload_size;
	uint8_t *p_payload = mqtt_parse_publish(p_sample, NULL, NULL, NULL, &payload_size, g_size);

	// Make sure the function threw proper error.
	TEST_ASSERT_NULL_MESSAGE(p_payload, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_OUTSIDE_BOUNDARY, mqtt_get_error(),
			MSG_ERROR_NOT_SET);
}

TEST(parse_invalid_packet, zero_topic_length) 
{
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);

	// Create a sample PUBLISH packet.
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"tests/zero_topic_length",
			(const uint8_t*)"hello",
			strlen("hello"), 1,
			&g_size
			);

	// Modify the packet so that it has zero topic length (not allowed in MQTT 3.1.1).
	p_sample[2] = 0;	// Topic length MSB.
	p_sample[3] = 0;	// Topic length LSB.	

	// Parse the packet.
	uint8_t *p_payload = mqtt_parse_publish(p_sample, NULL, NULL, NULL, &g_reference_size,
			g_size);

	// Make sure the function threw proper error.
	TEST_ASSERT_NULL_MESSAGE(p_payload, MSG_ERROR_IGNORED);

	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_TOPIC_LENGTH, mqtt_get_error(),
			MSG_ERROR_NOT_SET);
}

TEST(parse_invalid_packet, zero_packet_id)
{
	// 1. IN PUBLISH PACKET

	// Create a PUBLISH packet with QoS = 1.
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"topic",
			(const uint8_t*)"test",
			strlen("test"),
			12,						// Non-zero packet id.
			&g_size
			);

	// Set packet_id = 0.
	p_sample[10] = 0;	// LSB of packet id.
	
	// Parse the PUBLISH packet.
	mqtt_packet_id_t packet_id;
	uint8_t *p_payload = mqtt_parse_publish(
			p_sample, 
			NULL,				// pointer to QoS
			&packet_id,
			NULL,				// pointer to the pointer to the topic string
			&g_reference_size,	// pointer to payload length
			g_size
			);

	TEST_ASSERT_NULL_MESSAGE(p_payload, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(), MSG_ERROR_NOT_SET);

	mqtt_reset_error();
	free(p_sample);
	p_sample = NULL;


	// 2. IN PUBACK PACKET
	// NOTE: the test also applies to PUBREC, PUBREL, PUBCOMP, UNSUBACK.

	// Create a PUBACK packet.
	p_sample = mqtt_create_puback_packet(1, &g_size);

	// Change packet id to zero.
	p_sample[3] = 0x00;		// Packet ID MSB.
	p_sample[4] = 0x00;		// Packet ID LSB.

	// Parse the PUBACK packet.
	packet_id = mqtt_parse_puback(p_sample, g_size);

	// Make sure the function threw proper error.
	TEST_ASSERT_EQUAL_MESSAGE(0, packet_id, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	mqtt_reset_error();
	free(p_sample);
	p_sample = NULL;


	// 3. IN SUBACK PACKET
	// Prepare SUBACK packet.
	const size_t SUBACK_PAYLOAD_SIZE = 4;
	const size_t SUBACK_REMAINING_LENGTH = SUBACK_PAYLOAD_SIZE + 2;
	g_size = SUBACK_REMAINING_LENGTH + 2;	
	p_sample = (uint8_t*)malloc(g_size);
	p_sample[0] = 0b10010000;					// Packet type = suback.
	p_sample[1] = 0x05;							// Remaining length.
	p_sample[2] = 0x00;							// MSB of packet_id.
	p_sample[3] = 0x00;							// LSB of packet_id.	
	p_sample[4] = 0x01;							// Return code success with QoS 1.
	p_sample[5] = 0x02;							// Return code success with QoS 2.
	p_sample[6] = 0x00;							// Return code success with QoS 0.
	p_sample[7] = 0x80;							// Return code Failure.

	// Parse SUBACK packet.
	packet_id = mqtt_parse_suback(
			p_sample,
			NULL,
			0,
			g_size);
	
	// Make sure the function threw proper error.
	TEST_ASSERT_EQUAL_MESSAGE(0, packet_id, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);
}

TEST(parse_invalid_packet, null_packet)
{
	// For undefined packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_PACKET_TYPE_NOT_RECOGNIZED, mqtt_recognize_packet(NULL),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For CONNACK packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_CONNACK_FUNCTION_ERROR, mqtt_parse_connack(NULL, NULL, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For PUBACK packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_puback(NULL, 4), 
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For PUBREC packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_pubrec(NULL, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For PUBREL packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_pubrel(NULL, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For PUBCOMP packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_pubcomp(NULL, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For SUBACK packet.
	mqtt_reset_error();
	mqtt_return_code_suback_t return_codes[1];	
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_suback(NULL, return_codes, 1, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For UNSUBACK packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_unsuback(NULL, 4),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);

	// For PUBLISH packet.
	mqtt_reset_error();
	TEST_ASSERT_EQUAL_INT_MESSAGE(0, mqtt_parse_publish(NULL, NULL, NULL, NULL, NULL, 10),
			MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_INT_MESSAGE(MQTT_ERR_INVALID_POINTER, mqtt_get_error(),
			MSG_ERROR_NOT_SET);
} /* parse invalid packet, null_packet */

TEST_GROUP_RUNNER(parse_invalid_packet) 
{
	RUN_TEST_CASE(parse_invalid_packet, null_packet);
	RUN_TEST_CASE(parse_invalid_packet, big_remaining_length);
	RUN_TEST_CASE(parse_invalid_packet, big_topic_length);
	RUN_TEST_CASE(parse_invalid_packet, zero_topic_length);
	RUN_TEST_CASE(parse_invalid_packet, remaining_length_cut);
	RUN_TEST_CASE(parse_invalid_packet, zero_packet_id);
}

TEST_GROUP(create_invalid_packet);

TEST_SETUP(create_invalid_packet)
{
	mqtt_set_custom_allocator(unity_malloc);
	mqtt_set_custom_free(unity_free);
}

TEST_TEAR_DOWN(create_invalid_packet)
{
	if (p_sample != NULL)
	{
		free(p_sample);
		p_sample = NULL;
	}

	if (p_reference != NULL)
	{
		free(p_reference);
		p_reference = NULL;
	}

	mqtt_reset_error();
}
TEST(create_invalid_packet, zero_packet_id)
{
	// PUBLISH PACKET
	const uint8_t NON_ZERO_QOS = 1;
	const mqtt_packet_id_t ZERO_PACKET_ID = 0;
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, NON_ZERO_QOS, false);
	const size_t PAYLOAD_SIZE = 1;
	p_reference = (uint8_t*)malloc(PAYLOAD_SIZE);
	TEST_ASSERT_NOT_NULL_MESSAGE(p_reference, "Couldn't allocate memory for payload.");
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"the_topic",
			p_reference,
			PAYLOAD_SIZE,
			ZERO_PACKET_ID,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// SUBSCRIBE PACKET
	const size_t N_TOPICS = 2;
	p_sample = mqtt_create_subscribe_packet(
			ZERO_PACKET_ID,
			(const char *[]){"topic1", "topic2"},
			N_TOPICS,
			(uint8_t []){1, 0},
			&g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// UNSUBSCRIBE PACKET
	p_sample = mqtt_create_unsubscribe_packet(
			ZERO_PACKET_ID,
			(const char *[]){"topic1", "topic2"},
			N_TOPICS,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// PUBACK PACKET
	p_sample = mqtt_create_puback_packet(ZERO_PACKET_ID, &g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// PUBREC PACKET
	p_sample = mqtt_create_pubrec_packet(ZERO_PACKET_ID, &g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// PUBREL PACKET
	p_sample = mqtt_create_pubrel_packet(ZERO_PACKET_ID, &g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);


	// PUBCOMP PACKET
	p_sample = mqtt_create_pubcomp_packet(ZERO_PACKET_ID, &g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_ZERO_PACKET_ID, mqtt_get_error(),
			MSG_ERROR_NOT_SET);
}

TEST(create_invalid_packet, null_client_id)
{
	// Client ID is NULL.
	mqtt_conn_flags_t connect_flags = mqtt_create_connect_flags(true, true, true, 2, true, true);
	mqtt_conn_credentials_t connent_credentials_null = mqtt_create_connect_credentials(
			NULL,
			"username",
			"password"
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connent_credentials_null,
			1200,
			"last will topic",
			"last will message",
			&g_size);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_CLIENT_ID, mqtt_get_error(), MSG_ERROR_NOT_SET);

	mqtt_reset_error();

	// Client ID is empty string.
	mqtt_conn_credentials_t connect_credentials_empty = mqtt_create_connect_credentials(
			"",
			"username",
			"password"
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials_empty,
			1200,
			"last will topic",
			"last will message",
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_CLIENT_ID, mqtt_get_error(), MSG_ERROR_NOT_SET);
}

TEST(create_invalid_packet, null_username)
{
	mqtt_conn_flags_t connect_flags = mqtt_create_connect_flags(
			true,	// username is switched ON.
			true,
			true,
			2,
			true,
			true
			);

	mqtt_conn_credentials_t connect_credentials_null = mqtt_create_connect_credentials(
			"client_id",
			NULL,
			"password"
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials_null,
			1200,
			"last will topic",
			"last will message",
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_USERNAME, mqtt_get_error(), MSG_ERROR_NOT_SET);

	mqtt_reset_error();

	mqtt_conn_credentials_t connect_credentials_empty = mqtt_create_connect_credentials(
			"client_id",
			"",
			"password"
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials_empty,
			1200,
			"last will topic",
			"last will message",
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_USERNAME, mqtt_get_error(), MSG_ERROR_NOT_SET);
}

TEST(create_invalid_packet, null_password)
{
	mqtt_conn_flags_t connect_flags = mqtt_create_connect_flags(
			true,
			true,	// passwors is switched ON.
			true,
			2,
			true,
			true);

	mqtt_conn_credentials_t connect_credentials_null = mqtt_create_connect_credentials(
			"client_id",
			"username",
			NULL
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials_null,
			1200,
			"last will topic",
			"last will message",
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_PASSWORD, mqtt_get_error(), MSG_ERROR_NOT_SET);

	mqtt_reset_error();

	mqtt_conn_credentials_t connect_credentials_empty = mqtt_create_connect_credentials(
			"client_id",
			"username",
			""
			);
	p_sample = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials_empty,
			1200,
			"last will topic",
			"last will message",
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_PASSWORD, mqtt_get_error(), MSG_ERROR_NOT_SET);
}

TEST(create_invalid_packet, null_topic)
{
	// SUBSCRIBE PACKET
	const char *subscribe_topics_null[] = {NULL};
	const char *subscribe_topics_empty[] = {""};
	const uint8_t QoS_array[] = {1};

	p_sample = mqtt_create_subscribe_packet(
			1,
			subscribe_topics_null,
			ARRAY_SIZE(subscribe_topics_null),
			QoS_array,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_TOPIC_LENGTH, mqtt_get_error(), MSG_ERROR_NOT_SET);

	p_sample = mqtt_create_subscribe_packet(
			1,
			subscribe_topics_empty,
			ARRAY_SIZE(subscribe_topics_empty),
			QoS_array,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_TOPIC_LENGTH, mqtt_get_error(), MSG_ERROR_NOT_SET);

	// TODO: UNSUBSCRIBE PACKET
}

TEST(create_invalid_packet, null_payload)
{
	// PUBLISH PACKET
	// Null payload pointer.
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"topic",
			NULL,		// Pointer to the payload (NULL).
			10,			// Payload size (any, non-zero).
			255,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_PAYLOAD, mqtt_get_error(), MSG_ERROR_NOT_SET);

	mqtt_reset_error();

	// PUBLISH PACKET
	// Create payload
	const size_t NON_ZERO_PAYLOAD_SIZE = 10;
	p_reference = (uint8_t*)malloc(NON_ZERO_PAYLOAD_SIZE);
	// Zero payload size.
	p_sample = mqtt_create_publish_packet(
			control_flags,
			"topic",
			p_reference,
			0,				// Zero payload size.
			255,
			&g_size
			);
	TEST_ASSERT_NULL_MESSAGE(p_sample, MSG_ERROR_IGNORED);
	TEST_ASSERT_EQUAL_MESSAGE(MQTT_ERR_INVALID_PAYLOAD, mqtt_get_error(), MSG_ERROR_NOT_SET);
}

TEST_GROUP_RUNNER(create_invalid_packet)
{
	RUN_TEST_CASE(create_invalid_packet, zero_packet_id);
	RUN_TEST_CASE(create_invalid_packet, null_client_id);
	RUN_TEST_CASE(create_invalid_packet, null_username);
	RUN_TEST_CASE(create_invalid_packet, null_password);
	RUN_TEST_CASE(create_invalid_packet, null_topic);
	RUN_TEST_CASE(create_invalid_packet, null_payload);
}

void runAllTests(void) 
{
	RUN_TEST_GROUP(packet_type);			// Check packet types of each packet.
	RUN_TEST_GROUP(compare_to_reference);	// Check generated packet vs reference packet.
	RUN_TEST_GROUP(parse_reference);		// Check parsing of known reference packets.
	RUN_TEST_GROUP(parse_invalid_packet);	// Check handling of invalid/malefactor packets.
	RUN_TEST_GROUP(create_invalid_packet);	// Check handling of attempts to create invalid packets.
}

int main(int argc, const char *argv[])
{
	return UnityMain(argc, argv, runAllTests);
} /* main() */

/*!
 * @brief Convert hex text to bytes.
 * @param p_file Text file to convert.
 * @param[out] p_size Number of resulted bytes.
 */
static uint8_t *hex_to_bytes(FILE *p_file, size_t *p_size)
{
	// Find file size and number of bytes.
	fseek(p_file, 0, SEEK_END);
	long file_size = ftell(p_file);

	// File size must be NOT EVEN (the size of hex is even + EOF).
	if(file_size % 2 == 0)
		return NULL;

	size_t n_bytes = (file_size - 1)/2;
	*p_size = n_bytes;
	uint8_t *p_data = (uint8_t *)unity_malloc(n_bytes);

	if(!p_data)
		return NULL;

	// Hex to bytes.
	long int position = 0;
	fseek(p_file, position, SEEK_SET);

	for(; position < file_size; position += 2)
	{
		fscanf(p_file, "%2hhx", p_data + position/2);
	}

	return p_data;
} /* hex_to_bytes() */

static void print_bytes(uint8_t *p_data, size_t n_bytes)
{
	for (size_t i = 0; i < n_bytes ; i++)
	{
		printf("%02X ", *(p_data + i));
	}
} /* print_data() */

static void print_bytes_ascii(uint8_t *p_data, size_t n_bytes)
{
	for (size_t i = 0; i < n_bytes; i++)
	{
		printf("%c", *(p_data + i));
	}
}

static void print_packets_pair(
		const char *title1,
		uint8_t *p_packet1,
		size_t packet1_size,
		const char *title2,
		uint8_t *p_packet2,
		size_t packet2_size
		)
{
	// Print the first packet title and body.
	printf("\n\n%s\n", title1);
	print_bytes(p_packet1, packet1_size);
	puts("");

	// Print the second packet title and body.
	puts(title2);
	print_bytes(p_packet2, packet2_size);
} /* print_packets() */

static void print_packets_pair_ascii(
		const char *title1,
		uint8_t *p_packet1,
		size_t packet1_size,
		const char *title2,
		uint8_t *p_packet2,
		size_t packet2_size
		)
{
	// Print the first packet title and body (ascii).
	printf("\n\n%s\n", title1);
	print_bytes_ascii(p_packet1, packet1_size);
	puts("");

	// Print the second packet title and body (ascii).
	puts(title2);
	print_bytes_ascii(p_packet2, packet2_size);
} /* print_packets_ascii() */

/*!
 * @brief Allocate heap memory for the reference packet and fill it with valid data using
 * the provided directory.
 * @param[in] direct ry Directory of the file including file name.
 * @param[out] p_size Pointer to the variable that stores size in bytes.
 */
static uint8_t *prepare_reference(const char* directory, size_t *p_size)
{
	FILE *p_file = fopen(directory, "r");
	if (p_file == NULL)
	{
		printf("\nCouldn't open the %s file.\n", directory);
		return NULL;
	}

	// Find file size and number of bytes.
	fseek(p_file, 0, SEEK_END);
	long file_size = ftell(p_file);

	// File size must be NOT EVEN (the size of hex is even + EOF).
	if(file_size % 2 == 0)
	{
		printf("Invalid HEX code in the %s. Number of text characters must be even.\n", directory);
		return NULL;
	}

	*p_size = (file_size - 1)/2;
	uint8_t *p_data = (uint8_t *)unity_malloc(*p_size);

	// Funtion returns NULL if couldn't allocate memory.
	if(!p_data)
		return NULL;

	// Hex to bytes.
	long int position = 0;
	fseek(p_file, position, SEEK_SET);
	for(; position < file_size; position += 2)
	{
		fscanf(p_file, "%2hhx", p_data + position/2);
	}

	return p_data;
} /* prepare_reference() */


/*** end of file ***/
