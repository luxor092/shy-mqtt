/** @file shy_mqtt.c
 * 
 * @brief Implementation of the MQTT parser and generator.
 *
 * @copyright Copyright (c) 2026 Pavel Salnikov. 
 * @license This project is licensed under the MIT License.
 * 			See the LICENSE file in the root directory for details.
 */

#include "shy_mqtt.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define LENGTH_FIELD_PREFIX 2                   // Two bytes for field prefix to store size of the field.
#define LENGTH_PACKET_ID 2						// Packet Identifier takes two bytes.
#define LENGTH_PACKET_TYPE 1					// Packet Type takes one byte.
#define LENGTH_QOS 1							// QoS takes one byte.
#define LENGTH_PROTOCOL_NAME 4                  // Presumed protocol name "MQTT".
#define LENGTH_PROTOCOL_LEVEL 1                 // Presumed protocol level = 4 (one byte to code value 0×04).
#define LENGTH_CONNECT_FLAGS 1
#define LENGTH_KEEPALIVE 2
#define LENGTH_PUBX_PACKET 4					// Size of PUBACK, PUBREC, PUBREL, PUBCOMP packets.
#define LENGTH_CONNACK_PACKET 4
#define LENGTH_CONTROL_HEADER 1
#define PROTOCOL_LEVEL 4                        // Presume protocol level = 4 (mqtt v 3.1.1).

// Packet offsets.
#define PACKET_TYPE 	 0
#define REMAINING_LENGTH 1

// Packet types.
#define CONNECT		0b00010000
#define CONNACK 	0b00100000
#define PUBLISH 	0b00110000
#define PUBACK 		0b01000000
#define PUBREC 		0b01010000
#define PUBREL 		0b01100010
#define PUBCOMP 	0b01110000
#define SUBSCRIBE 	0b10000010
#define SUBACK 		0b10010000
#define UNSUBSCRIBE 0b10100010
#define UNSUBACK 	0b10110000
#define PINGREQ 	0b11000000
#define PINGRESP 	0b11010000
#define DISCONNECT	0b11100000

#define INITIALIZE_AUX_POINTERS uint8_t *p, *p_out; \
	p = p_packet; \
	p_out = p_packet + data_size; \

#define VALIDATE_PACKET_POINTER if (!p_packet) \
{ \
	mqtt_error = MQTT_ERR_INVALID_POINTER; \
	return error; \
}

#define VALIDATE_BOUNDARY(n_bytes) if (p + n_bytes > p_out) \
{ \
	mqtt_error = MQTT_ERR_OUTSIDE_BOUNDARY; \
	return error; \
}

#define VALIDATE_REMAINING_LENGTH if (!length_remaining) {return error;}

#define VALIDATE_PACKET_ID if (packet_id == 0) \
{ \
	mqtt_error = MQTT_ERR_ZERO_PACKET_ID; \
	return NULL; \
}

static mqtt_error_t mqtt_error = MQTT_ERR_NO_ERROR;
const char *p_LAST_WILL_TOPIC_DEFAULT = "status";
const char *p_LAST_WILL_MESSAGE_DEFAULT = "device signal lost";

/*!
 * Pointer to a memory allocation function. 
 * For embedded systems consider assigning no-heap memory allocation function.
 */
static void * (*memory_allocate)(size_t);

/*!
 * Pointer to a memory release function.
 * For embedded systems consider assigning no-heap memory allocator/free functions.
 */
static void (*memory_free)(void *);

static size_t parse_remaining_length(uint8_t *p, size_t data_size);
static uint8_t *write_remaining_length(uint8_t *p_destination, size_t length);
static uint8_t *write_packet_id(uint8_t *p_destination, mqtt_packet_id_t packet_id);
static mqtt_packet_id_t read_packet_id(uint8_t *p_source);
static size_t compute_self_length(size_t length);
static uint8_t *write_field(uint8_t *p_destination, const char *p_source);
static uint8_t *create_pubx_packet(uint8_t packet_type, mqtt_packet_id_t packet_id, size_t *p_packet_size);
static mqtt_packet_id_t parse_pubx(uint8_t *p_packet, size_t data_size);

mqtt_error_t mqtt_get_error() {return mqtt_error;}

void mqtt_set_custom_allocator(void * (*p_allocator)(size_t))
{
	memory_allocate = p_allocator;
} /* mqtt_set_custom_allocator() */

