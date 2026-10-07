/** @file example_create_packet.c
 *
 * @brief Example of using shy_mqtt generating functions.
 * 
 * @copyright Copyright (c) 2026 Pavel Salnikov.
 * @license This project is licensed under the MIT License.
 * 			See the LICENSE file in the root directory for details.
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "../src/shy_mqtt.h"
#include "common/shy_preview.h"

#define PRINT_LONG_PACKETS_COMPLETELY 0
#define USE_ANONYMOUS_ARRAY 0

int main() 
{
	/* 
	 * Pass memory allocator function.
	 * 
	 * NOTE: This example uses malloc() as memory allocator function.
	 * For MCUs, prefer using an allocator function that doesn't utilize heap memory. 
	 */
	mqtt_set_custom_allocator(malloc);

    // Declare variables to store size of created packets.
    size_t connect_packet_size,
		   publish_packet_size,
		   subscribe_packet_size,
		   unsubscribe_packet_size,
		   puback_packet_size,
		   pubrec_packet_size,
		   pubrel_packet_size,
		   pubcomp_packet_size,
		   pingreq_packet_size,
		   disconnect_packet_size;

	// Common packet ID for all the example packets.
	mqtt_packet_id_t packet_id = 256;


		/* GENERATE MQTT PACKETS */

	// CONNECT packet example.
	//
	// Prepare connect flags for the connect packet.
	mqtt_conn_flags_t connect_flags = mqtt_create_connect_flags(true, true, true, 2, true, true);
	
	// Prepare connect credentials for the connect packet.
	mqtt_conn_credentials_t connect_credentials = mqtt_create_connect_credentials(
			"your_client_id",
			"your_username",
			"your_password"
			);

	// Keepalive value.
	uint16_t keepalive = 1200;

	// Create connect packet.
	uint8_t *p_connect_packet = mqtt_create_connect_packet(
			connect_flags,
			connect_credentials,
			keepalive,
			"your last will topic",
			"your last will message",
			&connect_packet_size
			);

	if (p_connect_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_connect_packet() function error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print connect packet.
		print_packet("CONNECT packet", p_connect_packet, connect_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_connect_packet);
	}
	print_empty_string();


	// SUBSCRIBE packet example.
	//
#if USE_ANONYMOUS_ARRAY
	
	uint8_t *p_subscribe_packet = mqtt_create_subscribe_packet(
			packet_id,
			(const char *[]){"the_first_topic", "the_second_topic/#"},  /* array of topics */
			2, 														    /* number of topics */
			(uint8_t []){1,0}, 											/* array of QoS */
			&subscribe_packet_size
			);

#else
	
	// Create the array of the topics to subscribe.
	const char *subscribe_topics[] = {"the_first_topic", "the_second_topic/#"};

	// Create the array of QoS for each topic respectively.
	const uint8_t QoS_array[] = {1,0};

	// Create subscribe packet.
	uint8_t *p_subscribe_packet = mqtt_create_subscribe_packet(
			packet_id, 
			subscribe_topics, 
			ARRAY_SIZE(subscribe_topics), 
			QoS_array,
			&subscribe_packet_size);

#endif

	if (p_subscribe_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_subscribe_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		print_packet("SUBSCRIBE packet", p_subscribe_packet, subscribe_packet_size);
		free(p_subscribe_packet);
	}
	print_empty_string();


	// UNSUBSCRIBE packet example.
	//
#if USE_ANONYMOUS_ARRAY

	uint8_t *p_unsubscribe_packet = mqtt_create_unsubscribe_packet(
			1,															// Packet ID.
			(const char *[]){"the_first_topic", "the_second_topic"},	// Topics to unsubscribe.
			2,															// Number of topics to unsubscribe.
			&unsubscribe_packet_size
			);

#else

	// Create the array of the topics to unsubscribe.
	const char *unsubscribe_topics[] = {"the_first_topic", "the_second_topic"};

	// Create the unsubscribe packet.
	uint8_t *p_unsubscribe_packet = mqtt_create_unsubscribe_packet(
			packet_id,
			unsubscribe_topics,	
			ARRAY_SIZE(unsubscribe_topics),
			&unsubscribe_packet_size
			);

	if (p_unsubscribe_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_unsubscribe_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		print_packet("UNSUBSCRIBE packet", p_unsubscribe_packet, unsubscribe_packet_size);
		free(p_unsubscribe_packet);
	}

	print_empty_string();

