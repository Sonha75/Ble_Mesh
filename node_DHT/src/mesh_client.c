
#include "mesh_client.h"

#include <sdkconfig.h>                                                                                                                                                                                                       

#include "esp_log.h"

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_ble_mesh_defs.h"
#include "mqtt.h"
#include "mqtt_client.h"
#define TAG "MESH_CLIENT"

/*******************************************
 ****** Private Variables Definitions ******
 *******************************************/
extern esp_mqtt_client_handle_t client;
static uint8_t dev_uuid[16] = { 0xdd, 0xdd };               /**< Device UUID */

static bool is_provisioning = false; /**<Provision flags> */

// Configuration Server Model
static esp_ble_mesh_cfg_srv_t config_server = {
    .relay = ESP_BLE_MESH_RELAY_DISABLED,
    .beacon = ESP_BLE_MESH_BEACON_ENABLED,
#if defined(CONFIG_BLE_MESH_FRIEND)
    .friend_state = ESP_BLE_MESH_FRIEND_ENABLED,
#else
    .friend_state = ESP_BLE_MESH_FRIEND_NOT_SUPPORTED,
#endif
#if defined(CONFIG_BLE_MESH_GATT_PROXY_SERVER)
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_ENABLED,
#else
    .gatt_proxy = ESP_BLE_MESH_GATT_PROXY_NOT_SUPPORTED,
#endif
    .default_ttl = 7,
    /* 3 transmissions with 20ms interval */
    .net_transmit = ESP_BLE_MESH_TRANSMIT(2, 20),
    .relay_retransmit = ESP_BLE_MESH_TRANSMIT(2, 20),
};

//* opcode set/get message
static const esp_ble_mesh_client_op_pair_t custom_model_op_pair[] = {
    {ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_GET , ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_STATUS },
     {ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_SET , ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_STATUS },
};

//* opcode received from server
static esp_ble_mesh_model_op_t custom_sensor_op[] = {
    ESP_BLE_MESH_MODEL_OP(ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_STATUS, 2),
    ESP_BLE_MESH_MODEL_OP_END,
};

//* custom message client
static esp_ble_mesh_client_t custom_sensor_client = {
    .op_pair_size = ARRAY_SIZE(custom_model_op_pair),
    .op_pair = custom_model_op_pair,
};


// define extend model
static esp_ble_mesh_model_t custom_models[] = {
    ESP_BLE_MESH_VENDOR_MODEL(CID_ESP, ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_ID_CLIENT,
    custom_sensor_op, NULL, &custom_sensor_client),
};


//define root model
static esp_ble_mesh_model_t root_models[] = {
    ESP_BLE_MESH_MODEL_CFG_SRV(&config_server),
};

//define element
static esp_ble_mesh_elem_t elements[] = {
    ESP_BLE_MESH_ELEMENT(0, root_models, custom_models),
};

//define composition
static esp_ble_mesh_comp_t composition = {
    .cid = CID_ESP,
    .elements = elements,
    .element_count = ARRAY_SIZE(elements),
};



static esp_ble_mesh_prov_t provision = {
    .uuid = dev_uuid,
    .output_size = 0,
    .output_actions = 0,
};


static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index);


static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                        esp_ble_mesh_prov_cb_param_t *param);

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event, 
                                        esp_ble_mesh_cfg_server_cb_param_t *param);

static void ble_mesh_custom_sensor_client_model_cb(esp_ble_mesh_model_cb_event_t event,
                                                   esp_ble_mesh_model_cb_param_t *param);


/**
 * @brief Parses received Sensor raw data and stores it in struct
 * 
 * @param  recv_param   Pointer to model callback received parameter
 * @param  parsed_data  Pointer to where the parsed data will be stored
 */
static void parse_received_data(esp_ble_mesh_model_cb_param_t *recv_param, model_sensor_data_t *parsed_data);

bool is_client_provisioned(void)
{
    return is_provisioning;
}