void mqtt_set_custom_free(void (*p_free)(void*)) 
{
	memory_free = p_free;
} /* mqtt_set_custom_free() */

mqtt_conn_flags_t mqtt_create_connect_flags(
		_Bool username, 
		_Bool password,
		_Bool will_retain,
		uint8_t will_QoS,
		_Bool will_send,
		_Bool clean_session
		)
{
    // Create and reset connect flags.
    mqtt_conn_flags_t connect_flags;
    connect_flags.value = 0;

    // Modify connect flags.
    if (username)
        connect_flags.value |= 0b10000000;
    if (password)
        connect_flags.value |= 0b01000000;
    if (will_retain)
        connect_flags.value |= 0b00100000;
    if (will_send)
        connect_flags.value |= 0b00000100;
    if (clean_session)
        connect_flags.value |= 0b00000010;

    // Reset QoS if it has invalid value:
    if (will_QoS > 2)
        will_QoS = 0;

    // Modify QoS connect flag.
    connect_flags.value |= (will_QoS << 3);
 
    return connect_flags;
} /* mqtt_create_connect_flags() */

mqtt_conn_credentials_t mqtt_create_connect_credentials(
		const char *p_client_id,
		const char *p_username,
		const char *p_userpass
		)
{
	mqtt_conn_credentials_t connect_credentials;
	connect_credentials.p_client_id = p_client_id;
	connect_credentials.p_username = p_username;
	connect_credentials.p_userpass = p_userpass;
	return connect_credentials;
} /* mqtt_create_connect_credentials() */

mqtt_ctrl_flags_t mqtt_create_control_flags(_Bool Dup, uint8_t QoS, _Bool retain)
{
    // Create and reset control flags.
    mqtt_ctrl_flags_t control_flags;
    control_flags.value = 0;

    // Modify control flags.
    if (Dup)
        control_flags.bits.b3_DUP = 1;
    if (retain)
        control_flags.bits.b0_retain = 1;
    
    if (QoS == 2) {

        control_flags.bits.b2_QoS = 1;
        control_flags.bits.b1_QoS = 0;
    } else if (QoS == 1) {

        control_flags.bits.b1_QoS = 1;
    }
    
    return control_flags; 
} /* mqtt_create_control_flags() */

