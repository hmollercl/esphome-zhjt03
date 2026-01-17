#include "zhjt03_climate.h"
#include "esphome/core/log.h"

namespace esphome {
namespace zhjt03 {

static const char *const TAG = "zhjt03.climate";

// ---- helpers para mapear strings a enums ----
static climate::ClimateMode parse_mode(const std::string &s) {
  if (s == "OFF") return climate::CLIMATE_MODE_OFF;
  if (s == "AUTO") return climate::CLIMATE_MODE_AUTO;
  if (s == "COOL") return climate::CLIMATE_MODE_COOL;
  if (s == "HEAT") return climate::CLIMATE_MODE_HEAT;
  if (s == "DRY") return climate::CLIMATE_MODE_DRY;
  if (s == "FAN") return climate::CLIMATE_MODE_FAN_ONLY;
  return climate::CLIMATE_MODE_COOL;
}
static climate::ClimateFanMode parse_fan(const std::string &s) {
  if (s == "LOW") return climate::CLIMATE_FAN_LOW;
  if (s == "MEDIUM") return climate::CLIMATE_FAN_MEDIUM;
  if (s == "HIGH") return climate::CLIMATE_FAN_HIGH;
  return climate::CLIMATE_FAN_AUTO;
}
static climate::ClimateSwingMode parse_swing(const std::string &s) {
  if (s == "ON") return climate::CLIMATE_SWING_VERTICAL;
  return climate::CLIMATE_SWING_OFF;
}

void ZHJT03Climate::set_defaults(const std::string &mode, const std::string &fan, int temp, const std::string &swing) {
  this->def_mode_ = parse_mode(mode);
  this->def_fan_ = parse_fan(fan);
  this->def_temp_ = temp;
  this->def_swing_ = parse_swing(swing);
}

void ZHJT03Climate::setup() {
  // prefs
  this->pref_ = global_preferences->make_preference<Persisted>(0xA17C3E11);

  Persisted loaded{};
  if (this->pref_.load(&loaded) && loaded.magic == 0x42) {
    this->st_ = loaded;
    ESP_LOGI(TAG, "Estado restaurado: power=%d temp=%d", (int) this->st_.power, (int) this->st_.target_temp);
  } else {
    this->st_.magic = 0x42;
    this->st_.power = false;
    this->st_.mode = (uint8_t) this->def_mode_;
    this->st_.fan = (uint8_t) this->def_fan_;
    this->st_.swing = (uint8_t) this->def_swing_;
    this->st_.target_temp = (uint8_t) this->def_temp_;
    this->pref_.save(&this->st_);
    ESP_LOGI(TAG, "Estado inicializado (defaults).");
  }

  // set climate current state from persisted
  this->mode = this->st_.power ? (climate::ClimateMode) this->st_.mode : climate::CLIMATE_MODE_OFF;
  this->fan_mode = (climate::ClimateFanMode) this->st_.fan;
  this->swing_mode = (climate::ClimateSwingMode) this->st_.swing;
  this->target_temperature = this->st_.target_temp;

  this->publish_state();
}

void ZHJT03Climate::loop() {
  // publicar current_temperature desde el sensor (si existe)
  if (this->ambient_ != nullptr && (millis() - this->last_publish_ms_) > 30000) {
    this->current_temperature = this->ambient_->state;
    this->last_publish_ms_ = millis();
    this->publish_state();
  }
}

void ZHJT03Climate::dump_config() {
  ESP_LOGCONFIG(TAG, "ZHJT03 Climate");
  ESP_LOGCONFIG(TAG, "  Limits: %d..%d step %d", this->min_t_, this->max_t_, this->step_);
  LOG_SENSOR("  ", "Ambient", this->ambient_);
}

climate::ClimateTraits ZHJT03Climate::traits() {
  climate::ClimateTraits t;
  t.set_supports_current_temperature(true);
  t.set_visual_min_temperature(this->min_t_);
  t.set_visual_max_temperature(this->max_t_);
  t.set_visual_temperature_step(this->step_);

  t.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_AUTO,
      climate::CLIMATE_MODE_COOL,
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_DRY,
      climate::CLIMATE_MODE_FAN_ONLY,
  });

  t.set_supported_fan_modes({
      climate::CLIMATE_FAN_AUTO,
      climate::CLIMATE_FAN_LOW,
      climate::CLIMATE_FAN_MEDIUM,
      climate::CLIMATE_FAN_HIGH,
  });