static void prov_complete(uint16_t net_idx, uint16_t addr, uint8_t flags, uint32_t iv_index) {
    ESP_LOGI(TAG, "net_idx: 0x%04x, addr: 0x%04x", net_idx, addr);
    ESP_LOGI(TAG, "flags: 0x%02x, iv_index: 0x%08x", flags, iv_index);
    is_provisioning = true; //  Provisioned
    ESP_LOGI(TAG, "Device provisioned"); 
}

static void ble_mesh_provisioning_cb(esp_ble_mesh_prov_cb_event_t event,
                                        esp_ble_mesh_prov_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_PROV_REGISTER_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_PROV_REGISTER_COMP_EVT, err_code %d", param->prov_register_comp.err_code);
        break;
        
        case ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_ENABLE_COMP_EVT, err_code %d", param->node_prov_enable_comp.err_code);
        break;
        
        case ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_OPEN_EVT, bearer %s",
                param->node_prov_link_open.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
        
        case ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_LINK_CLOSE_EVT, bearer %s",
                param->node_prov_link_close.bearer == ESP_BLE_MESH_PROV_ADV ? "PB-ADV" : "PB-GATT");
        break;
        
        case ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_PROV_COMPLETE_EVT");
            prov_complete(param->node_prov_complete.net_idx, param->node_prov_complete.addr,
                param->node_prov_complete.flags, param->node_prov_complete.iv_index);
        break;
        
        case ESP_BLE_MESH_NODE_PROV_RESET_EVT:
        break;
        
        case ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT:
            ESP_LOGI(TAG, "ESP_BLE_MESH_NODE_SET_UNPROV_DEV_NAME_COMP_EVT, Name: %s, err_code %d", BLE_MESH_DEVICE_NAME, param->node_set_unprov_dev_name_comp.err_code);
        break;
        
        default:
        break;
    }
}

static void ble_mesh_config_server_cb(esp_ble_mesh_cfg_server_cb_event_t event,
                                              esp_ble_mesh_cfg_server_cb_param_t *param) {
    if (event == ESP_BLE_MESH_CFG_SERVER_STATE_CHANGE_EVT) {
        switch (param->ctx.recv_op) {
            case ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_APP_KEY_ADD");
                ESP_LOGI(TAG, "net_idx 0x%04x, app_idx 0x%04x",
                    param->value.state_change.appkey_add.net_idx,
                    param->value.state_change.appkey_add.app_idx);
                ESP_LOG_BUFFER_HEX("AppKey", param->value.state_change.appkey_add.app_key, 16);
            break;
        
            case ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND:
                ESP_LOGI(TAG, "ESP_BLE_MESH_MODEL_OP_MODEL_APP_BIND");
                ESP_LOGI(TAG, "elem_addr 0x%04x, app_idx 0x%04x, cid 0x%04x, mod_id 0x%04x",
                    param->value.state_change.mod_app_bind.element_addr,
                    param->value.state_change.mod_app_bind.app_idx,
                    param->value.state_change.mod_app_bind.company_id,
                    param->value.state_change.mod_app_bind.model_id);
            break;

            default:
            break;
        }
    }
}


