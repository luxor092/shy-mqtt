/** @file shy_mqtt.h
 * 
 * @brief Public API for the MQTT packet generator and parser.
 *
 * @details
 * This header provides interface for serializing (generating) 
 * and deserializing (parsing) MQTT 3.1.1 protocol packets. 
 * It is specifically tailored MQTT client applications running
 * on resource-constrained embedded systems.
 *
 * @note The implementation focuses on client-side packet types.
 * 		 Broker-specific packet generation is currently out of
 * 		 scope.
 *
 * @copyright Copyright (c) 2026 Pavel Salnikov. 
 * @license This project is licensed under the MIT License.
 * 			See the LICENSE file in the root directory for details.
 */

#ifndef SHY_MQTT_H
#define SHY_MQTT_H

#ifdef __cplusplus
extern "C" {
#endif


#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

/// MQTT error type.
typedef enum
{
	MQTT_ERR_NO_ERROR,
	MQTT_ERR_INVALID_POINTER,				// Invalid pointer to the packet for parsing.
	MQTT_ERR_INVALID_REMAINING_LENGTH,		// Packet has more than 4 bytes of remaining length.
	MQTT_ERR_OUTSIDE_BOUNDARY,				// Reading packet outside available boundary.
	MQTT_ERR_PACKET_TYPE_LENGTH_MISMATCH,	// Wrong remaining length for the current type of MQTT packet.
	MQTT_ERR_MALLOC_FAULT,					// Couldn't allocate memory for topic string.
	MQTT_ERR_INVALID_TOPIC_LENGTH,			// Zero topic length is not allowed.
	MQTT_ERR_ZERO_PACKET_ID,				// Packet identifier must be non-zero. [MQTT-2.3.1-1].
	MQTT_ERR_INVALID_PAYLOAD,				// Payload can't be NULL.
	MQTT_ERR_INVALID_CLIENT_ID,				// Client Id must be present [MQTT-3-1.3-3].
	MQTT_ERR_INVALID_USERNAME,				// Username must be present according to connect flags.
	MQTT_ERR_INVALID_PASSWORD,				// Password must be presnt according to connect flags.
} mqtt_error_t;

/// MQTT packet type.
typedef enum
{
	MQTT_PACKET_TYPE_NOT_RECOGNIZED,
    MQTT_PACKET_TYPE_CONNACK = 1,
    MQTT_PACKET_TYPE_PUBLISH,
    MQTT_PACKET_TYPE_PUBACK,
	MQTT_PACKET_TYPE_PUBREC,
	MQTT_PACKET_TYPE_PUBREL,
	MQTT_PACKET_TYPE_PUBCOMP,
    MQTT_PACKET_TYPE_SUBACK,
	MQTT_PACKET_TYPE_UNSUBACK,
    MQTT_PACKET_TYPE_PINGRESP,
} mqtt_packet_type_t;

/// MQTT connack packet return codes.
typedef enum
{
	MQTT_CONNACK_FUNCTION_ERROR = -1,
	MQTT_CONNACK_CONNECTION_ACCEPTED = 0,
	MQTT_CONNACK_UNACCEPTABLE_PROTOCOL,
	MQTT_CONNACK_IDENTIFIER_REJECTED,
	MQTT_CONNACK_SERVER_UNAVAILABLE,
	MQTT_CONNACK_BAD_USERNAME_PASS,
	MQTT_CONNACK_NOT_AUTHORIZED
} mqtt_return_code_connack_t;

/// MQTT suback packet return codes.
typedef enum
{
	MQTT_SUBACK_SUCCESS_QOS_0,
	MQTT_SUBACK_SUCCESS_QOS_1,
	MQTT_SUBACK_SUCCESS_QOS_2,
	MQTT_SUBACK_FAILURE = 128
} mqtt_return_code_suback_t;

/// Connect flags.
typedef union
{
    char value;
    struct
    {
        unsigned b0_reserved_zero:1;
        unsigned b1_clean_session:1;
        unsigned b2_will_flag:1;
        unsigned b3_will_QoS:1;
        unsigned b4_will_QoS:1;
        unsigned b5_will_retain:1;
        unsigned b6_password:1;
        unsigned b7_username:1;
    } bits;
} mqtt_conn_flags_t;

/// Connect parameters.
typedef struct
{
	const char 	*p_client_id;
	const char 	*p_username;
	const char 	*p_userpass;
} mqtt_conn_credentials_t;

/// Control flags.
typedef union
{
    uint8_t value;
    struct
    {
        unsigned b0_retain:1;
        unsigned b1_QoS:1;
        unsigned b2_QoS:1;
        unsigned b3_DUP:1;
        unsigned b4_reserved_zero:1;
        unsigned b5_reserved_zero:1;
        unsigned b6_reserved_zero:1;
        unsigned b7_reserved_zero:1;
    } bits;
} mqtt_ctrl_flags_t;

/// Packet id.
typedef uint16_t mqtt_packet_id_t;

// Return code.
typedef uint8_t mqtt_return_code_t;

/*!
 * @brief Assign function for memory allocation.
 * For embedded systems prefer assigning no-heap memory allocation/free functions.
 */
void mqtt_set_custom_allocator(void * (*p_allocator)(size_t));

/*!
 * @brief Assign function for memory release.
 * For embedded systems prefer assigning no-heap memory allocation/free functions.
 */
void mqtt_set_custom_free(void (*p_free)(void*));

/// Return error.
mqtt_error_t mqtt_get_error();

// Reset error.
void mqtt_reset_error();

/*!
 * @brief Make a connect flags byte.
 * @param username Whether to use “username” for connection.
 * @param password Whether to use “password” for connection.
 * @param will_retain Whether MQTT broker has to retain the last will message when no subscriber received it instantly.
 * @param will_QoS Quality of service for the last will message.
 * @param will_send Whether to send "last will" message.
 * @param clean_session Whether to clean session upon reconnect.
 * - TRUE: make a new session everytime.
 * - FALSE: connect to the previous session if exists.
 * @return Connect flags byte.
 */
mqtt_conn_flags_t mqtt_create_connect_flags(
		_Bool username,
		_Bool password,
		_Bool will_retain,
		uint8_t will_QoS,
		_Bool will_send,
		_Bool clean_session
		);

mqtt_conn_credentials_t mqtt_create_connect_credentials(
		const char *p_client_id,
		const char *p_username,
		const char *p_userpass
		);

/*!
 * @brief Create control flags byte.
 * @param The DUP flag indicates that this is not the first time the Client or Server has attempted 
 * to send this MQTT publish packet.
 * Must be set to TRUE on attempt to re-deliver a PUBLISH packet [MQTT-3.3.1.-1].
 * Must be set 0 for all QoS 0 messages [MQTT-3.3.1-2].\n
 * @param QoS Level of assurance for delivery of an Application Message.
 * - 0: at most once delivery.
 * - 1: at least once delivery.
 * - 2: exactly once delivery.
 * @param retain Set TRUE if the Server MUST store the Application Message and its QoS, so that it 
 * can be delivered to future subscribers whose subscriptions match its topic name [MQTT-3.3.1-5].
 */
mqtt_ctrl_flags_t mqtt_create_control_flags(_Bool Dup, uint8_t QoS, _Bool retain);

/*!
 * @brief Create MQTT connect packet.
 * @param[out] p_packet_size MQTT packet size (derived inside the function).
 * @return Pointer to MQTT connect packet or NULL in the case of error.
 * */
uint8_t *mqtt_create_connect_packet(
		mqtt_conn_flags_t connect_flags,
		mqtt_conn_credentials_t connect_credentials,
		uint16_t keepalive,
		const char *p_last_will_topic,
		const char *p_last_will_message,
		size_t *p_packet_size
		);

/*!
 * @brief Create MQTT publish packet.
 * @param control_flags Control flags.
 * @param p_topic_name Topic name pointer.
 * @param p_payload Pointer to the payload.
 * @param payload_size Payload size.
 * @param packet_id Packet ID.
 * @param[out] p_packet_size MQTT packet size (derived inside the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_publish_packet(
		mqtt_ctrl_flags_t control_flags, 
		const char * p_topic_name, 
		const uint8_t * p_payload, 
		size_t payload_size, 
		uint16_t packet_id, 
		size_t *p_packet_size
		);

/*!
 * @brief Create MQTT subscribe packet.
 * @param p_topic[] Array of topics to subscribe.
 * @param n_topics Number of topics in the array.
 * @param requested_QoS[] Array of max QoS values the server allowed to perform 
 * for the corresponding subscription. 
 * NOTE: number of QoS must match number of topics.
 * @param[out] p_packet_size Size of MQTT packet (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_subscribe_packet(
		mqtt_packet_id_t packet_id,
		const char *p_topic[],
		size_t n_topics,
		const uint8_t requested_QoS[],
    	size_t *p_packet_size
		);

/*!
 * @brief Create MQTT unsubscribe packet.
 * @param packet_id Packet id.
 * @param p_topic[] Array of topics to unsubscribe.
 * @param n_topics Number of topics to unsubscribe. Use the ARRAY_SIZE() macros.
 * @param[out] p_packet_size Size of MQTT packet (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_unsubscribe_packet(
		mqtt_packet_id_t packet_id,
		const char *p_topic[], 
		size_t n_topics, 
		size_t *p_packet_size
		);

/*!
 * @brief Create MQTT PUBACK packet.
 * @param packet_id Packet ID.
 * @param[out] p_packet_size MQTT packet size (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_puback_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size);

/*!
 * @brief Create MQTT PUBREC packet.
 * @param packet_id Packet ID.
 * @param[out] p_packet_size MQTT packet size (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_pubrec_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size);

/*!
 * @brief Create MQTT PUBREL packet.
 * @param packet_id Packet ID.
 * @param[out] p_packet_size MQTT packet size (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_pubrel_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size);

/*!
 * @brief Create MQTT PUBCOMP packet.
 * @param packet_id Packet ID.
 * @param[out] p_packet_size MQTT packet size (derived in the function).
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_pubcomp_packet(mqtt_packet_id_t packet_id, size_t *p_packet_size);

/*!
 * @brief Create PINGREQ packet.
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t *mqtt_create_pingreq_packet(size_t *p_packet_size);

/*!
 * @brief Create disconnect packet.
 * @return Pointer to the packet or NULL in the case of error.
 */
uint8_t * mqtt_create_disconnect_packet(size_t *p_packet_size);

/*!
 * @brief Recognize packet type.
 * @param p_packet The packet to be recognized.
 * @return Packet type.
 */
mqtt_packet_type_t mqtt_recognize_packet(uint8_t *p_packet);

/*!
 * @brief Parse MQTT connack packet.
 * @param p_packet Pointer to the packet to be parsed.
 * @param[out] p_SP Pointer to the "Session Presents" flag. 
 * Pass NULL if not required.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Return code of the connack packet or MQTT_CONNACK_FUNCTION_ERROR in the case of error.
 */
mqtt_return_code_connack_t mqtt_parse_connack(uint8_t *p_packet, _Bool *p_SP, size_t data_size);

/*!
 * @brief Parse MQTT puback packet.
 * @param p_packet Pointer to the packet.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_puback(uint8_t *p_packet, size_t data_size);

/*!
 * @brief Parse MQTT pubrec packet.
 * @param p_packet Pointer to the packet.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_pubrec(uint8_t *p_packet, size_t data_size);

/*!
 * @brief Parse MQTT pubrel packet.
 * @param p_packet Pointer to the packet.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_pubrel(uint8_t *p_packet, size_t data_size);

/*!
 * @brief Parse MQTT pubcomp packet.
 * @param p_packet Pointer to the packet.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_pubcomp(uint8_t *p_packet, size_t data_size);

/*!
 * @brief Parse MQTT suback packet.
 * @param p_packet Pointer to the packet.
 * @param[out] suback_return_code[] All suback return codes will be put in this array.
 * @param[in] array_capacity Capacity of the provided array. Use the ARRAY_SIZE()
 * 			  macros to calculate the capacity.
 * @param[in] data_size Number of bytes available for reading.
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_suback(
		uint8_t *p_packet, 
		mqtt_return_code_suback_t suback_return_codes[], 
		size_t array_capacity,
		size_t data_size
		);

/*!
 * @brief Parse MQTT unsuback packet.
 * @param p_packet Pointer to the packet.
 * @param[in] data_size Number of bytes available for reading. 
 * @return Packet ID or 0 in the case of error.
 */
mqtt_packet_id_t mqtt_parse_unsuback (uint8_t *p_packet, size_t data_size);

/*!
 * @brief Parse PUBLISH packet.
 *
 * The following arguments can be NULL if not required to parse:
 * p_QoS, pp_topic, p_packet_id;
 * 
 * NOTE: when the pp_topic argument is not NULL, the function allocates memory to store the topic.
 * User must free memory by themselves. 
 *
 *  @param[in] p_packet Pointer to the packet to parse.
 *  @param[out] p_QoS Pointer to the parsed QoS value.
 *  @param[out] p_packet_id Pointer to the parsed packet ID.
 *  @param[out] pp_topic Pointer to parsed topic string pointer.
 *  @param[out] p_payload_length Pointer to the payload length.
 *  @param[in] data_size Number of bytes available for reading.
 *  @return Pointer to the payload data in the packet or NULL in the case of error.
 */
uint8_t * mqtt_parse_publish(
		uint8_t *p_packet,
		uint8_t *p_QoS,
		mqtt_packet_id_t *p_packet_id,
		char **pp_topic,
        size_t *p_payload_length,
		size_t data_size
		);


#ifdef __cplusplus
}
#endif

#endif /* SHY_MQTT_H */

/*** end of file ***/