uint8_t *mqtt_create_connect_packet(
		mqtt_conn_flags_t connect_flags,
		mqtt_conn_credentials_t connect_credentials,
		uint16_t keepalive,
		const char *p_last_will_topic,
		const char *p_last_will_message,
		size_t *p_packet_size
		)
{

	uint8_t *error = NULL;

    // Variables to store lengths.
    size_t length_fixed_header;                       ///< length of fixed_header: package_type&control_flags(1) + length_of_remaining_length(1..4).
    size_t length_remaining;
    size_t length_variable_header;
    size_t length_payload;

    						/* Calculate payload length [MQTT-3.1.3] */

	// Create local variables, which store field lengths including their prefixes.
    size_t length_client_id, length_will_topic, length_will_message, length_username, length_password;

    // Client id is always required [MQTT-3.1.3-3].
	if (connect_credentials.p_client_id == NULL)
	{
		mqtt_error = MQTT_ERR_INVALID_CLIENT_ID;
		return error;
	}
    length_client_id = strlen(connect_credentials.p_client_id) + LENGTH_FIELD_PREFIX;
	// If user passed empty client_id.
	if (length_client_id - LENGTH_FIELD_PREFIX == 0)
	{
		mqtt_error = MQTT_ERR_INVALID_CLIENT_ID;
		return error;
	}

    // If last will is required.
    if (connect_flags.bits.b2_will_flag)
    {
		// If user didn't pass the last will topic, use the default one.
		if (p_last_will_topic == NULL)
			p_last_will_topic = p_LAST_WILL_TOPIC_DEFAULT;

		// If user didn't pass the last will message, use the default one.
		if (p_last_will_message == NULL)
			p_last_will_message = p_LAST_WILL_MESSAGE_DEFAULT;

        length_will_topic = strlen(p_last_will_topic) + LENGTH_FIELD_PREFIX;
        length_will_message = strlen(p_last_will_message) + LENGTH_FIELD_PREFIX;
    }
    else
    {
    	length_will_topic = 0;
        length_will_message = 0;
    }

    // If username is required:
    if (connect_flags.bits.b7_username)
	{
        if (connect_credentials.p_username == NULL)
		{
			mqtt_error = MQTT_ERR_INVALID_USERNAME;
			return error;
		}
		length_username = strlen(connect_credentials.p_username) + LENGTH_FIELD_PREFIX;
		if (length_username - LENGTH_FIELD_PREFIX == 0)
		{
			mqtt_error = MQTT_ERR_INVALID_USERNAME;
			return error;
		}

	}
    else
        length_username = 0;

    // If password is required:
    if (connect_flags.bits.b6_password)
	{
		if (connect_credentials.p_userpass == NULL)
		{
			mqtt_error = MQTT_ERR_INVALID_PASSWORD;
			return error;
		}
        length_password = strlen(connect_credentials.p_userpass) + LENGTH_FIELD_PREFIX;
		if (length_password - LENGTH_FIELD_PREFIX == 0)
		{
			mqtt_error = MQTT_ERR_INVALID_PASSWORD;
			return error;
		}
	}
    else
        length_password = 0;

	// Calculate payload length.
    length_payload = length_client_id + length_will_topic + length_will_message + length_username 
		+ length_password;

							/* End of payload length calculation */

	// Calculate Variable header length [see mqtt 3.1.2].
    length_variable_header = LENGTH_FIELD_PREFIX + LENGTH_PROTOCOL_NAME + LENGTH_PROTOCOL_LEVEL 
		+ LENGTH_CONNECT_FLAGS + LENGTH_KEEPALIVE;

    // Calculate Remaining length (Variable Header + Payload).
    length_remaining = length_variable_header + length_payload;

    // Fixed_header_length calculation.
    length_fixed_header = 1 + compute_self_length(length_remaining);    // 1 byte added for "control flags bits & reserved bits".

    // Total length of MQTT packet.
    *p_packet_size = length_fixed_header + length_remaining;

    // Assign memory for MQTT packet.
    uint8_t *p_package;
    uint8_t *p;
	assert(memory_allocate != NULL);
	p_package = (uint8_t*)memory_allocate(*p_packet_size);
	if (!p_package) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}
    p = p_package;
 
    				/* Fill the fixed header. */

    *p = 0b00010000;  // 0b0001xxxx - MQTT packet type "connect". Four less significant bits are reserved zeroes.
    p ++;
	p = write_remaining_length(p,length_remaining);

    				/* Fill the variable header. */

    p = write_field(p, "MQTT");
	*p = PROTOCOL_LEVEL;            //protocol level equals 4, when using mqtt v3.1.1
    p ++;
    *p = connect_flags.value;
    p ++;

	p[0] = (uint8_t)(keepalive >> 8);
	p[1] = (uint8_t)keepalive;
	p += 2;

    				/* Fill the payload. */

    // Client identifier.
    p = write_field(p, connect_credentials.p_client_id);
 
    // Last will (will topic and will message).
    if (connect_flags.bits.b2_will_flag)
    {
        p = write_field(p, p_last_will_topic);
        p = write_field(p, p_last_will_message);
    }

    // Username.
    if (connect_flags.bits.b7_username) 
		p = write_field(p, connect_credentials.p_username);

    // Password.
    if (connect_flags.bits.b6_password) 
		write_field(p, connect_credentials.p_userpass);

     /* End of poyload filling */

    return p_package;
} /* mqtt_create_connect_packet() */

