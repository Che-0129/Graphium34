#include "quantum.h"
#include "pointing_device_internal.h"
#include "pmw3610.h"
#include "wait.h"
#include <stddef.h>

#ifndef CONSTRAIN
#    define CONSTRAIN(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#endif

static pin_t    pmw3610_cs_pin      = NO_PIN;
static pin_t    pmw3610_sclk_pin    = NO_PIN;
static pin_t    pmw3610_sdio_pin    = NO_PIN;
static bool     pmw3610_initialized = false;
static uint16_t pmw3610_cached_cpi  = PMW3610_CPI;

#if PMW3610_SMART_MODE
static bool     pmw3610_smart_enabled = false;
static uint16_t pmw3610_smart_counter = 0;
#endif

/* --- SPI Bitbang (Mode 3: CPOL=1, CPHA=1) --- */
/* Idle state: SCLK High */

static void pmw3610_cache_pins(void) {
    bool left = is_keyboard_left();
    pmw3610_cs_pin   = left ? PMW3610_CS_PIN : PMW3610_CS_PIN_RIGHT;
    pmw3610_sclk_pin = left ? PMW3610_SCLK_PIN : PMW3610_SCLK_PIN_RIGHT;
    pmw3610_sdio_pin = left ? PMW3610_SDIO_PIN : PMW3610_SDIO_PIN_RIGHT;
}

static inline void pmw3610_spi_start(void) {
    if (pmw3610_cs_pin != NO_PIN) {
        gpio_write_pin_low(pmw3610_cs_pin);
    }
    wait_us(PMW3610_T_NCS_SCLK);
}

static inline void pmw3610_spi_stop(void) {
    if (pmw3610_cs_pin != NO_PIN) {
        gpio_write_pin_high(pmw3610_cs_pin);
    }
    /* Ensure SCLK returns to Idle High */
    gpio_write_pin_high(pmw3610_sclk_pin);
}

static inline void pmw3610_spi_shift_out(uint8_t data) {
    pin_t sclk = pmw3610_sclk_pin;
    pin_t sdio = pmw3610_sdio_pin;

    for (int8_t i = 7; i >= 0; i--) {
        /* Mode 3: Data setup while SCLK is High (or Low), Latch on Rising Edge */
        /* We set data while Low, then pull High to latch */
        gpio_write_pin_low(sclk);
        gpio_write_pin(sdio, (data >> i) & 0x01);
        wait_us(PMW3610_T_NCS_SCLK);
        
        gpio_write_pin_high(sclk); /* Rising edge: Sensor latches data */
        wait_us(PMW3610_T_NCS_SCLK);
    }
}

static inline uint8_t pmw3610_spi_shift_in(void) {
    uint8_t data = 0;
    pin_t   sclk = pmw3610_sclk_pin;
    pin_t   sdio = pmw3610_sdio_pin;

    for (int8_t i = 7; i >= 0; i--) {
        /* Mode 3: Slave updates data on Falling Edge, Master samples on Rising Edge (or stable High) */
        gpio_write_pin_low(sclk); /* Falling edge: Sensor updates SDIO */
        wait_us(PMW3610_T_SRX);
        
        gpio_write_pin_high(sclk);
        wait_us(PMW3610_T_SRX); /* Wait for stable */
        
        if (gpio_read_pin(sdio)) {
            data |= (uint8_t)(1u << i);
        }
    }
    return data;
}

/* --- Low Level R/W --- */

static bool pmw3610_write_raw(uint8_t reg_addr, uint8_t data) {
    uint8_t cmd = (reg_addr & PMW3610_REG_ADDR_MASK) | PMW3610_SPI_WRITE_BIT;
    pmw3610_spi_start();
    gpio_set_pin_output(pmw3610_sdio_pin);
    pmw3610_spi_shift_out(cmd);
    pmw3610_spi_shift_out(data);
    wait_us(PMW3610_T_SCLK_NCS_WR);
    pmw3610_spi_stop();
    wait_us(PMW3610_T_SWX);
    return true;
}

