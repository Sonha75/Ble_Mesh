/*
 * Copyright (c) 2017 Andrej Krutak <dev@andree.sk>
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>
 * Copyright (c) 2022 Simone Tollardo <simonetollardo@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of itscontributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file ESP32_BH1750.c
 *
 * @ingroup bh1750 ESP-IDF driver for BH1750 light sensor
 *
 * ESP-IDF driver for BH1750 light sensor
 *
 * Datasheet: ROHM Semiconductor bh1750fvi-e.pdf
 *
 * Ported from esp-open-rtos
 *
 * Copyright (c) 2017 Andrej Krutak <dev@andree.sk>\n
 * Copyright (c) 2018 Ruslan V. Uss <unclerus@gmail.com>\n
 * Copyright (c) 2022 Simone Tollardo <simonetollardo@gmail.com>
 *
 * BSD Licensed as described in the file LICENSE
 */
#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include "bh1750.h"

#define OPCODE_HIGH  (0x0)
#define OPCODE_HIGH2 (0x1)
#define OPCODE_LOW   (0x3)

#define OPCODE_CONT_H_RES (0x10)
#define OPCODE_CONT_H_RES2 (0x11)
#define OPCODE_CONT_L_RES (0x13)
#define OPCODE_OT_H_RES   (0x20)
#define OPCODE_OT_H_RES2  (0x21)
#define OPCODE_OT_L_RES   (0x23)

#define OPCODE_RESET (0x07)

#define OPCODE_POWER_DOWN (0x00)
#define OPCODE_POWER_ON   (0x01)
#define OPCODE_MT_HI      (0x40)    //Measurement time High Bits -> see datasheet
#define OPCODE_MT_LO      (0x60)    //Measurement time Low Bits -> see datasheet
#define MTIME_DEFAULT     (0x45)    //Default measurement time

#define I2C_FREQ_HZ (400000)

static const char* TAG="ESP32_BH1750";

//Support functions



/**
 *  @brief  Writes a register and an 8 bit value over I2C
 *  @param  reg
 *  @param  value
 */
static void BH1750_write8(ESP32_BH1750* BH1750, uint8_t opcode) {
    uint8_t buffer[1];
    buffer[0]=opcode;
    ESP_ERROR_CHECK(i2c_master_write_to_device(BH1750->i2c_port, BH1750->addr, buffer, 1, portMAX_DELAY));
   
}

//Public functions

/**
 * @brief Initialize device descriptor
 *
 * @param[out] ESP_BH1750 BH1750 Structure
 * @param[in] addr I2C address, BH1750_ADDR_LO or BH1750_ADDR_HI
 * @param[in] port I2C port number
 * @param[in] sda_gpio GPIO pin number for SDA
 * @param[in] scl_gpio GPIO pin number for SCL
 * @return `ESP_OK` on success
 */
esp_err_t BH1750_init(ESP32_BH1750 *BH1750, uint8_t addr, uint8_t i2c_port)
{   
    if(!BH1750->_bh1750Initialised){
        if (addr != BH1750_ADDR_LO && addr != BH1750_ADDR_HI)
        {
            ESP_LOGE(TAG,"Invalid I2C address");
            return ESP_ERR_INVALID_ARG;
        }
        BH1750->i2c_port=i2c_port;
        
        BH1750->addr = addr;

        BH1750->_bh1750Initialised = true;

        BH1750_set(BH1750, BH1750->mode, BH1750->res, BH1750->MTime);

        return ESP_OK;
    }
    ESP_LOGE(TAG, "BH1750 Already initialised");
    return ESP_FAIL;
}

/**
 * @brief Power down device
 *
 * @param ESP_BH1750 BH1750 Structure
 * 
 */
