#pragma once

#include "quantum.h"
#include "pointing_device.h"
#include <stdint.h>

/* --- SPI & Register Definitions --- */
#define PMW3610_SPI_WRITE_BIT 0x80
#define PMW3610_REG_ADDR_MASK 0x7F

/* Page Selection */
#define PMW3610_REG_PAGE_SELECT 0x7F
#define PMW3610_PAGE_SELECT_PAGE1 0xFF
#define PMW3610_PAGE_SELECT_PAGE0 0x00

/* Page 0 Registers */
#define PMW3610_REG_PRODUCT_ID 0x00
#define PMW3610_REG_MOTION 0x02
#define PMW3610_REG_DELTA_X_L 0x03
#define PMW3610_REG_DELTA_Y_L 0x04
#define PMW3610_REG_DELTA_XY_H 0x05
#define PMW3610_REG_SQUAL 0x06
#define PMW3610_REG_SHUTTER_HIGHER 0x07
#define PMW3610_REG_SHUTTER_LOWER 0x08
#define PMW3610_REG_PERFORMANCE 0x11
#define PMW3610_REG_MOTION_BURST 0x12
#define PMW3610_REG_RUN_DOWNSHIFT 0x1B
#define PMW3610_REG_REST1_PERIOD 0x1C
#define PMW3610_REG_REST1_DOWNSHIFT 0x1D
#define PMW3610_REG_OBSERVATION 0x2D
#define PMW3610_REG_SMART_MODE 0x32 /* DTEST2_PAD in datasheet, used as Smart Mode */
#define PMW3610_REG_POWER_UP_RESET 0x3A
#define PMW3610_REG_SHUTDOWN 0x3B
#define PMW3610_REG_NOT_PROD_ID 0x3F
#define PMW3610_REG_SPI_CLK_ON_REQ 0x41

/* Page 1 Registers */
#define PMW3610_REG_RES_STEP 0x05

/* Constants */
#define PMW3610_PRODUCT_ID 0x3E
#define PMW3610_NOT_PRODUCT_ID 0xC1
#define PMW3610_POWERUP_CMD_RESET 0x5A
#define PMW3610_SPI_CLOCK_CMD_ENABLE 0xBA
#define PMW3610_SPI_CLOCK_CMD_DISABLE 0xB5

/* Smart Mode */
#define PMW3610_SMART_MODE_ENABLE 0x00
#define PMW3610_SMART_MODE_DISABLE 0x80

/* Performance Register Bits */
#define PMW3610_PERFORMANCE_FMODE_MASK 0xF0
#define PMW3610_PERFORMANCE_FMODE_FORCE_AWAKE 0xF0
#define PMW3610_PERFORMANCE_POSR_MASK 0x0F
#define PMW3610_PERFORMANCE_POSR_125HZ 0x0D /* Default safe value */

/* Burst Read */
#define PMW3610_MAX_BURST_SIZE 10
#ifndef PMW3610_BURST_SIZE
#    define PMW3610_BURST_SIZE 7 /* Required for Smart Mode */
#endif

/* Burst Offsets */
#define PMW3610_BURST_MOTION 0
#define PMW3610_BURST_X_L 1
#define PMW3610_BURST_Y_L 2
#define PMW3610_BURST_XY_H 3
#define PMW3610_BURST_SQUAL 4
#define PMW3610_BURST_SHUTTER_HI 5
#define PMW3610_BURST_SHUTTER_LO 6

#define PMW3610_MOTION_BIT 0x80

/* CPI */
#define PMW3610_MAX_CPI 3200
#define PMW3610_MIN_CPI 200
#define PMW3610_CPI_STEP 200

#ifndef PMW3610_CPI
#    define PMW3610_CPI 800U
#endif

/* Timings (Conservative for stability) */
#define PMW3610_T_NCS_SCLK 1   /* 120ns -> 1us */
#define PMW3610_T_SCLK_NCS_WR 10
#define PMW3610_T_SRAD 5       /* Increased from 4us for stability */
#define PMW3610_T_SRAD_MOTBR 10 /* Increased for burst read setup */
#define PMW3610_T_SRX 1
#define PMW3610_T_SWX 30
#define PMW3610_T_BEXIT 1
#define PMW3610_T_CLOCK_ON 300 /* Time to stabilize internal clock */

/* Configuration Options */
#ifndef PMW3610_SMART_MODE
#    define PMW3610_SMART_MODE 1
#endif

#ifndef PMW3610_SMART_MODE_THRESHOLD
#    define PMW3610_SMART_MODE_THRESHOLD 45
#endif

/* Debounce count to prevent mode switching jitter (approx 50-100ms) */
#ifndef PMW3610_SMART_MODE_DEBOUNCE
#    define PMW3610_SMART_MODE_DEBOUNCE 50
#endif

/* Pins */
#ifndef PMW3610_CS_PIN
#    ifdef POINTING_DEVICE_CS_PIN
#        define PMW3610_CS_PIN POINTING_DEVICE_CS_PIN
#    else
#        error "PMW3610_CS_PIN not defined"
#    endif
#endif
#ifndef PMW3610_CS_PIN_RIGHT
#    define PMW3610_CS_PIN_RIGHT PMW3610_CS_PIN
#endif

#ifndef PMW3610_SCLK_PIN
#    ifdef POINTING_DEVICE_SCLK_PIN
#        define PMW3610_SCLK_PIN POINTING_DEVICE_SCLK_PIN
#    else
#        error "PMW3610_SCLK_PIN not defined"
#    endif
#endif
#ifndef PMW3610_SCLK_PIN_RIGHT
#    define PMW3610_SCLK_PIN_RIGHT PMW3610_SCLK_PIN
#endif

#ifndef PMW3610_SDIO_PIN
#    ifdef POINTING_DEVICE_SDIO_PIN
#        define PMW3610_SDIO_PIN POINTING_DEVICE_SDIO_PIN
#    else
#        error "PMW3610_SDIO_PIN not defined"
#    endif
#endif
#ifndef PMW3610_SDIO_PIN_RIGHT
#    define PMW3610_SDIO_PIN_RIGHT PMW3610_SDIO_PIN
#endif

/* API */
bool     pmw3610_init(uint8_t sensor);
uint16_t pmw3610_get_cpi(uint8_t sensor);
bool     pmw3610_set_cpi(uint8_t sensor, uint16_t cpi);
bool     pmw3610_write(uint8_t sensor, uint8_t reg_addr, uint8_t data);
uint8_t  pmw3610_read(uint8_t sensor, uint8_t reg_addr);

/* QMK Hooks */
bool           pointing_device_driver_init(void);
report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report);
void           pointing_device_driver_set_cpi(uint16_t cpi);
uint16_t       pointing_device_driver_get_cpi(void);