static void ble_mesh_custom_sensor_client_model_cb(esp_ble_mesh_model_cb_event_t event,
                                                   esp_ble_mesh_model_cb_param_t *param) {
    switch (event) {
        case ESP_BLE_MESH_MODEL_OPERATION_EVT:
            switch (param->model_operation.opcode) {
                case ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_STATUS:
                    ESP_LOGI(TAG, "OPCODE_STATUS received: 0x%06x", param->model_operation.opcode);
                    ESP_LOG_BUFFER_HEX("Message hex", param->model_operation.msg, param->model_operation.length);
                    // ESP_LOGI(TAG, "\t Mensagem recebida: 0x%06x", param->model_operation.msg);
                break;

                default:
                    ESP_LOGW(TAG, "Received unrecognized OPCODE message");
                break;
            }
        break;

        case ESP_BLE_MESH_MODEL_SEND_COMP_EVT:
            if (param->model_send_comp.err_code) {
                ESP_LOGE(TAG, "Failed to send message 0x%06x", param->model_send_comp.opcode);
                break;
            }
            ESP_LOGI(TAG, "Send message opcode 0x%06x successfully, ttl send:  %d  !", param->model_send_comp.opcode,param->model_send_comp.ctx->send_ttl);

        break;

        case ESP_BLE_MESH_CLIENT_MODEL_RECV_PUBLISH_MSG_EVT:
            switch (param->client_recv_publish_msg.opcode) {
                case ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_STATUS:
                    ESP_LOGI(TAG, "OPCODE_STATUS received: 0x%06x", param->client_recv_publish_msg.opcode);
                    ESP_LOGI(TAG, "src: 0x%04x, dst: 0x%04x,ttl recv: %d", param->client_recv_publish_msg.ctx->addr, param->client_recv_publish_msg.ctx->recv_dst,param->client_recv_publish_msg.ctx->recv_ttl);
                    ESP_LOG_BUFFER_HEX(TAG, param->client_recv_publish_msg.msg, param->client_recv_publish_msg.length);

                    
                    model_sensor_data_t received_data;
                    parse_received_data(param, &received_data);
                    
                break;

                default:
                    ESP_LOGW(TAG, "Received unrecognized OPCODE message: 0x%04x", param->client_recv_publish_msg.opcode);
                break;
            }


        break;

        case ESP_BLE_MESH_CLIENT_MODEL_SEND_TIMEOUT_EVT:
            ESP_LOGW(TAG, "Message opcode 0x%06x timeout", param->client_send_timeout.opcode);
            
        break;

        default:
            ESP_LOGW(TAG, "%s - Unrecognized event: 0x%04x", __func__, event);
        break;
    }
}

static void parse_received_data(esp_ble_mesh_model_cb_param_t *recv_param, model_sensor_data_t *parsed_data) {
    if (recv_param->client_recv_publish_msg.length < sizeof(parsed_data)) {
        ESP_LOGE(TAG, "Invalid received message lenght: %d", recv_param->client_recv_publish_msg.length);
        return;
    }
    
    parsed_data = (model_sensor_data_t *)recv_param->client_recv_publish_msg.msg;
    char data[50];
    ESP_LOGW("PARSED_DATA", "DEVICE NAME = %s", parsed_data->device_name);
     sprintf(data,"{\"Node num \":%s}",parsed_data->device_name);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);
     
    ESP_LOGW("PARSED_DATA", "Temperature %d= %f", parsed_data->index,parsed_data->temperature);
    sprintf(data,"{\"Temperature %d\":%f}",parsed_data->index,parsed_data->temperature);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);
   
    ESP_LOGW("PARSED_DATA", "Humidity  %d  = %f", parsed_data->index,parsed_data->humidity);
     sprintf(data,"{\"Humidity %d\":%f}",parsed_data->index,parsed_data->humidity);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);
   
     
    ESP_LOGW("PARSED_DATA", "temp_ADC   %d     = %f",parsed_data->index, parsed_data->temp_ADC);
     sprintf(data,"{\"temp_ADC %d\":%f}",parsed_data->index,parsed_data->temp_ADC);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);

     ESP_LOGW("PARSED_DATA", "Light   %d     = %d",parsed_data->index, parsed_data->light);
     sprintf(data,"{\"Light %d\":%d}",parsed_data->index,parsed_data->light);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);

     ESP_LOGW("PARSED_DATA", "Smoke   %d     = %f",parsed_data->index, parsed_data->smoke);
     sprintf(data,"{\"smoke %d\":%f}",parsed_data->index,parsed_data->smoke);
     esp_mqtt_client_publish(client, TOPIC , data, 0, 1, 0);





    
}