void BH1750_power_down(ESP32_BH1750 *BH1750)
{   
    if(BH1750->_bh1750Initialised){
        BH1750_write8(BH1750, OPCODE_POWER_DOWN);
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}

/**
 * @brief Power on device
 *
 * @param ESP_BH1750 BH1750 Structure
 * 
 */
void BH1750_power_on(ESP32_BH1750 *BH1750)
{   
    if(BH1750->_bh1750Initialised){
        BH1750_write8(BH1750, OPCODE_POWER_ON);
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}

/**
 * @brief Setup device parameters in the device structure
 *
 * @param ESP_BH1750 BH1750 Structure
 * @param mode Measurement mode
 * @param resolution Measurement resolution
 * 
 */
void BH1750_set(ESP32_BH1750 *BH1750, bh1750_mode_t mode, bh1750_resolution_t res, uint8_t time){
    if(BH1750->_bh1750Initialised){
        BH1750->mode = mode;
        BH1750->res = res;
        BH1750->MTime = time;
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}

/**
 * @brief Reset bh1750. This function cannot be called when device is powered off.
 *
 * @param ESP_BH1750 BH1750 Structure
 * 
 */
void BH1750_reset(ESP32_BH1750 *BH1750)
{
    if(BH1750->_bh1750Initialised){
        BH1750_write8(BH1750, OPCODE_RESET);
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}

/**
 * @brief Start the measure with the settings set by the arguments. Note that parameters are not set in the structure and so they are used only for this measure. Parameter can be setted permanently with bh1750_set().
 *
 * @param ESP_BH1750 BH1750 Structure
 * @param mode Measurement mode
 * @param resolution Measurement resolution
 * 
 */
void BH1750_measure(ESP32_BH1750 *BH1750)
{   
    if(BH1750->_bh1750Initialised){

        //set measurement time
        uint8_t MTHigh = OPCODE_MT_HI | (BH1750->MTime >> 5);
        uint8_t MTLow = OPCODE_MT_LO | (BH1750->MTime & 0x1F);
        BH1750_write8(BH1750, MTHigh);
        BH1750_write8(BH1750, MTLow);

        //set measurement resolution and mode and start measurement
        if(BH1750->mode==BH1750_MODE_CONTINUOUS){
            if(BH1750->res==BH1750_RES_HIGH){
                BH1750_write8(BH1750, OPCODE_CONT_H_RES);
            }
            else if(BH1750->res==BH1750_RES_HIGH2){
                BH1750_write8(BH1750, OPCODE_CONT_H_RES2);
            }
            else if(BH1750->res==BH1750_RES_LOW){
                BH1750_write8(BH1750, OPCODE_CONT_L_RES);
            }
        }
        else{
            if(BH1750->res==BH1750_RES_HIGH){
                BH1750_write8(BH1750, OPCODE_OT_H_RES);
            }
            else if(BH1750->res==BH1750_RES_HIGH2){
                BH1750_write8(BH1750, OPCODE_OT_H_RES2);
            }
            else if(BH1750->res==BH1750_RES_LOW){
                BH1750_write8(BH1750, OPCODE_OT_L_RES);
            }
        }
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
    
}

/**
 * @brief Read measured value. Note that this function should be called after bh1750_measure().
 * You must wait the measurement time before calling this function or instead use bh1750_measure_and_read().
 * @param ESP_BH1750 BH1750 Structure
 * @param level Uint16_t pointer to store measured value
 * 
 */
void BH1750_read_measure(ESP32_BH1750 *BH1750, uint16_t *level){
    if(BH1750->_bh1750Initialised){
        uint8_t buffer[2];
        ESP_ERROR_CHECK(i2c_master_read_from_device(BH1750->i2c_port, BH1750->addr, buffer, 2, 1000 / portTICK_PERIOD_MS));

        *level = buffer[0] << 8 | buffer[1];
        *level = (*level * 10) / 12; // convert to LUX
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}

/**
 *  @brief  Complete a measure, wait the necessary time, and read the value.
 *  If the BH1750 is set in One-Time measurement mode, you need to do a 
 *  bh1750_power_on() before calling again this function.
 *  @param level Uint16_t pointer to store measured value
 */
void BH1750_measure_and_read(ESP32_BH1750* BH1750, uint16_t *level) {
    if(BH1750->_bh1750Initialised){
        uint8_t buffer[2];
        double wait_time_ms=0;

        if(BH1750->mode == BH1750_MODE_ONE_TIME) {
            BH1750_power_on(BH1750);
            BH1750_measure(BH1750);
            
        }
        else if(BH1750->mode == BH1750_MODE_CONTINUOUS) {
            BH1750_measure(BH1750);
        }

        switch (BH1750->res)      //measure times are a bit higher in order to archieve proper measure
        {
        case BH1750_RES_LOW:  
            wait_time_ms = 30.0*(BH1750->MTime/((double)MTIME_DEFAULT));
            break;
        case BH1750_RES_HIGH:
            wait_time_ms = 190.0*(BH1750->MTime/((double)MTIME_DEFAULT));
            break;
        case BH1750_RES_HIGH2:
            wait_time_ms = 190.0*(BH1750->MTime/((double)MTIME_DEFAULT));
            break;
        default:
            wait_time_ms=0;
            ESP_LOGE(TAG, "bh1750_measure_and_read: Invalid resolution");
            break;
        }

        //wait measure time
        vTaskDelay(wait_time_ms/portTICK_PERIOD_MS);

        //read
        ESP_ERROR_CHECK(i2c_master_read_from_device(BH1750->i2c_port, BH1750->addr, buffer, 2, 1000 / portTICK_PERIOD_MS));

        *level = buffer[0] << 8 | buffer[1];
        *level = (*level * 10) / 12; // convert to LUX
    }
    else{
        ESP_LOGE(TAG, "BH1750 Not initialised");
    }
}
