#include <PipCore/Config/Features.hpp>

#if PIPCORE_TARGET_ESP32

#include <PipCore/Platforms/ESP32/Peripherals/Ft6336u.hpp>

#include <driver/i2c.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>

#include <cstring>

namespace pipcore::esp32
{
    namespace
    {
        constexpr const char *kTag = "FT6336U";

        constexpr uint8_t RegDevMode = 0x00;
        constexpr uint8_t RegGestureId = 0x01;
        constexpr uint8_t RegTdStatus = 0x02;
        constexpr uint8_t RegP1 = 0x03;
        constexpr uint8_t RegP2 = 0x09;
        constexpr uint8_t RegThreshold = 0x80;
        constexpr uint8_t RegGMode = 0xA4;
        constexpr uint8_t RegPeriodActive = 0x88;

        inline constexpr bool validPin(int8_t p) noexcept { return p >= 0; }
    }

    void Ft6336u::configure(int8_t sda,
                            int8_t scl,
                            int8_t intr,
                            uint8_t addr7,
                            uint32_t freqHz,
                            int port) noexcept
    {
        deinit();
        _sda = sda;
        _scl = scl;
        _intr = intr;
        _addr7 = addr7 ? addr7 : DefaultAddress;
        _freqHz = freqHz ? freqHz : 400000U;
        _port = port;
    }

    bool Ft6336u::init() noexcept
    {
        if (_initialized)
            return true;

        if (!validPin(_sda) || !validPin(_scl))
        {
            ESP_LOGE(kTag, "init: invalid SDA/SCL pins");
            return false;
        }

        const i2c_port_t port = static_cast<i2c_port_t>(_port);

        i2c_config_t conf{};
        conf.mode = I2C_MODE_MASTER;
        conf.sda_io_num = static_cast<gpio_num_t>(_sda);
        conf.scl_io_num = static_cast<gpio_num_t>(_scl);
        conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        conf.master.clk_speed = _freqHz;
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || \
    defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
        conf.clk_flags = 0;
#endif

        esp_err_t err = i2c_param_config(port, &conf);
        if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_param_config failed: %s", esp_err_to_name(err));
            return false;
        }

        err = i2c_driver_install(port, conf.mode, 0, 0, 0);
        if (err == ESP_ERR_INVALID_STATE)
        {
            _busOwned = false;
        }
        else if (err != ESP_OK)
        {
            ESP_LOGE(kTag, "i2c_driver_install failed: %s", esp_err_to_name(err));
            return false;
        }
        else
        {
            _busOwned = true;
        }

        uint8_t devMode = 0xFF;
        if (!readReg(RegDevMode, devMode) || (devMode & 0x70) != 0)
        {
            if (!readReg(RegDevMode, devMode))
            {
                ESP_LOGE(kTag, "no ACK at addr 0x%02X", _addr7);
                if (_busOwned)
                {
                    i2c_driver_delete(port);
                    _busOwned = false;
                }
                return false;
            }
        }

        (void)writeReg(RegGMode, 0x00);

        _initialized = true;
        ESP_LOGI(kTag, "init ok (addr=0x%02X, sda=%d scl=%d, %s)",
                 _addr7, _sda, _scl, _busOwned ? "owned bus" : "shared bus");
        return true;
    }

    void Ft6336u::deinit() noexcept
    {
        if (_busOwned)
        {
            i2c_driver_delete(static_cast<i2c_port_t>(_port));
            _busOwned = false;
        }
        _initialized = false;
    }

    bool Ft6336u::writeReg(uint8_t reg, uint8_t value) noexcept
    {
        const i2c_port_t port = static_cast<i2c_port_t>(_port);
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, static_cast<uint8_t>(_addr7 << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_write_byte(cmd, value, true);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        return err == ESP_OK;
    }

    bool Ft6336u::readReg(uint8_t reg, uint8_t &out) noexcept
    {
        const i2c_port_t port = static_cast<i2c_port_t>(_port);
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, static_cast<uint8_t>(_addr7 << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, static_cast<uint8_t>(_addr7 << 1) | I2C_MASTER_READ, true);
        i2c_master_read_byte(cmd, &out, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        return err == ESP_OK;
    }

    bool Ft6336u::readRegs(uint8_t reg, uint8_t *buf, size_t len) noexcept
    {
        const i2c_port_t port = static_cast<i2c_port_t>(_port);
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, static_cast<uint8_t>(_addr7 << 1) | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, reg, true);
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, static_cast<uint8_t>(_addr7 << 1) | I2C_MASTER_READ, true);
        if (len > 1)
            i2c_master_read(cmd, buf, len - 1, I2C_MASTER_ACK);
        i2c_master_read_byte(cmd, buf + len - 1, I2C_MASTER_LAST_NACK);
        i2c_master_stop(cmd);
        esp_err_t err = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(10));
        i2c_cmd_link_delete(cmd);
        return err == ESP_OK;
    }

    static void decodePoint(const uint8_t *r, Ft6336u::RawPoint &p) noexcept
    {
        const uint16_t x = (static_cast<uint16_t>(r[0] & 0x0F) << 8) | r[1];
        const uint16_t y = (static_cast<uint16_t>(r[2] & 0x0F) << 8) | r[3];
        p.x = x;
        p.y = y;
        p.weight = r[4];
        p.id = (r[2] >> 4) & 0x0F;
        p.valid = true;
    }

    uint8_t Ft6336u::read(RawPoint *out) noexcept
    {
        for (uint8_t i = 0; i < MaxPoints; ++i)
            out[i].valid = false;

        if (!_initialized)
            return 0;

        uint8_t td = 0;
        if (!readReg(RegTdStatus, td))
            return 0;

        uint8_t n = td & 0x0F;
        if (n == 0)
            return 0;
        if (n > MaxPoints)
            n = MaxPoints;

        uint8_t buf[11] = {};
        const size_t want = (n >= 2) ? (RegP2 - RegP1 + 6u) : 6u;
        if (!readRegs(RegP1, buf, want))
            return 0;

        decodePoint(buf, out[0]);
        if (n >= 2)
            decodePoint(buf + (RegP2 - RegP1), out[1]);

        return n;
    }
}

#endif