uint8_t *mqtt_create_publish_packet(
		mqtt_ctrl_flags_t control_flags, 
		const char *p_topic_name, 
		const uint8_t *p_payload,
		size_t payload_size, 
		uint16_t packet_id, 
		size_t *p_packet_size
		)
{
	uint8_t *error = NULL;

	// Topic must present.
	if (p_topic_name == NULL)
	{
		mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
		return error;
	}

	// Payload must present.
	if (p_payload == NULL || payload_size == 0) 
	{
		mqtt_error = MQTT_ERR_INVALID_PAYLOAD;
		return error;
	}
	
    // Declare variables for length calculation.
    size_t length_remaining = 0;
    uint8_t packet_id_length;
    _Bool packet_id_required;
    size_t topic_length = strlen(p_topic_name);
 
    // Is the "packet_id" field required?
    if(control_flags.bits.b2_QoS || control_flags.bits.b1_QoS) 
    {
        packet_id_required = true;
        packet_id_length = LENGTH_PACKET_ID;
    }
    else 
    {
        packet_id_required = false;
        packet_id_length = 0;
    }

	// Packet it must be non-zero when it presents.
	if (packet_id_required && packet_id == 0)
	{
		mqtt_error = MQTT_ERR_ZERO_PACKET_ID;
		return NULL;
	}

    // Calculate the Remaining length of the packet.
    length_remaining = LENGTH_FIELD_PREFIX + topic_length + packet_id_length + payload_size;

	// Calculate the fixed header length.
	size_t length_fixed_header = LENGTH_PACKET_TYPE + compute_self_length(length_remaining);

    // Calculate total length of the packet.
    *p_packet_size = length_fixed_header + length_remaining;

    // Assign memory for the packet.
	assert(memory_allocate != NULL);
    uint8_t* p_packet = memory_allocate(*p_packet_size);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}

    uint8_t *p = p_packet;

    						/* Fill the Fixed header. */

	// Fill the packet type + flags field.
    *p = 0b00110000; 				// 0b0011xxxx : packet type = publish.
    *p ^= control_flags.value; 		// 4 less significat bits taken from control flags.
 	p ++;
 
	// Fill remaining length.
	p = write_remaining_length(p,length_remaining);

							/* Fill the Variable header. */

    // Field: topic name.
    p = write_field(p,p_topic_name);
 
	// Field: packet identifier.
	if (packet_id_required)
		p =	write_packet_id(p, packet_id);

						/* Fill the Payload */

    for (size_t i = 0; i < payload_size; i++)
		p[i] = p_payload[i];

    // Return pointer to the packet.
    return p_packet;
} /* mqtt_create_publish_packet() */
uint8_t *mqtt_create_subscribe_packet(
		mqtt_packet_id_t packet_id,
		const char *p_topic[],
		size_t n_topics,
		const uint8_t requested_QoS[],
        size_t *p_packet_size
		)
{
	uint8_t *error = NULL;
	VALIDATE_PACKET_ID;

	// At least one topic.
	if (n_topics == 0 || p_topic == NULL)
	{
		mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
		return error;
	}

	// Calculate Payload length and check validity of each topic.
	size_t length_payload = 0;
	size_t current_topic_length = 0;
	for (size_t i = 0; i < n_topics ; i ++)
	{
		if(p_topic[i] == NULL)
		{
			mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
			return error;
		}
		current_topic_length = strlen(p_topic[i]);
		if(current_topic_length == 0)
		{
			mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
			return error;
		}
		length_payload += current_topic_length;
	}
	length_payload += (LENGTH_FIELD_PREFIX + LENGTH_QOS) * n_topics;

    // Calculate Remaining length.
    size_t length_remaining = LENGTH_PACKET_ID + length_payload;

	// Calculate Total Packet length.
	*p_packet_size = length_remaining + compute_self_length(length_remaining) +
		LENGTH_PACKET_TYPE;

    // Create the packet.
	assert(memory_allocate != NULL);
    uint8_t *p_packet = (uint8_t*)memory_allocate(*p_packet_size);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}

    // Create pointer.
    uint8_t *p = p_packet;

    // Fill the fixed header.
    *p = SUBSCRIBE;
    p ++;
	p = write_remaining_length(p, length_remaining);

	// Fill the variable header (Packet id).
	p = write_packet_id(p, packet_id);

	// Fill the payload (topic fields and matching QoS').
	for (size_t i = 0; i < n_topics; i++)
	{
		p = write_field(p, p_topic[i]);
		*p = requested_QoS[i];
		p ++;
	}

    return p_packet;
} /* mqtt_create_subscribe() */

uint8_t *mqtt_create_unsubscribe_packet(
		mqtt_packet_id_t packet_id,
		const char *p_topic[],
		size_t n_topics,
		size_t *p_packet_size
		)
{
	uint8_t *error = NULL;
	VALIDATE_PACKET_ID;
	
	// At least one topic.
	if (n_topics == 0 || p_topic == NULL)
	{
		mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
		return error;
	}

	// Calculate Payload length.
	size_t length_payload = 0;
	for (size_t i = 0; i < n_topics ; i ++)
	{
		length_payload += strlen(p_topic[i]);
	}
	length_payload += LENGTH_FIELD_PREFIX * n_topics;

	// Calculate the Remaining length and Packet length.
	size_t length_remaining = length_payload + LENGTH_PACKET_ID;
	size_t length_packet = length_remaining + compute_self_length(length_remaining) + LENGTH_PACKET_TYPE;

	// Create the Packet.
	uint8_t *p_packet = memory_allocate(length_packet);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}
	*p_packet_size = length_packet;
	
	// Fill the packet with data.
	// Packet type.
	p_packet[0] = 0b10100010;		// TODO: use literals.

	// Remaining Lenth
	uint8_t *p = write_remaining_length(p_packet + 1, length_remaining);
	
	// Packet ID.
	p = write_packet_id(p, packet_id);

	// Payload
	for (size_t i = 0; i < n_topics ; i ++)
	{
		p = write_field(p, p_topic[i]);
	}

	return p_packet;
} /* mqtt_create_unsubscribe_packet() */

