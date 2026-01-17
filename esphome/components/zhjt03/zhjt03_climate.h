#pragma once
#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace zhjt03 {

class ZHJT03Climate : public climate::Climate, public Component {
 public:
  void set_temperature_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *tx) { this->tx_ = tx; }

  void setup() override {}
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

 protected:
  sensor::Sensor *sensor_{nullptr};
  remote_transmitter::RemoteTransmitterComponent *tx_{nullptr};

  bool power_{false};
  //climate::ClimateMode mode_{climate::CLIMATE_MODE_OFF};
  //float target_temp_{24};

  void send_state_frame_();
  void send_power_toggle_();
  void transmit_frame_(uint16_t timer, uint16_t extra, uint16_t main_cmd, uint16_t fan, uint16_t temp_mode, uint16_t footer);
};

}  // namespace zhjt03
}  // namespace esphome