static bool pmw3610_read_raw(uint8_t reg_addr, uint8_t *value) {
    if (!value) return false;
    uint8_t cmd = reg_addr & PMW3610_REG_ADDR_MASK;
    pmw3610_spi_start();
    gpio_set_pin_output(pmw3610_sdio_pin);
    pmw3610_spi_shift_out(cmd);
    
    /* Turnaround */
    gpio_write_pin_low(pmw3610_sclk_pin);
    gpio_set_pin_input(pmw3610_sdio_pin);
    wait_us(PMW3610_T_SRAD);
    
    *value = pmw3610_spi_shift_in();
    wait_us(PMW3610_T_NCS_SCLK);
    pmw3610_spi_stop();
    wait_us(PMW3610_T_SRX);
    return true;
}

static bool pmw3610_burst_read_raw(uint8_t *data, uint8_t length) {
    if (!data || length == 0 || length > PMW3610_MAX_BURST_SIZE) return false;
    
    pmw3610_spi_start();
    gpio_set_pin_output(pmw3610_sdio_pin);
    pmw3610_spi_shift_out(PMW3610_REG_MOTION_BURST & PMW3610_REG_ADDR_MASK);
    
    /* Turnaround */
    gpio_write_pin_low(pmw3610_sclk_pin);
    gpio_set_pin_input(pmw3610_sdio_pin);
    wait_us(PMW3610_T_SRAD_MOTBR); /* Critical wait for burst start */
    
    for (uint8_t i = 0; i < length; i++) {
        data[i] = pmw3610_spi_shift_in();
        if (i + 1 < length) wait_us(PMW3610_T_SRX);
    }
    
    wait_us(PMW3610_T_NCS_SCLK);
    pmw3610_spi_stop();
    wait_us(PMW3610_T_BEXIT);
    return true;
}

static inline bool pmw3610_spi_clk_on(void) {
    if (!pmw3610_write_raw(PMW3610_REG_SPI_CLK_ON_REQ, PMW3610_SPI_CLOCK_CMD_ENABLE)) return false;
    wait_us(PMW3610_T_CLOCK_ON);
    return true;
}

static inline bool pmw3610_spi_clk_off(void) {
    return pmw3610_write_raw(PMW3610_REG_SPI_CLK_ON_REQ, PMW3610_SPI_CLOCK_CMD_DISABLE);
}

/* --- High Level API --- */

bool pmw3610_write(uint8_t sensor, uint8_t reg_addr, uint8_t data) {
    (void)sensor;
    if (!pmw3610_spi_clk_on()) return false;
    bool ok = pmw3610_write_raw(reg_addr, data);
    pmw3610_spi_clk_off();
    return ok;
}

uint8_t pmw3610_read(uint8_t sensor, uint8_t reg_addr) {
    (void)sensor;
    uint8_t val = 0;
    pmw3610_read_raw(reg_addr, &val);
    return val;
}

bool pmw3610_set_cpi(uint8_t sensor, uint16_t cpi) {
    (void)sensor;
    cpi = CONSTRAIN(cpi, PMW3610_MIN_CPI, PMW3610_MAX_CPI);
    uint8_t res = (uint8_t)(cpi / PMW3610_CPI_STEP);

    if (!pmw3610_spi_clk_on()) return false;
    
    bool ok = pmw3610_write_raw(PMW3610_REG_PAGE_SELECT, PMW3610_PAGE_SELECT_PAGE1);
    uint8_t val = 0;
    if (ok) ok = pmw3610_read_raw(PMW3610_REG_RES_STEP, &val);
    
    if (ok) {
        val &= ~0x1F; /* Clear resolution bits */
        val |= (res & 0x1F);
        
        /* Preserve inversion bits if needed, or force clear */
#if defined(PMW3610_INVERT_X)
        val |= (1 << 6);
#else
        val &= ~(1 << 6);
#endif
#if defined(PMW3610_INVERT_Y)
        val |= (1 << 5);
#else
        val &= ~(1 << 5);
#endif
        ok = pmw3610_write_raw(PMW3610_REG_RES_STEP, val);
    }
    
    bool restored = pmw3610_write_raw(PMW3610_REG_PAGE_SELECT, PMW3610_PAGE_SELECT_PAGE0);
    pmw3610_spi_clk_off();
    
    if (ok && restored) {
        pmw3610_cached_cpi = cpi;
        return true;
    }
    return false;
}

