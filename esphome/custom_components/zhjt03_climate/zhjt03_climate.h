#pragma once

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace zhjt03 {

class ZHJT03Climate : public climate::Climate, public Component {
 public:
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *tx) { this->tx_ = tx; }
  void set_ambient_sensor(sensor::Sensor *s) { this->ambient_ = s; }

  void set_limits(int min_t, int max_t, int step) {
    this->min_t_ = min_t; this->max_t_ = max_t; this->step_ = step;
  }

  void set_defaults(const std::string &mode, const std::string &fan, int temp, const std::string &swing);

  void setup() override;
  void loop() override;
  void dump_config() override;

  climate::ClimateTraits traits() override;
  void control(const climate::ClimateCall &call) override;

 protected:
  // --- IR protocol ---
  void send_state_(bool toggle_power);
  void build_frame_words_(uint16_t out_words[6], bool toggle_power);

  void send_words_necish_(const uint16_t words[6]);
  void send_bit_(bool bit);
  void send_mark_(uint32_t usec);
  void send_space_(uint32_t usec);

  // bit order helpers (puedes invertir si tu AC usa al revés)
  void send_word_msb_(uint16_t w);
  void send_word_lsb_(uint16_t w);

  // --- state ---
  remote_transmitter::RemoteTransmitterComponent *tx_{nullptr};
  sensor::Sensor *ambient_{nullptr};

  ESPPreferenceObject pref_;
  struct Persisted {
    uint8_t magic;
    bool power;
    uint8_t mode;   // climate::ClimateMode enum index
    uint8_t fan;    // climate::ClimateFanMode enum index
    uint8_t swing;  // climate::ClimateSwingMode enum index
    uint8_t target_temp;
  } st_{};

  int min_t_{16}, max_t_{31}, step_{1};

  // defaults
  climate::ClimateMode def_mode_{climate::CLIMATE_MODE_COOL};
  climate::ClimateFanMode def_fan_{climate::CLIMATE_FAN_AUTO};
  climate::ClimateSwingMode def_swing_{climate::CLIMATE_SWING_OFF};
  int def_temp_{24};

  // publishing cadence
  uint32_t last_publish_ms_{0};

  // --- ZH/JT-03 timings (según spec del repo: header 6234/7392, footer 608/7372/616) :contentReference[oaicite:1]{index=1}
  static constexpr uint32_t HDR_MARK = 6234;
  static constexpr uint32_t HDR_SPACE = 7392;
  static constexpr uint32_t BIT_MARK = 560;
  static constexpr uint32_t ZERO_SPACE = 560;
  static constexpr uint32_t ONE_SPACE = 1690;
  static constexpr uint32_t FTR_MARK1 = 608;
  static constexpr uint32_t FTR_SPACE = 7372;
  static constexpr uint32_t FTR_MARK2 = 616;
};

}  // namespace zhjt03
}  // namespace esphome
