
#ifndef __MESH_SERVER_H__
#define __MESH_SERVER_H__

#include <stdio.h>
#include <string.h>

#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_generic_model_api.h"
#include "custom_sensor_model_defs.h"
#include "freertos/queue.h"



/**
 * @brief Initializes BLE Mesh stack, initializing Models and it's callback functions
 * 
 */
esp_err_t ble_mesh_device_init_server(void);

void server_send_to_unicast(model_sensor_data_t server_model_state);
void server_send_to_multicast(model_sensor_data_t server_model_state);
bool is_server_provisioned(void);
void set_information();
#endif  