uint16_t pmw3610_get_cpi(uint8_t sensor) {
    (void)sensor;
    return pmw3610_cached_cpi;
}

bool pmw3610_init(uint8_t sensor) {
    (void)sensor;
    pmw3610_initialized = false;
    pmw3610_cache_pins();

    if (pmw3610_sclk_pin == NO_PIN || pmw3610_sdio_pin == NO_PIN) return false;

    /* Idle State: CS High, SCLK High (Mode 3) */
    if (pmw3610_cs_pin != NO_PIN) {
        gpio_set_pin_output(pmw3610_cs_pin);
        gpio_write_pin_high(pmw3610_cs_pin);
    }
    gpio_set_pin_output(pmw3610_sclk_pin);
    gpio_write_pin_high(pmw3610_sclk_pin); /* CPOL=1 */
    gpio_set_pin_input(pmw3610_sdio_pin);

    wait_ms(10);

    /* Reset */
    if (!pmw3610_write_raw(PMW3610_REG_POWER_UP_RESET, PMW3610_POWERUP_CMD_RESET)) return false;
    wait_ms(50); /* Increased reset wait */

    /* Check ID */
    uint8_t pid = 0, npid = 0;
    pmw3610_read_raw(PMW3610_REG_PRODUCT_ID, &pid);
    pmw3610_read_raw(PMW3610_REG_NOT_PROD_ID, &npid);
    if (pid != PMW3610_PRODUCT_ID || npid != PMW3610_NOT_PRODUCT_ID) {
        pd_dprintf("PMW3610: ID mismatch %02X/%02X\n", pid, npid);
        return false;
    }

    if (!pmw3610_spi_clk_on()) return false;

    /* Observation Check */
    pmw3610_write_raw(PMW3610_REG_OBSERVATION, 0x00);
    wait_ms(10);
    uint8_t obs = 0;
    pmw3610_read_raw(PMW3610_REG_OBSERVATION, &obs);
    if ((obs & 0x0F) != 0x0F) {
         pd_dprintf("PMW3610: Obs check failed %02X\n", obs);
    }

    /* Clear Motion */
    uint8_t dummy[PMW3610_BURST_SIZE];
    pmw3610_burst_read_raw(dummy, PMW3610_BURST_SIZE);

    /* Performance: Force Awake (0xF0) + 125Hz (0x0D) = 0xFD */
    /* This is CRITICAL to prevent stutter from sleep/wake latency */
    uint8_t perf_val = PMW3610_PERFORMANCE_FMODE_FORCE_AWAKE | PMW3610_PERFORMANCE_POSR_125HZ;
    pmw3610_write_raw(PMW3610_REG_PERFORMANCE, perf_val);
    
    /* Verify Force Awake */
    uint8_t read_perf = 0;
    pmw3610_read_raw(PMW3610_REG_PERFORMANCE, &read_perf);
    if ((read_perf & PMW3610_PERFORMANCE_FMODE_MASK) != PMW3610_PERFORMANCE_FMODE_FORCE_AWAKE) {
        pd_dprintf("PMW3610: Warning - Force Awake not set correctly (%02X)\n", read_perf);
    }

    /* Downshift settings (Effectively disabled by Force Awake, but set safe values) */
    pmw3610_write_raw(PMW3610_REG_RUN_DOWNSHIFT, 0x04);
    pmw3610_write_raw(PMW3610_REG_REST1_PERIOD, 0x04);
    pmw3610_write_raw(PMW3610_REG_REST1_DOWNSHIFT, 0x0F);

#if PMW3610_SMART_MODE
    /* Init Smart Mode to Disabled (Normal) first, let logic handle it */
    pmw3610_write_raw(PMW3610_REG_SMART_MODE, PMW3610_SMART_MODE_DISABLE);
    pmw3610_smart_enabled = false;
#endif

    pmw3610_spi_clk_off();

    /* Set CPI */
    if (!pmw3610_set_cpi(0, PMW3610_CPI)) {
        pd_dprintf("PMW3610: Set CPI failed\n");
    }

    pmw3610_initialized = true;
    return true;
}

