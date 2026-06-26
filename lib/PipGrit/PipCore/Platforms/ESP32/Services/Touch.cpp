#include <PipCore/Config/Features.hpp>

#if PIPCORE_TARGET_ESP32

#include <PipCore/Platforms/ESP32/Services/Touch.hpp>

#include <driver/gpio.h>
#include <esp_attr.h>
#include <esp_log.h>

#include <algorithm>
#include <cstring>

namespace pipcore::esp32::services
{
    namespace
    {
        constexpr const char *kTag = "Touch";
        Touch *g_instance = nullptr;
    }

    void IRAM_ATTR Touch::markTouched(Touch *self) noexcept
    {
        if (self)
            self->_touchedFlag = true;
    }

    namespace
    {
        void IRAM_ATTR touchIsrHandler(void * /*arg*/)
        {
            Touch::markTouched(g_instance);
        }
    }

    Touch::~Touch()
    {
        end();
    }

    bool Touch::configure(int8_t sda,
                          int8_t scl,
                          int8_t intr,
                          uint8_t i2cAddr,
                          uint32_t freqHz,
                          uint16_t width,
                          uint16_t height,
                          uint8_t rotation) noexcept
    {
        end();
        if (intr < 0)
        {
            ESP_LOGE(kTag, "configure: INT pin is mandatory (got -1)");
            return false;
        }
        _width = width;
        _height = height;
        _rotation = rotation & 3u;
        _intr = intr;
        _dev.configure(sda, scl, intr, i2cAddr, freqHz, 0);
        return true;
    }

    bool Touch::begin() noexcept
    {
        if (_ready)
            return true;
        if (_intr < 0)
        {
            ESP_LOGE(kTag, "begin: INT pin not configured");
            return false;
        }
        if (!_dev.init())
            return false;

        for (uint8_t i = 0; i < MaxPoints; ++i)
            _slots[i] = Slot{};
        _reported = 0;
        _touchedFlag = false;

        const gpio_num_t intPin = static_cast<gpio_num_t>(_intr);
        gpio_config_t io{};
        io.pin_bit_mask = 1ULL << static_cast<uint8_t>(_intr);
        io.mode = GPIO_MODE_INPUT;
        io.pull_up_en = GPIO_PULLUP_ENABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type = GPIO_INTR_NEGEDGE;
        if (gpio_config(&io) != ESP_OK)
        {
            ESP_LOGE(kTag, "gpio_config failed for INT pin %d", _intr);
            return false;
        }

        {
            const esp_err_t isrErr = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
            if (isrErr != ESP_OK && isrErr != ESP_ERR_INVALID_STATE)
            {
                ESP_LOGE(kTag, "gpio_install_isr_service failed: %s", esp_err_to_name(isrErr));
                return false;
            }
        }

        g_instance = this;
        if (gpio_isr_handler_add(intPin, touchIsrHandler, nullptr) != ESP_OK)
        {
            ESP_LOGE(kTag, "gpio_isr_handler_add failed for pin %d", _intr);
            g_instance = nullptr;
            return false;
        }
        _isrAttached = true;

        _ready = true;
        ESP_LOGI(kTag, "ready (INT pin %d, addr 0x%02X)", _intr, PIPCORE_TOUCH_I2C_ADDR);
        return true;
    }

    void Touch::end() noexcept
    {
        if (_isrAttached)
        {
            gpio_isr_handler_remove(static_cast<gpio_num_t>(_intr));
            _isrAttached = false;
        }
        if (g_instance == this)
            g_instance = nullptr;

        if (_ready)
        {
            _dev.deinit();
            _ready = false;
        }
        _reported = 0;
        _touchedFlag = false;
        for (uint8_t i = 0; i < MaxPoints; ++i)
            _slots[i] = Slot{};
    }

    Touch::Slot *Touch::findSlotById(uint8_t id) noexcept
    {
        for (uint8_t i = 0; i < MaxPoints; ++i)
            if (_slots[i].id == id)
                return &_slots[i];
        return nullptr;
    }

    Touch::Slot *Touch::findFreeSlot() noexcept
    {
        for (uint8_t i = 0; i < MaxPoints; ++i)
            if (_slots[i].id == 0xFF)
                return &_slots[i];
        return nullptr;
    }

    void Touch::remap(uint16_t inX, uint16_t inY, uint16_t &outX, uint16_t &outY) const noexcept
    {
        switch (_rotation)
        {
        default:
        case 0:
            outX = inX;
            outY = inY;
            break;
        case 1:
            outX = inY;
            outY = static_cast<uint16_t>(_height - 1 - inX);
            break;
        case 2:
            outX = static_cast<uint16_t>(_width - 1 - inX);
            outY = static_cast<uint16_t>(_height - 1 - inY);
            break;
        case 3:
            outX = static_cast<uint16_t>(_width - 1 - inY);
            outY = static_cast<uint16_t>(_height - 1 - inX);
            break;
        }
        if (outX >= _width)
            outX = static_cast<uint16_t>(_width - 1);
        if (outY >= _height)
            outY = static_cast<uint16_t>(_height - 1);
    }

    void Touch::update() noexcept
    {
        if (!_ready)
            return;

        const bool flag = _touchedFlag;
        _touchedFlag = false;

        if (!flag && _reported == 0)
        {
            return;
        }

        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            _slots[i].wasPresent = _slots[i].present;
            _slots[i].present = false;
        }

        Ft6336u::RawPoint raw[MaxPoints];
        const uint8_t n = _dev.read(raw);

        for (uint8_t i = 0; i < n; ++i)
        {
            if (!raw[i].valid)
                continue;

            Slot *s = findSlotById(raw[i].id);
            if (!s)
                s = findFreeSlot();
            if (!s)
                continue;

            const bool fresh = (s->id == 0xFF);
            if (fresh)
                s->id = raw[i].id;

            uint16_t mx = 0, my = 0;
            remap(raw[i].x, raw[i].y, mx, my);

            const bool moved = !fresh && (mx != s->x || my != s->y);
            s->x = mx;
            s->y = my;
            s->present = true;

            if (fresh || !s->wasPresent)
                s->state = pipcore::TouchState::Pressed;
            else if (moved)
                s->state = pipcore::TouchState::Moved;
            else
                s->state = pipcore::TouchState::Held;
        }

        uint8_t reported = 0;
        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            Slot &s = _slots[i];
            if (!s.present)
            {
                if (s.wasPresent)
                    s.state = pipcore::TouchState::Released;
                else if (s.state == pipcore::TouchState::Released)
                    s.id = 0xFF;
            }
            if (s.state != pipcore::TouchState::Released && s.id != 0xFF)
                ++reported;
        }
        _reported = reported;
    }

    pipcore::TouchPoint Touch::point(uint8_t index) const noexcept
    {
        if (index >= MaxPoints)
            return {};

        uint8_t out = 0;
        for (uint8_t i = 0; i < MaxPoints; ++i)
        {
            const Slot &s = _slots[i];
            if (s.id == 0xFF || s.state == pipcore::TouchState::Released)
                continue;
            if (out == index)
            {
                pipcore::TouchPoint p;
                p.x = s.x;
                p.y = s.y;
                p.id = s.id;
                p.state = s.state;
                return p;
            }
            ++out;
        }
        return {};
    }
}

#endif