uint8_t *mqtt_create_puback_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size)
{
	return	create_pubx_packet(PUBACK, packet_id, p_packet_size);
} /* mqtt_create_puback_packet() */

uint8_t *mqtt_create_pubrec_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size)
{
	return create_pubx_packet(PUBREC, packet_id, p_packet_size);
} /* mqtt_create_pubrec_packet() */

uint8_t *mqtt_create_pubrel_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size)
{
	return create_pubx_packet(PUBREL, packet_id, p_packet_size);
} /* mqtt_create_pubrel_packet() */

uint8_t *mqtt_create_pubcomp_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size)
{
	return create_pubx_packet(PUBCOMP, packet_id, p_packet_size);
} /* mqtt_create_pubcomp_packet() */

uint8_t *mqtt_create_pingreq_packet(size_t *p_packet_size)
{	
	uint8_t *error = NULL;
	assert(memory_allocate != NULL);
	uint8_t *p_packet = (uint8_t*)memory_allocate(2);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}
	p_packet[0] = 0b11000000;	// TODO: use literals.
	p_packet[1] = 0x00;
	*p_packet_size = 2;			// Pingreq packet size is const 2;
    return p_packet;
} /* mqtt_create_pingreq_packet() */

uint8_t *mqtt_create_disconnect_packet(size_t *p_packet_size)
{
	uint8_t *error = NULL;
	assert(memory_allocate != NULL);
    uint8_t *p_packet = memory_allocate(2);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}
    p_packet[0] = 0b11100000;
    p_packet[1] = 0x00;		// Remaining length value const 0.
	*p_packet_size = 2;			// Disconnect packet size const 2.
	return p_packet;
} /* mqtt_create_disconnect_packet() */

mqtt_packet_type_t mqtt_recognize_packet(uint8_t *p_packet)
{
	mqtt_packet_type_t error = MQTT_PACKET_TYPE_NOT_RECOGNIZED;
	VALIDATE_PACKET_POINTER;

    switch (p_packet[PACKET_TYPE])
    {
        case CONNACK:
            return MQTT_PACKET_TYPE_CONNACK;

		case PUBACK:
            return MQTT_PACKET_TYPE_PUBACK;

		case SUBACK:
            return MQTT_PACKET_TYPE_SUBACK;

		case UNSUBACK:
			return MQTT_PACKET_TYPE_UNSUBACK;

		case PINGRESP:
			if (p_packet[REMAINING_LENGTH] == 0x00)
				return MQTT_PACKET_TYPE_PINGRESP;
			else
				return MQTT_PACKET_TYPE_NOT_RECOGNIZED; 

		case PUBREC:
			return MQTT_PACKET_TYPE_PUBREC;

		case PUBREL:
			return MQTT_PACKET_TYPE_PUBREL;

		case PUBCOMP:
			return MQTT_PACKET_TYPE_PUBCOMP;
		
		default:
			/* NOTE: four less significant bits of PUBLISH packet may vary.
			 * Reset the four less significant bits and then compare to the PUBLISH
			 * type. */
			if ((p_packet[PACKET_TYPE] & 0b11110000) == PUBLISH)
				return MQTT_PACKET_TYPE_PUBLISH;

			return MQTT_PACKET_TYPE_NOT_RECOGNIZED;
    }
} /* mqtt_recognize_packet() */