/* --- QMK Hooks --- */

bool pointing_device_driver_init(void) { return pmw3610_init(0); }
void pointing_device_driver_set_cpi(uint16_t cpi) { pmw3610_set_cpi(0, cpi); }
uint16_t pointing_device_driver_get_cpi(void) { return pmw3610_get_cpi(0); }

report_mouse_t pointing_device_driver_get_report(report_mouse_t mouse_report) {
    mouse_report.x = 0;
    mouse_report.y = 0;

    static int32_t carry_x = 0;
    static int32_t carry_y = 0;

    if (!pmw3610_initialized) return mouse_report;

    uint8_t burst[PMW3610_BURST_SIZE];
    if (!pmw3610_burst_read_raw(burst, PMW3610_BURST_SIZE)) {
        carry_x = 0; carry_y = 0;
        return mouse_report;
    }

    /* Validate Motion Byte: 0xFF usually means SPI error / float */
    uint8_t motion = burst[PMW3610_BURST_MOTION];
    if (motion == 0xFF || motion == 0x00) {
        /* 0x00 is valid (no motion), 0xFF is error. 
           If 0x00, we skip delta processing but still might need to check smart mode? 
           No, smart mode needs valid shutter data which comes with motion usually.
        */
        if (motion == 0xFF) return mouse_report; /* Error */
    }

    bool has_motion = (motion & PMW3610_MOTION_BIT) != 0;

#if PMW3610_SMART_MODE
    /* Smart Mode Logic with Debounce to prevent stutter */
    if (has_motion) {
        uint16_t shutter = ((uint16_t)burst[PMW3610_BURST_SHUTTER_HI] << 8) | burst[PMW3610_BURST_SHUTTER_LO];
        
        bool should_enable = (shutter < PMW3610_SMART_MODE_THRESHOLD);
        
        /* Check if state needs change */
        if (should_enable != pmw3610_smart_enabled) {
            pmw3610_smart_counter++;
            if (pmw3610_smart_counter > PMW3610_SMART_MODE_DEBOUNCE) {
                /* Execute Switch */
                if (pmw3610_spi_clk_on()) {
                    uint8_t cmd = should_enable ? PMW3610_SMART_MODE_ENABLE : PMW3610_SMART_MODE_DISABLE;
                    pmw3610_write_raw(PMW3610_REG_SMART_MODE, cmd);
                    pmw3610_spi_clk_off();
                    pmw3610_smart_enabled = should_enable;
                }
                pmw3610_smart_counter = 0;
            }
        } else {
            pmw3610_smart_counter = 0; /* Reset if stable */
        }
    }
#endif

    if (has_motion) {
        uint8_t xl = burst[PMW3610_BURST_X_L];
        uint8_t yl = burst[PMW3610_BURST_Y_L];
        uint8_t xyh = burst[PMW3610_BURST_XY_H];

        int16_t dx = (int16_t)(xl | ((uint16_t)(xyh & 0xF0) << 4));
        int16_t dy = (int16_t)(yl | ((uint16_t)(xyh & 0x0F) << 8));

        /* Sign extend 12-bit */
        if (dx & 0x0800) dx |= 0xF000;
        if (dy & 0x0800) dy |= 0xF000;

#if defined(PMW3610_SWAP_XY)
        int16_t tmp = dx; dx = dy; dy = tmp;
#endif
#if defined(PMW3610_INVERT_X)
        dx = -dx;
#endif
#if defined(PMW3610_INVERT_Y)
        dy = -dy;
#endif

        carry_x += dx;
        carry_y += dy;
    }

    int16_t out_x = CONSTRAIN_HID_XY(carry_x);
    int16_t out_y = CONSTRAIN_HID_XY(carry_y);

    mouse_report.x = out_x;
    mouse_report.y = out_y;

    carry_x -= out_x;
    carry_y -= out_y;

    return mouse_report;
}