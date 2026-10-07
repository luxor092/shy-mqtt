/*! @file shy_preview.c
 *
 * @brief Functions to preview MQTT packets.
 */

#include "shy_preview.h"
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include "../../src/shy_mqtt.h"

void print_packet(const char *introduction, uint8_t *p_package, const size_t package_size)
{
	if (!package_size) 
	{
		printf("NULL");
		return;
	}

	// Print introduction of the packet.
	printf("%s:\n", introduction);

	// Print the packet.
    uint8_t * p;
    for (size_t i = 0; i < package_size; i++)
    {
        p = p_package + i;
        printf("%02X ", *p);
    }
} /* print_packet() */

void print_packet_partly(const char *introduction, uint8_t *p_packet, const size_t packet_size, 
		const size_t limit)
{
	if (packet_size > limit)
	{
		print_packet(introduction, p_packet, limit);
		printf(" ... %d of %d bytes printed.", limit, packet_size);
	}
	else
		print_packet(introduction, p_packet, packet_size);

} /* print_packet_partly() */

void print_empty_string()
{
    printf("\n\n");
} /* print_empty_string() */

void print_error(mqtt_error_t error)
{
	switch (error) 
	{
		case MQTT_ERR_ZERO_PACKET_ID:
			printf("PACKET ID = 0. Must be non-zero.");
			break;

		case MQTT_ERR_INVALID_POINTER:
			printf("Invalid pointer to the packet. Pointer must be not NULL.");
			break;

		case MQTT_ERR_PACKET_TYPE_LENGTH_MISMATCH:
			printf("Invalid Remaining Length value for this type of packet.");
			break;

		case MQTT_ERR_OUTSIDE_BOUNDARY:
			printf("Remaining Length is too big and forces the parser to read outside of available range.");
			break;

		case MQTT_ERR_INVALID_TOPIC_LENGTH:
			printf("Zero topic length is not allowed.");
			break;

		case MQTT_ERR_INVALID_REMAINING_LENGTH:
			printf("The packet had invalid remaining length field.");
			break;

		case MQTT_ERR_MALLOC_FAULT:
			printf("Couldn't allocate dynamic memory.");
			break;

		case MQTT_ERR_INVALID_PAYLOAD:
			printf("Payload can't be null.");
			break;

		default:
			printf("Unknown error.");
			break;
	}
}


/*** end of file ***/