mqtt_return_code_connack_t mqtt_parse_connack(uint8_t *p_packet, _Bool *p_SP, size_t data_size)
{	
	const mqtt_return_code_connack_t error = MQTT_CONNACK_FUNCTION_ERROR;
	VALIDATE_PACKET_POINTER;
	INITIALIZE_AUX_POINTERS;

	p += LENGTH_CONTROL_HEADER;

	size_t length_remaining = parse_remaining_length(p, data_size);
	VALIDATE_REMAINING_LENGTH;

	// For CONNACK packets remaining length must equal 2.
	if (length_remaining != 2)
	{
		mqtt_error = MQTT_ERR_PACKET_TYPE_LENGTH_MISMATCH;
		return error;
	}
	p += compute_self_length(length_remaining);
	
	// Validate Session Present flag (1 byte).
	VALIDATE_BOUNDARY(1);
	// Pass Session Present flag if required.
	if (p_SP)
		*p_SP = *p;
	p += 1;

	// Validate Return Code.
	VALIDATE_BOUNDARY(1);
	return (mqtt_return_code_connack_t)*p;
} /* mqtt_parse_connack() */

mqtt_packet_id_t mqtt_parse_puback(uint8_t *p_packet, size_t data_size)
{
	return parse_pubx(p_packet, data_size);
} /* mqtt_parse_puback() */

mqtt_packet_id_t mqtt_parse_pubrec(uint8_t *p_packet, size_t data_size)
{
	return parse_pubx(p_packet, data_size);
} /* mqtt_parse_pubrec() */

mqtt_packet_id_t mqtt_parse_pubrel(uint8_t *p_packet, size_t data_size)
{
	return parse_pubx(p_packet, data_size);
} /* mqtt_parse_pubrel() */

mqtt_packet_id_t mqtt_parse_pubcomp(uint8_t *p_packet, size_t data_size)
{
	return parse_pubx(p_packet, data_size);
} /* mqtt_parse_pubcomp() */

mqtt_packet_id_t mqtt_parse_suback(
		uint8_t *p_packet, 
		mqtt_return_code_suback_t suback_return_codes[],
		size_t array_capacity,
		size_t data_size
		)
{
	const mqtt_packet_id_t error = 0;
	VALIDATE_PACKET_POINTER;
	INITIALIZE_AUX_POINTERS;

	p += LENGTH_CONTROL_HEADER;

	// Calculate remaining length.
	size_t length_remaining = parse_remaining_length(p, data_size);
	VALIDATE_REMAINING_LENGTH;
	p += compute_self_length(length_remaining);

	// Fill packet id.
	VALIDATE_BOUNDARY(LENGTH_PACKET_ID);
	mqtt_packet_id_t packet_id = read_packet_id(p);
	p += LENGTH_PACKET_ID;

	// Calculate payload length.
	size_t length_payload = length_remaining - LENGTH_PACKET_ID; 

	VALIDATE_BOUNDARY(length_payload);
	 
 	// Copy suback return codes to the provided array.
	for (size_t i = 0 ; i < length_payload && i < array_capacity ; i++) 
	{
		suback_return_codes[i] =(mqtt_return_code_suback_t)p[i];
		// TODO: raise an error if value is other that 0x00, 0x01, 0x02, 0x80.
	}

	return packet_id;
} /* mqtt_parse_suback() */

mqtt_packet_id_t mqtt_parse_unsuback(uint8_t *p_packet, size_t data_size)
{
	return parse_pubx(p_packet, data_size);
} /* mqtt_parse_unsuback() */