static void ble_mesh_get_dev_uuid(uint8_t *dev_uuid) {
    if (dev_uuid == NULL) {
        ESP_LOGE(TAG, "%s, Invalid device uuid", __func__);
        return;
    }

    /* Copy device address to the device uuid with offset equals to 2 here.
     * The first two bytes is used for matching device uuid by Provisioner.
     * And using device address here is to avoid using the same device uuid
     * by different unprovisioned devices.
     */
    memcpy(dev_uuid + 2, esp_bt_dev_get_address(), BD_ADDR_LEN);
}

static esp_err_t bluetooth_init(void) {
    esp_err_t ret;

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s initialize controller failed", __func__);
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s enable controller failed", __func__);
        return ret;
    }
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s init bluetooth failed", __func__);
        return ret;
    }
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s enable bluetooth failed", __func__);
        return ret;
    }

    return ret;
}



esp_err_t ble_mesh_device_init_client(void) {
    esp_err_t err = ESP_OK;


    err = bluetooth_init();
    if (err) {
        ESP_LOGE(TAG, "esp32_bluetooth_init failed (err %d)", err);
        return err;
    }

    ble_mesh_get_dev_uuid(dev_uuid);

    esp_ble_mesh_register_prov_callback(ble_mesh_provisioning_cb);
    esp_ble_mesh_register_config_server_callback(ble_mesh_config_server_cb);
    esp_ble_mesh_register_custom_model_callback(ble_mesh_custom_sensor_client_model_cb);

    err = esp_ble_mesh_init(&provision, &composition);
    if (err) {
        ESP_LOGE(TAG, "Initializing mesh failed (err %d)", err);
        return err;
    }


    err = esp_ble_mesh_client_model_init(&custom_models[0]);
    if (err) {
        ESP_LOGE(TAG, "Failed to initialize vendor client");
        return err;
    }

    esp_ble_mesh_set_unprovisioned_device_name(BLE_MESH_DEVICE_NAME);

    esp_ble_mesh_node_prov_enable(ESP_BLE_MESH_PROV_ADV | ESP_BLE_MESH_PROV_GATT);

    ESP_LOGI(TAG, "BLE Mesh Node initialized");

    return err;
}


esp_err_t ble_mesh_custom_sensor_client_model_message_set(model_sensor_data_t set_data) {
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode;
    esp_err_t err;

    opcode = ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_SET;
    
    ctx.net_idx = 0;
    ctx.app_idx = 0;
    //ctx.addr = ESP_BLE_MESH_ADDR_ALL_NODES;
    ctx.addr = ESP_BLE_MESH_GROUP_PUB_ADDR;
    ctx.send_ttl = 3;
    ctx.send_rel = false;

    err = esp_ble_mesh_client_model_send_msg(custom_sensor_client.model, &ctx, opcode,
            sizeof(set_data), (uint8_t *)&set_data, 0, false, ROLE_NODE);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send the custom msg 0x%06x, err = 0x%06x", opcode, err);
    }

    return err;
}

esp_err_t ble_mesh_custom_sensor_client_model_message_get(void) {
    esp_ble_mesh_msg_ctx_t ctx = {0};
    uint32_t opcode;
    esp_err_t err;

    opcode = ESP_BLE_MESH_CUSTOM_SENSOR_MODEL_OP_GET;

    ctx.net_idx = 0;
    ctx.app_idx = 0;
    // ctx.addr = ESP_BLE_MESH_ADDR_ALL_NODES;
    ctx.addr = ESP_BLE_MESH_GROUP_GW_SUB_ADDR;  
    ctx.send_ttl = 3;
    ctx.send_rel = false;

    ESP_LOGI(TAG, "*** %s ***", __func__);

    err = esp_ble_mesh_client_model_send_msg(custom_sensor_client.model, &ctx, opcode,
            0, NULL, 0, true, ROLE_NODE);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send the custom msg 0x%06x, err = 0x%06x", opcode, err);
    }

    return err;
}


