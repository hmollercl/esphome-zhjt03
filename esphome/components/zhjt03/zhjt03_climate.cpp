#include "zhjt03_climate.h"
#include "esphome/core/log.h"
#include <cmath>

namespace esphome {
namespace zhjt03 {

static uint8_t temp_code(uint8_t t) {
  static const uint8_t table[16] = {0x00,0x20,0x40,0x60,0x80,0xA0,0xC0,0xE0,0xE1,0x69,0x49,0x29,0x09,0x89,0xA9,0xC9};
  return table[t - 16];
}

static uint8_t mode_code(climate::ClimateMode m) {
  switch (m) {
    case climate::CLIMATE_MODE_COOL: return 0xB4;
    case climate::CLIMATE_MODE_HEAT: return 0xE1;
    case climate::CLIMATE_MODE_DRY:  return 0x94;
    case climate::CLIMATE_MODE_FAN_ONLY: return 0xF4;
    case climate::CLIMATE_MODE_AUTO: return 0xA4;
    default: return 0xB4;
  }
}

// Fan auto + swing fijo (ajustable después)
static const uint16_t FAN_AUTO_FIXED = 0xBF40;

climate::ClimateTraits ZHJT03Climate::traits() {
  auto t = climate::ClimateTraits();
  t.set_supports_current_temperature(true);
  t.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_AUTO, climate::CLIMATE_MODE_COOL,
                         climate::CLIMATE_MODE_HEAT, climate::CLIMATE_MODE_DRY, climate::CLIMATE_MODE_FAN_ONLY});
  t.set_visual_min_temperature(16);
  t.set_visual_max_temperature(31);
  t.set_visual_temperature_step(1);
  return t;
}

void ZHJT03Climate::control(const climate::ClimateCall &call) {
  if (call.get_mode().has_value()) {
    auto m = *call.get_mode();
    if (m == climate::CLIMATE_MODE_OFF) {
      if (this->power_) {
        this->send_power_toggle_();
        this->power_ = false;
      }
      this->mode_ = climate::CLIMATE_MODE_OFF;
      this->publish_state();
      return;
    }
    this->mode_ = m;
    this->power_ = true;
  }

  if (call.get_target_temperature().has_value())
    this->target_temp_ = *call.get_target_temperature();

  this->send_state_frame_();
  this->publish_state();
}

void ZHJT03Climate::send_power_toggle_() {
  this->transmit_frame_(0xFF00, 0xFF00, 0xFF00, 0x0000, 0x0000, 0x54AB);
}

void ZHJT03Climate::send_state_frame_() {
  uint8_t t = (uint8_t) lroundf(this->target_temp_);
  if (t < 16) t = 16;
  if (t > 31) t = 31;

  uint16_t temp_mode = (temp_code(t) << 8) | mode_code(this->mode_);
  this->transmit_frame_(0xFF00, 0xFF00, 0x7F80, FAN_AUTO_FIXED, temp_mode, 0x54AB);
}

void ZHJT03Climate::transmit_frame_(uint16_t timer, uint16_t extra, uint16_t main_cmd, uint16_t fan, uint16_t temp_mode, uint16_t footer) {
  if (this->tx_ == nullptr) return;

  this->tx_->transmit([&](remote_transmitter::RemoteTransmitData *d) {
    d->set_carrier_frequency(38000);

    auto send_word = [&](uint16_t w) {
      for (int i = 15; i >= 0; i--) {
        d->mark(560);
        d->space((w & (1 << i)) ? 1690 : 560);
      }
    };

    d->mark(6234);
    d->space(7392);

    send_word(timer);
    send_word(extra);
    send_word(main_cmd);
    send_word(fan);
    send_word(temp_mode);
    send_word(footer);

    d->mark(608);
    d->space(7372);
    d->mark(616);
  });
}

}  // namespace zhjt03
}  // namespace esphome
