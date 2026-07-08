#include "board_io.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "board_io";

#define I2C_MASTER_NUM   I2C_NUM_0
#define I2C_SDA_PIN      39
#define I2C_SCL_PIN      40
#define I2C_TIMEOUT      pdMS_TO_TICKS(100)

/* TCA9535-registers (16-bit, little-endian over twee bytes) */
#define TCA9535_REG_INPUT   0x00
#define TCA9535_REG_OUTPUT  0x02
#define TCA9535_REG_CONFIG  0x06

static bool     s_i2c_ready   = false;
static uint8_t  s_ioexp_addr  = 0;      /* 0 = nog niet gedetecteerd */
static uint16_t s_output_reg  = 0xFFFF; /* schaduwregisters — power-on default */
static uint16_t s_config_reg  = 0xFFFF; /* 1 = input */

void board_i2c_init(void) {
    if (s_i2c_ready) return;

    i2c_config_t cfg = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = I2C_SDA_PIN,
        .scl_io_num       = I2C_SCL_PIN,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &cfg));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, cfg.mode, 0, 0, 0));
    s_i2c_ready = true;
    ESP_LOGI(TAG, "I2C-bus klaar (SDA=%d SCL=%d)", I2C_SDA_PIN, I2C_SCL_PIN);
}

esp_err_t board_i2c_probe(uint8_t addr) {
    uint8_t dummy;
    return board_i2c_read_reg8(addr, 0x00, &dummy, 1);
}

esp_err_t board_i2c_write_reg8(uint8_t addr, uint8_t reg,
                               const uint8_t *data, size_t len) {
    uint8_t buf[8];
    if (len + 1 > sizeof(buf)) return ESP_ERR_INVALID_SIZE;
    buf[0] = reg;
    for (size_t i = 0; i < len; i++) buf[i + 1] = data[i];
    return i2c_master_write_to_device(I2C_MASTER_NUM, addr,
                                      buf, len + 1, I2C_TIMEOUT);
}

esp_err_t board_i2c_read_reg8(uint8_t addr, uint8_t reg,
                              uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, addr,
                                        &reg, 1, data, len, I2C_TIMEOUT);
}

static esp_err_t tca9535_write16(uint8_t reg, uint16_t val) {
    uint8_t data[2] = { (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
    return board_i2c_write_reg8(s_ioexp_addr, reg, data, 2);
}

static esp_err_t tca9535_read16(uint8_t reg, uint16_t *val) {
    uint8_t data[2];
    esp_err_t err = board_i2c_read_reg8(s_ioexp_addr, reg, data, 2);
    if (err == ESP_OK) *val = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return err;
}

esp_err_t ioexp_init(void) {
    if (s_ioexp_addr != 0) return ESP_OK;
    board_i2c_init();

    /* Adres 0x20 op de meeste revisies, 0x39 als alternatief (Seeed BSP) */
    const uint8_t candidates[] = { 0x20, 0x39 };
    for (size_t i = 0; i < sizeof(candidates); i++) {
        if (i2c_master_write_read_device(I2C_MASTER_NUM, candidates[i],
                (uint8_t[]){TCA9535_REG_INPUT}, 1,
                (uint8_t[2]){0}, 2, I2C_TIMEOUT) == ESP_OK) {
            s_ioexp_addr = candidates[i];
            break;
        }
    }
    if (s_ioexp_addr == 0) {
        ESP_LOGE(TAG, "TCA9535 IO-expander niet gevonden op 0x20/0x39");
        return ESP_FAIL;
    }

    /* Schaduwregisters seeden met de echte hardware-toestand, zodat
       set_level/set_direction geen ongerelateerde pinnen omgooien */
    tca9535_read16(TCA9535_REG_OUTPUT, &s_output_reg);
    tca9535_read16(TCA9535_REG_CONFIG, &s_config_reg);

    ESP_LOGI(TAG, "TCA9535 op 0x%02X (out=0x%04X cfg=0x%04X)",
             s_ioexp_addr, s_output_reg, s_config_reg);
    return ESP_OK;
}

esp_err_t ioexp_set_level(uint8_t pin, bool level) {
    if (s_ioexp_addr == 0 || pin >= 16) return ESP_ERR_INVALID_STATE;
    if (level) s_output_reg |=  (1u << pin);
    else       s_output_reg &= ~(1u << pin);
    return tca9535_write16(TCA9535_REG_OUTPUT, s_output_reg);
}

esp_err_t ioexp_set_direction(uint8_t pin, bool output) {
    if (s_ioexp_addr == 0 || pin >= 16) return ESP_ERR_INVALID_STATE;
    if (output) s_config_reg &= ~(1u << pin);
    else        s_config_reg |=  (1u << pin);
    return tca9535_write16(TCA9535_REG_CONFIG, s_config_reg);
}