#endif

	// PUBLISH packet example.
	//
	// Prepare the payload to be sent as part of the publish packet.
	const size_t EXAMPLE_PAYLOAD_SIZE = 500;
	uint8_t *p_example_payload = (uint8_t*)malloc(EXAMPLE_PAYLOAD_SIZE);

	// Prepare control flags for the publish packet (Duplicate = false, QoS = 1, retain = false).
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);
	
	// Create publish packet.
	uint8_t *p_publish_packet = mqtt_create_publish_packet(
			control_flags,
			"mytopic", 
			p_example_payload,
			EXAMPLE_PAYLOAD_SIZE,
			packet_id,
			&publish_packet_size
			);

	// As the publish packet comprises payload, the memory allocated for the payload may be freed.
	free(p_example_payload);

	if (p_publish_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_publish_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print publish packet.

#if PRINT_LONG_PACKETS_COMPLETELY
		print_packet("PUBLISH packet", p_publish_packet, publish_packet_size);
#else
		print_packet_partly("PUBLISH packet", p_publish_packet, publish_packet_size, 34);
#endif 
	
		// Release memory allocated for the packet after the packet has been utilized.
		free(p_publish_packet); 
	}
	print_empty_string();


	// PUBACK packet example.
	//
	uint8_t *p_puback_packet = mqtt_create_puback_packet(packet_id, &puback_packet_size);

	if (p_puback_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_puback_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print puback packet.
		print_packet("PUBACK packet", p_puback_packet, puback_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_puback_packet);
	}

	print_empty_string();


	// PUBREC packet example.
	//
	uint8_t *p_pubrec_packet = mqtt_create_pubrec_packet(packet_id, &pubrec_packet_size);
	
	if (p_pubrec_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_pubrec_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print pubrec packet.
		print_packet("PUBREC packet", p_pubrec_packet, pubrec_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_pubrec_packet);
	}
	print_empty_string();


	// PUBREL packet example.
	//
	uint8_t *p_pubrel_packet = mqtt_create_pubrel_packet(packet_id, &pubrel_packet_size);
	
	if (p_pubrel_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_pubrel_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print pubrel packet.
		print_packet("PUBREL packet", p_pubrel_packet, pubrel_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_pubrel_packet);
	}
	print_empty_string();

	// PUBCOMP packet example.
	//
	uint8_t *p_pubcomp_packet = mqtt_create_pubcomp_packet(packet_id, &pubcomp_packet_size);
	
	if (p_pubcomp_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_pubcomp_packet() error: ");
		print_error(mqtt_get_error());
	}
	else {
		// Print pubcomp packet.
		print_packet("PUBCOMP packet", p_pubcomp_packet, pubcomp_packet_size);
	
		// Release memory allocated for the packet after the packet has been utilized.
		free(p_pubcomp_packet);
	}
	print_empty_string();


	// PINGREQ packet example.
	//
	uint8_t *p_pingreq_packet = mqtt_create_pingreq_packet(&pingreq_packet_size);
	
	if (p_pingreq_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_pingreq_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print pingreq packet.
		print_packet("PINGREQ packet", p_pingreq_packet, pingreq_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_pingreq_packet);
	}
	print_empty_string();


	// DISCONNECT packet example.
	//
	uint8_t *p_disconnect_packet = mqtt_create_disconnect_packet(&disconnect_packet_size);
	
	if (p_disconnect_packet == NULL) 
	{
		// Print error.
		printf("mqtt_create_disconnect_packet() error: ");
		print_error(mqtt_get_error());
	}
	else 
	{
		// Print disconnect packet.
		print_packet("DISCONNECT packet", p_disconnect_packet, disconnect_packet_size);

		// Release memory allocated for the packet after the packet has been utilized.
		free(p_disconnect_packet);
	}
	print_empty_string();

    return 0;
} /* main() */


/*** end of file ***/
