/** @file example_parse_packet.c 
 *
 * @brief Example of using shy_mqtt parsing functions.
 *
 * @copyright Copyright (c) 2026 Pavel Salnikov.
 * @license This project is licensed under the MIT License.
 *  		See the LICENSE file in the root directory for details.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/shy_mqtt.h"
#include "common/shy_preview.h"

static uint8_t *cook_example_connack(size_t *p_size, _Bool SP_flag, uint8_t return_code);
static uint8_t *cook_example_pubrec(size_t *p_size);
static uint8_t *cook_example_pubrel(size_t *p_size);
static uint8_t *cook_example_pubcomp(size_t *p_size);
static uint8_t *cook_example_suback(mqtt_packet_id_t packet_id, size_t *p_size);
static uint8_t *cook_example_publish(size_t *p_size);
static uint8_t *cook_example_unsuback(mqtt_packet_id_t packet_id, size_t *p_size);

int main() 
{
	// Shy-mqtt initialization:
	//
	// Memory allocator and memory free funtions must be provided for shy-mqtt.
	mqtt_set_custom_allocator(malloc);
	mqtt_set_custom_free(free);


	// 0. Declare variables used for example packet generation.
	size_t example_packet_size;
	uint8_t *p_example_packet;	


	// 1. Prepare example CONNACK packet to parse.
	p_example_packet = cook_example_connack(
			&example_packet_size, 
			true, 
			MQTT_CONNACK_CONNECTION_ACCEPTED);

	// Print example CONNACK.
	print_packet("Example CONNACK to parse", p_example_packet, example_packet_size);	
	print_empty_string();

	/* Recognize CONNACK packet.
	 *
	 * NOTE: In this example we know exactly that we have a CONNACK packet by the pointer,
	 * but in normal circumstances we have to recognize type of the packet before parsing.
	 */
	mqtt_packet_type_t packet_type = mqtt_recognize_packet(p_example_packet);

	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED) 
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_CONNACK) 
	{
		// Prepare variables for parsed data.
		_Bool session_present;
		mqtt_return_code_connack_t return_code;

		// Parse the CONNACK.
		return_code = mqtt_parse_connack(p_example_packet, &session_present, example_packet_size);

		// Print parsed data:
		printf("CONNACK parsed data:\n");
		printf("return code: ");
		switch (return_code) 
		{
			// NOTE: this value is returned as the indicator of error.
			case MQTT_CONNACK_FUNCTION_ERROR:
				printf("mqtt_parse_connack() function error: ");
				print_error(mqtt_get_error());		// Handle the error.
				break;

			case MQTT_CONNACK_CONNECTION_ACCEPTED:
				printf("CONNECTION ACCEPTED");
				break;

			case MQTT_CONNACK_UNACCEPTABLE_PROTOCOL:
				printf("UNACCEPTABLE PROTOCOL VERSION");
				break;

			case MQTT_CONNACK_IDENTIFIER_REJECTED:
				printf("IDENTIFIER REJECTED");
				break;

			case MQTT_CONNACK_SERVER_UNAVAILABLE:
				printf("SERVER UNAVAILABLE");
				break;

			case MQTT_CONNACK_BAD_USERNAME_PASS:
				printf("BAD USERNAME OR PASSWORD");
				break;

			case MQTT_CONNACK_NOT_AUTHORIZED:
				printf("NOT AUTHORIZED");
				break;

			default:
				printf("UNKNOWN RETURN CODE");
				break;
		} /* switch (return_code) */
		printf("\nSession Present flag: %d", session_present);
		print_empty_string();
	} /* if (connack) */
	free(p_example_packet);


	// 2. Prepare example SUBACK packet.
	//
	p_example_packet = cook_example_suback(255, &example_packet_size);
	
	// Print example SUBACK packet.
	print_packet("Example SUBACK to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize SUBACK packet.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED)
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_SUBACK)
	{
		// Parse the SUBACK.
		mqtt_return_code_suback_t return_codes[N_TOPICS_TO_SUBSCRIBE];
		mqtt_packet_id_t packet_id = mqtt_parse_suback(
				p_example_packet, 
				return_codes, 
				ARRAY_SIZE(return_codes), 
				example_packet_size
				);

		if (packet_id != 0) 
		{
			// Print parsed data:
			printf("SUBACK parsed data:\n");
			printf("packet ID: %d\n", packet_id);	
			// Go through return code for each topic.
			for (size_t i = 0; i < N_TOPICS_TO_SUBSCRIBE ; i++) 
			{
				printf("topic #%d, Result: ", i);

				switch (return_codes[i]) 
				{
					case MQTT_SUBACK_SUCCESS_QOS_0:
						printf("Success - Maximum QoS 0\n");
						break;

					case MQTT_SUBACK_SUCCESS_QOS_1:
						printf("Success - Maximum QoS 1\n");
						break;

					case MQTT_SUBACK_SUCCESS_QOS_2:
						printf("Success - Maximum QoS 2\n");
						break;

					case MQTT_SUBACK_FAILURE:
						printf("Failure\n");
						break;

					default:
						printf("Invalid value in SUBACK packet");
						break;
				}
			} /* for each return code */
		} /* if parser returned non-zero packet_id (valid) */
		
		else
		{
			// If parser returned packet_id == 0 (error occured).
			printf("mqtt_parse_suback() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();
	
	} /* if (suback) */
	free(p_example_packet);

	// 3: Prepare example UNSUBACK packet to parse.
	//
	p_example_packet = cook_example_unsuback(256, &example_packet_size);

	// Print example UNSUBACK packet.
	print_packet("Example UNSUBACK to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize UNSUBACK packet.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED)
	{
		printf("Couldn't recognize the packet.");
	}
	if (packet_type == MQTT_PACKET_TYPE_UNSUBACK)
	{
		mqtt_packet_id_t packet_id = mqtt_parse_unsuback(p_example_packet, example_packet_size);
		
		// If parser returned non-zero packet_id (valid).
		if (packet_id != 0) 
		{
			// Print parsed data.
			printf("UNSUBACK parsed data:\n");
			printf("packet id: %d", packet_id);
		}
		else 
		{
			// If parser returned packet_id == 0 (error occured).
			printf("mqtt_parse_unsuback() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();
	} /* if (unsuback) */
	free(p_example_packet);


	// 4. Prepare example PUBLISH packet to parse (use shy_mqtt to compose the packet).
	//
	mqtt_ctrl_flags_t control_flags = mqtt_create_control_flags(false, 1, false);
	
	p_example_packet = mqtt_create_publish_packet(
			control_flags, 
			"temperature/sensor1", 
			(const uint8_t*)"20", 
			strlen("20"), 255, 
			&example_packet_size
			);

	// Print example PUBLISH packet.
	print_packet("Example PUBLISH to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize PUBLISH packet.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED) 
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_PUBLISH)
	{
		// Prepare variables to save parsed data.
		uint8_t QoS;
		mqtt_packet_id_t packet_id;
		char *p_topic;
		uint8_t *p_payload;
		size_t payload_length;

		// Parse PUBLISH packet.
		p_payload = mqtt_parse_publish(p_example_packet, &QoS, &packet_id, &p_topic, &payload_length, example_packet_size);

		// If parser returned valid pointer (not NULL).
		if (p_payload != NULL) 
		{
			// Print parsed data.
			printf("PUBLISH parsed data:\n");
			printf("packet id: %d\n", packet_id);
			printf("QoS: %d\n", QoS);
			printf("Topic: %s\n", p_topic);
			printf("Payload as string: %s\n",p_payload);	
			printf("Payload as bytes: ");
			for(size_t i = 0; i < payload_length; i++)
				printf("%02X ", *p_payload + i);
		}
		else 
		{
			// If parser returned NULL (error occured).
			printf("mqtt_parse_publish() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();
	}
	free(p_example_packet);


	// 5. Prepare example PUBACK packet to parse (use shy_mqtt to compose the packet).
	//
	p_example_packet = mqtt_create_puback_packet(256, &example_packet_size);

	// Print example PUBACK.
	print_packet("Example PUBACK to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize PUBACK packet.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED) 
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_PUBACK)
	{
		// Parse Packet ID of the PUBACK.
		mqtt_packet_id_t packet_id = mqtt_parse_puback(p_example_packet, example_packet_size);

		if (packet_id != 0)
		{
			// Print parsed data:
			printf("PUBACK parsed data:\n");
			printf("packet id: %d", packet_id);
		}
		else 
		{
			printf("mqtt_parse_puback() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();
		
	} /* if (puback) */
	free(p_example_packet);


	// 6. Prepare example PUBREC packet to parse.
	p_example_packet = mqtt_create_pubrec_packet(257, &example_packet_size);

	// Print example PUBREC:
	print_packet("Example PUBREC to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize PUBREC.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED)
	{
		printf("Couldn't recognize the packet.");
	}
	if (packet_type == MQTT_PACKET_TYPE_PUBREC)
	{
		// Parse Packet ID of the PUBREC.
		mqtt_packet_id_t packet_id = mqtt_parse_pubrec(p_example_packet, example_packet_size);
		
		// If parser returned non-zero packet id (valid).
		if( packet_id != 0) 
		{
			// Print parsed data:
			printf("PUBREC parsed data:\n");
			printf("packet id: %d", packet_id);
		}
		else 
		{
			// If parser returned packet_id == 0 (error occured).
			printf("mqtt_parse_pubrec() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();
	} /* if (pubrec) */
	free(p_example_packet);


	// 7. Prepare example PUBREL packet to parse.
	p_example_packet = mqtt_create_pubrel_packet(259, &example_packet_size);

	// Print example PUBREL.
	print_packet("Example PUBREL to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize PUBREL.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED) 
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_PUBREL)
	{
		// Parse Packet ID of the PUBREL.
		mqtt_packet_id_t packet_id = mqtt_parse_pubrel(p_example_packet, example_packet_size);

		if (packet_id != 0) 
		{
			// Print parsed data:
			printf("PUBREL parsed data:\n");
			printf("packet id: %d", packet_id);
		}
		else 
		{
			printf("mqtt_parse_pubrel() function error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();

	} /* if (pubrel) */
	free(p_example_packet);

	// 8. Prepare example PUBCOMP packet to parse.
	p_example_packet = mqtt_create_pubcomp_packet(260, &example_packet_size);

	// Print example PUBCOMP.
	print_packet("Example PUBCOMP to parse", p_example_packet, example_packet_size);
	print_empty_string();

	// Recognize PUBCOMP packet.
	packet_type = mqtt_recognize_packet(p_example_packet);
	if (packet_type == MQTT_PACKET_TYPE_NOT_RECOGNIZED) 
	{
		printf("Couldn't recognize the packet");
	}
	if (packet_type == MQTT_PACKET_TYPE_PUBCOMP)
	{
		// Parse Packet ID of the PUBCOMP.
		mqtt_packet_id_t packet_id = mqtt_parse_pubcomp(p_example_packet, example_packet_size);
		
		// If parser returned non-zero packet_id (valid). 
		if (packet_id != 0) 
		{
			// Print parsed data:
			printf("PUBCOMP parsed data:\n");
			printf("packet id: %d", packet_id);
		}
		else 
		{
			// If parser returned packet_id == 0 (error occured).
			printf("mqtt_parse_pubcomp() error: ");
			print_error(mqtt_get_error());
		}
		print_empty_string();

	} /* if (pubcomp) */
	free(p_example_packet);

} /* main() */

static uint8_t *cook_example_connack(size_t *p_size,_Bool SP_flag, uint8_t return_code)
{
	#define CONNACK_PACKET_SIZE 4
	uint8_t *p_connack = (uint8_t*)malloc(CONNACK_PACKET_SIZE);
	p_connack[0] = 0b00100000;
	p_connack[1] = 0b00000010;
	p_connack[2] = 0b00000000;
	if(SP_flag)
		p_connack[2] |= 0b00000001;
	p_connack[3] = return_code;
	*p_size = 4;
	return p_connack;
} /* cook_example_connack() */

static uint8_t *cook_example_suback(mqtt_packet_id_t packet_id, size_t *p_size) 
{
	const size_t SUBACK_PAYLOAD_SIZE = 4;
	const size_t SUBACK_REMAINING_LENGTH = SUBACK_PAYLOAD_SIZE + 2;
	*p_size = SUBACK_REMAINING_LENGTH + 2;	
	uint8_t *p_suback = (uint8_t*)malloc(*p_size);

	p_suback[0] = 0b10010000;					// Packet type = suback.
	p_suback[1] = 0x05;							// Remaining length.
	p_suback[2] = (uint8_t)(packet_id >> 8);	// MSB of packet_id.
	p_suback[3] = (uint8_t)packet_id;			// LSB of packet_id.	
	p_suback[4] = 0x01;							// Return code success with QoS 1.
	p_suback[5] = 0x02;							// Return code success with QoS 2.
	p_suback[6] = 0x00;							// Return code success with QoS 0.
	p_suback[7] = 0x80;							// Return code Failure.
	
	return p_suback;
} /* cook_example_suback() */

static uint8_t *cook_example_unsuback(mqtt_packet_id_t packet_id, size_t *p_size) 
{
	*p_size = 4;
	uint8_t *p_unsuback = (uint8_t*)malloc(*p_size);
	p_unsuback[0] = 0b10110000;
	p_unsuback[1] = 2;
	p_unsuback[2] = (uint8_t)(packet_id >> 8);	// MSB of packet_id.
	p_unsuback[3] = (uint8_t)packet_id;			// LSB of packet_id.
	return p_unsuback;
} /* cook_example_unsuback() */

/*** end of file ***/