uint8_t *mqtt_parse_publish(uint8_t *p_packet,
	uint8_t *p_QoS,
	mqtt_packet_id_t *p_packet_id,
	char **pp_topic,
	size_t *p_payload_length,
	size_t data_size
	)
{
	assert(memory_allocate != NULL);
	assert(memory_free != NULL);

	uint8_t *error = NULL;
	VALIDATE_PACKET_POINTER;
	INITIALIZE_AUX_POINTERS;

	// Find QoS.
	VALIDATE_BOUNDARY(LENGTH_CONTROL_HEADER);
	uint8_t QoS = ((*p & 0b00000110) >> 1);
	if (p_QoS != NULL)
		*p_QoS = QoS;
	p += LENGTH_CONTROL_HEADER;

	/* 
	 * Find remaining length.
	 * 
	 * NOTE: shouldn't do boundary validation for this step because the parse_remaining_length() 
	 * function does it on it's own. 
	 */
	size_t length_remaining = parse_remaining_length(p, data_size);
	if (!length_remaining)
		return error;
	uint8_t length_of_remaining_length = compute_self_length(length_remaining);
	p += length_of_remaining_length;					// Jump to topic length.

	// Find topic length.
	VALIDATE_BOUNDARY(LENGTH_FIELD_PREFIX);
	size_t length_topic = 0;
	length_topic += p[1];					// Copy LSB of topic length.
	length_topic += (size_t)p[0] << 8;		// Add MSB of topic length.
	if (length_topic == 0)
	{
		// Zero topic length is not allowed.
		mqtt_error = MQTT_ERR_INVALID_TOPIC_LENGTH;
		return error;
	}
	p += LENGTH_FIELD_PREFIX;				// Jump to the topic.

	// Assign memory for topic and copy the topic.
	VALIDATE_BOUNDARY(length_topic);
	char *p_topic_string = NULL;
	const uint8_t LENGTH_STRING_TERMINATOR = 1;
	assert(memory_allocate != NULL);

		// make sure user passed the addres of pointer that is meant to point to the topic string.
	if (pp_topic != NULL)
	{
		p_topic_string = (char*)memory_allocate(length_topic + LENGTH_STRING_TERMINATOR);
		if (p_topic_string == NULL)
		{
			mqtt_error = MQTT_ERR_MALLOC_FAULT;
			return error;
		}
		memcpy(p_topic_string, p, length_topic);
		p_topic_string[length_topic] = '\0';
		*pp_topic = p_topic_string;	// Fill topic string pointer of the caller funtion with a valid value.
	}
	p += length_topic;

	// Packet ID field presents when QoS > 0.
	uint8_t length_packet_id = 0;
	mqtt_packet_id_t packet_id = 0;					// Initialize packet id value.
	if (QoS > 0)
	{
		VALIDATE_BOUNDARY(LENGTH_PACKET_ID);
		packet_id = read_packet_id(p);
		if (p_packet_id)								// When user provided variable to packet_id pointer
			*p_packet_id = packet_id;					// copy packet_id value by the pointer.

		if (packet_id == 0)								// When parsed zero packet id (error).
		{
			if (p_topic_string)							
				memory_free(p_topic_string);			// Clear memory allocated to store the topic.
			return error;
		}

		p += LENGTH_PACKET_ID;							// Jump to Payload.
		length_packet_id = LENGTH_PACKET_ID;			// Packet ID takes two bytes when presents.
	}

	size_t length_variable_header = length_packet_id + LENGTH_FIELD_PREFIX + length_topic;

	// Calculate payload length as [remaining length - variable header length].
	size_t payload_length_claimed = length_remaining - length_variable_header;
	
	// Calculate payload length using data_size provided by the caller function.
	size_t payload_length_real = data_size - LENGTH_CONTROL_HEADER - length_of_remaining_length -
		length_variable_header;

	*p_payload_length = payload_length_real;

	// Check if the packet claimed too big payload lenght.
	if (payload_length_claimed > payload_length_real) 
	{
		mqtt_error = MQTT_ERR_OUTSIDE_BOUNDARY;
		return error;
	}

	// Return pointer to payload.
	return p;
} /* mqtt_parse_publish() */

/*!
* @brief Parse remaining length of a packet.
* @param Pointer to the first occurence of a remaining length byte
* in MQTT packet.
* @param data_size Number of bytes available to read.
* @return Remaining length value. Returns 0 if an error occured. In the case of an error, 
* corresponding value is set for the mqtt_error variable.
*/
static size_t parse_remaining_length(uint8_t *p, size_t data_size)
{
	data_size -= LENGTH_CONTROL_HEADER; 
	
	size_t remaining_length = 0;
	size_t multiplier = 1;
	uint8_t multiplier_power = 0;
	uint8_t encoded_byte = 0;
	do {
		// Prevent reading outside of the packet:
		if (multiplier_power == data_size) 
		{
			mqtt_error = MQTT_ERR_OUTSIDE_BOUNDARY;
			return 0;
		}

		encoded_byte = *p;
		remaining_length += (encoded_byte & 127) * multiplier;
		multiplier *= 128;
		multiplier_power += 1;
		if (multiplier_power > 3) 
		{
			mqtt_error = MQTT_ERR_INVALID_REMAINING_LENGTH;
			return 0;
		}
		p ++;
	} while ((encoded_byte & 128) != 0);

	return remaining_length;
} /* parse_remaining_length() */

static uint8_t *write_remaining_length(uint8_t *p_destination, size_t length)
{
	uint8_t encoded_byte = 0;
	do {
		encoded_byte = length % 128;
		length = length / 128;
		if(length > 0)
			encoded_byte |= 128;
		*p_destination = encoded_byte;
		p_destination += 1;
	} while (length > 0);

	return p_destination;
} /* write_remaining_length() */