  t.set_supported_swing_modes({
      climate::CLIMATE_SWING_OFF,
      climate::CLIMATE_SWING_VERTICAL,  // lo usamos como ON (si después implementas fixed/natural)
  });

  return t;
}

void ZHJT03Climate::control(const climate::ClimateCall &call) {
  bool send = false;
  bool toggle_power = false;

  // target temp
  if (call.get_target_temperature().has_value()) {
    float tt = *call.get_target_temperature();
    int t = (int) lroundf(tt);
    if (t < this->min_t_) t = this->min_t_;
    if (t > this->max_t_) t = this->max_t_;
    this->target_temperature = t;
    this->st_.target_temp = (uint8_t) t;
    send = true;
  }

  // fan
  if (call.get_fan_mode().has_value()) {
    this->fan_mode = *call.get_fan_mode();
    this->st_.fan = (uint8_t) this->fan_mode.value();
    send = true;
  }

  // swing
  if (call.get_swing_mode().has_value()) {
    this->swing_mode = *call.get_swing_mode();
    this->st_.swing = (uint8_t) this->swing_mode.value();
    send = true;
  }

  // mode / power
  if (call.get_mode().has_value()) {
    auto m = *call.get_mode();

    if (m == climate::CLIMATE_MODE_OFF) {
      if (this->st_.power) {
        toggle_power = true;   // ZHJT03 es toggle
        this->st_.power = false;
        this->mode = climate::CLIMATE_MODE_OFF;
        send = true;
      }
    } else {
      // si estaba apagado y te piden un modo “ON”, hay que togglear power
      if (!this->st_.power) {
        toggle_power = true;
        this->st_.power = true;
      }
      this->mode = m;
      this->st_.mode = (uint8_t) m;
      send = true;
    }
  }

  // persist + send
  this->pref_.save(&this->st_);
  this->publish_state();

  if (send) {
    this->send_state_(toggle_power);
  }
}

// ===================== IR ENCODER =====================
// Basado en la especificación del frame (6 words) descrita en README: header 6234/7392 y footer 608/7372/616 :contentReference[oaicite:2]{index=2}
//
// OJO: el orden de bits puede variar por equipo.
// - Si no te funciona, cambia send_word_msb_ <-> send_word_lsb_ en send_words_necish_.

void ZHJT03Climate::send_state_(bool toggle_power) {
  if (this->tx_ == nullptr) {
    ESP_LOGE(TAG, "No transmitter configured");
    return;
  }

  uint16_t w[6];
  this->build_frame_words_(w, toggle_power);

  ESP_LOGD(TAG, "Sending words: %04X %04X %04X %04X %04X %04X",
           w[0], w[1], w[2], w[3], w[4], w[5]);

  this->send_words_necish_(w);
}

static uint16_t merge_nibbles(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
  return (uint16_t)((a & 0xF) << 12) | (uint16_t)((b & 0xF) << 8) | (uint16_t)((c & 0xF) << 4) | (uint16_t)(d & 0xF);
}

static uint8_t temp_to_code(int t) {
  // tabla del README: 16->0xF0, 17->0x78, 18->0xB4 ... 31->0x0F; 32 comparte con 16 :contentReference[oaicite:3]{index=3}
  // aquí implemento el patrón “Gray-ish” como tabla explícita (16..31)
  static const uint8_t tbl[16] = {
      0xF0, 0x78, 0xB4, 0x3C, 0xD2, 0x5A, 0x96, 0x1E,
      0xE1, 0x69, 0xA5, 0x2D, 0xC3, 0x4B, 0x87, 0x0F
  };
  if (t < 16) t = 16;
  if (t > 31) t = 31;
  return tbl[t - 16];
}

static uint8_t mode_code(climate::ClimateMode m, int t) {
  // según README:
  // auto=_f_0, cool=_b_4 (16-31) o _3_c (32), heat=_e_1 (16-31) o _6_9 (32), dry=_d_2, fan=_9_6 (16-31) o _1_e (32) :contentReference[oaicite:4]{index=4}
  // acá ignoramos 32 (tú usarás 16-31), así que usamos siempre “16-31”.
  switch (m) {
    case climate::CLIMATE_MODE_AUTO: return 0xF0;
    case climate::CLIMATE_MODE_COOL: return 0xB4;
    case climate::CLIMATE_MODE_HEAT: return 0xE1;
    case climate::CLIMATE_MODE_DRY: return 0xD2;
    case climate::CLIMATE_MODE_FAN_ONLY: return 0x96;
    default: return 0xB4;
  }
}

