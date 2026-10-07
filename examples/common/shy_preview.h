/*! @file shy_preview.h
 *
 * @brief Functions to preview MQTT packets.
 */

#ifndef SHY_PREVIEW_H
#define SHY_PREVIEW_H


#include <stdint.h>
#include <stddef.h>
#include "../../src/shy_mqtt.h"

#define N_TOPICS_TO_SUBSCRIBE 3

/*!
 * @brief Print MQTT packet.
 * @param p_package Pointer to the packet.
 * @param package_size Size of the packet.
 */
void print_packet(const char *introduction, uint8_t *p_package, const size_t package_size);

/*!
 * @brief Partly print MQTT packet.
 * @param introduction Desctiption of the packet to be printed.
 * @param p_packet Pointer to the packet.
 * @param packet_size Size of the packet.
 * @param limit Number of packet bytes to print.
 */
void print_packet_partly(const char *introduction, uint8_t *p_packet, const size_t packet_size, 
		const size_t limit);

void print_empty_string();

/*!
 * @brief Print the error of shy-mqtt. 
 * NOTE: This function is not part of the shy-mqtt library. 
 */
void print_error(mqtt_error_t error);


#endif /* SHY_PREVIEW_H */

/*** end of file ***/