/*!
 * @brief Write packet_id MSB and LSB to the destination.
 * @return Pointer to the byte after the Packet ID bytes.
 */
static uint8_t *write_packet_id(uint8_t *p_destination, mqtt_packet_id_t packet_id) 
{
	p_destination[0] = (uint8_t)(packet_id >> 8);	// Copy MSB.
	p_destination[1] = (uint8_t)packet_id;			// Copy LSB.
	return p_destination + 2;
}

static mqtt_packet_id_t read_packet_id(uint8_t *p_source) 
{
	mqtt_packet_id_t packet_id;
	packet_id = p_source[0];  // Copy MSB of packet id.
	packet_id <<= 8;
	packet_id += p_source[1]; // Copy LSB of packet id.	
	if (packet_id == 0) 
	{
		mqtt_error = MQTT_ERR_ZERO_PACKET_ID;
	}
	return packet_id;
}

/*!
 * @brief Compute self length of the "remaining_length" field.
 * @return Length (in bytes) of the “length” field (1..4 bytes).
 */
static size_t compute_self_length(size_t length)
{
    if (length < 128)
        return 1;
    if (length < 16384)
        return 2;
    if (length < 2097152)
        return 3;

    return 4;
} /* compute_self_length() */

/*!
 * @brief Write UTF8 string length (two bytes) and copy the string
 * to the destination from the source.
 */
static uint8_t *write_field(uint8_t *p_destination, const char *p_source)
{
	size_t source_length = strlen(p_source);

	// Field length must not exceed 65 535 bytes [MQTT-1.5.3 :85]
	if (source_length > UINT16_MAX)
		source_length = UINT16_MAX;

	// Write the length bytes:
	*p_destination = (uint8_t)(source_length >> 8); 	// MSB
	p_destination ++;
	*p_destination = (uint8_t)source_length; 			// LSB
	p_destination ++;

	// Copy UTF8 field.
	memcpy(p_destination, p_source, source_length);

	// Return pointer to free space.
	return p_destination + source_length;
} /* write_field() */

/*!
 * @brief Create packets PUBACK, PUBREC, PUBREL, PUBCOMP.
 */
static uint8_t *create_pubx_packet(uint8_t packet_type, mqtt_packet_id_t packet_id, 
		size_t *p_packet_size)
{
	uint8_t *error = NULL;
	VALIDATE_PACKET_ID;

	assert(memory_allocate != NULL);
	uint8_t *p_packet = (uint8_t*)memory_allocate(LENGTH_PUBX_PACKET);
	if (!p_packet) 
	{
		mqtt_error = MQTT_ERR_MALLOC_FAULT;
		return error;
	}
	*p_packet_size = LENGTH_PUBX_PACKET;
	p_packet[0] = packet_type;					// Packet type.
	p_packet[1] = 2;							// Remaining length = 2 for pubX packets.
	p_packet[2] = (uint8_t)(packet_id >> 8);	// Packet ID MSB.
	p_packet[3] = (uint8_t)packet_id;			// Packet ID LSB.
	return p_packet;
} /* mqtt_create_pubx_packet() */

/*!
 * @brief Parse packets PUBACK, PUBREC, PUBREL, PUBCOMP, UNSUBACK.
 */
static mqtt_packet_id_t parse_pubx(uint8_t *p_packet, size_t data_size)
{
	const mqtt_packet_id_t error = 0;
	VALIDATE_PACKET_POINTER;
	INITIALIZE_AUX_POINTERS;
	p += LENGTH_CONTROL_HEADER;

	size_t length_remaining = parse_remaining_length(p, data_size);
	VALIDATE_REMAINING_LENGTH;

	// For all pubX packets remaining length must equal 2.
	if (length_remaining != 2) 
	{
		mqtt_error = MQTT_ERR_PACKET_TYPE_LENGTH_MISMATCH;
		return error;
	}

	p += compute_self_length(length_remaining);

	VALIDATE_BOUNDARY(LENGTH_PACKET_ID);
	return read_packet_id(p);
} /* mqtt_parse_pubx() */

/*! 
 * This functions is used for tests. Not part of public API
 */
void mqtt_reset_error()
{
	mqtt_error = MQTT_ERR_NO_ERROR;
}

/*** end of file  ***/