static uint16_t fan_word(bool power, climate::ClimateFanMode fan, climate::ClimateSwingMode swing) {
  // Fan settings (#4) = (power/swing/sleep nibble-pair) + (speed/air flow nibble-pair) :contentReference[oaicite:5]{index=5}
  //
  // Tomamos swing OFF=fixed (b_4_) y ON=horizontal (a_5_) por ahora; sleep off.
  uint8_t ps = 0xB4; // swing fixed
  if (!power) ps = 0xF0; // "Power off with swing fixed" según tabla de power/swing/sleep (f_0_) :contentReference[oaicite:6]{index=6}
  if (swing != climate::CLIMATE_SWING_OFF) ps = power ? 0xA5 : 0xE1; // horizontal / power-off-horizontal

  uint8_t sp = 0xF0; // smart/auto
  if (fan == climate::CLIMATE_FAN_LOW) sp = 0x96;
  else if (fan == climate::CLIMATE_FAN_MEDIUM) sp = 0xD2;
  else if (fan == climate::CLIMATE_FAN_HIGH) sp = 0xB4;

  // word = a956-style (2 bytes): high byte = ps, low byte = sp (como en ejemplos del README)
  return (uint16_t)(ps << 8) | sp;
}

void ZHJT03Climate::build_frame_words_(uint16_t out_words[6], bool toggle_power) {
  // Estructura 6 words según README: [timer][extra][main][fan][temp+mode][footer] :contentReference[oaicite:7]{index=7}
  out_words[0] = 0xFF00; // no timer
  out_words[1] = 0xFF00; // no extra

  // main: o mandas POWER toggle, o mandas “set mode/temp/fan” (en muchos AC igual manda frame completo siempre).
  // aquí: si toggle_power => main=ff00; si no, main=7f80 (change mode) para “forzar estado”.
  out_words[2] = toggle_power ? 0xFF00 : 0x7F80;

  bool power = this->st_.power;
  auto cmode = this->mode;
  if (!power) cmode = climate::CLIMATE_MODE_OFF;

  out_words[3] = fan_word(power, this->fan_mode.value_or(climate::CLIMATE_FAN_AUTO),
                          this->swing_mode.value_or(climate::CLIMATE_SWING_OFF));

  int t = (int) this->target_temperature;
  uint8_t temp = temp_to_code(t);
  uint8_t mod = mode_code(this->mode, t);

  out_words[4] = (uint16_t)(temp << 8) | mod;

  out_words[5] = 0x54AB; // footer fijo :contentReference[oaicite:8]{index=8}
}

void ZHJT03Climate::send_words_necish_(const uint16_t words[6]) {
  // ESPHome raw transmitter API
  remote_transmitter::RemoteTransmitData data(this->tx_);

  data.set_carrier_frequency(38000);

  // header
  data.item(HDR_MARK, HDR_SPACE);

  // payload: 6 words x 16 bits
  for (int i = 0; i < 6; i++) {
    // PRUEBA 1: MSB first (si no funciona, cambia por send_word_lsb_)
    // send_word_msb_(words[i]) emite via data.item, pero aquí lo hacemos inline:
    for (int b = 15; b >= 0; b--) {
      bool bit = (words[i] >> b) & 1;
      data.item(BIT_MARK, bit ? ONE_SPACE : ZERO_SPACE);
    }
  }

  // footer (tres señales)
  data.item(FTR_MARK1, FTR_SPACE);
  data.mark(FTR_MARK2);

  this->tx_->transmit(data);
}

// (no usadas por ahora, las dejo por si quieres refactor)
void ZHJT03Climate::send_mark_(uint32_t usec) {}
void ZHJT03Climate::send_space_(uint32_t usec) {}
void ZHJT03Climate::send_bit_(bool bit) {}
void ZHJT03Climate::send_word_msb_(uint16_t w) {}
void ZHJT03Climate::send_word_lsb_(uint16_t w) {}

}  // namespace zhjt03
}  // namespace esphome
