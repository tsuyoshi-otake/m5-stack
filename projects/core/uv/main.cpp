// uv — DFRobot SEN0636 (Gravity: 240370 Ultraviolet Index Sensor) の値を画面に出す。
//
// 結線: M5Stack Core の Grove ポート A（赤）に挿す。
//   Port A  SDA = G21 / SCL = G22 / 5V / GND
//   センサ側は 4 線 D/R(SDA) C/T(SCL) GND VCC で、基板上の Communication Mode Switch を
//   I2C 側にしておくこと。UART 側だと I2C では一切応答しない。
//   I2C アドレスは 0x23 固定（変更不可）。
//
// センサの出力は 3 つ:
//   readUvOriginalData() … UV 電圧 0-3300 mV
//   readUvIndexData()    … UV インデックス 0-11 の整数
//   readRiskLevelData()  … 危険度 0-4（Low / Moderate / High / Very High / Extreme）
// インデックスも危険度もセンサ側で算出済みなので、こちら側で換算する必要はない。
//
// BtnA で I2C 診断画面に切り替わる。Core 本体の電源 IC が同じバスの 0x75 にいるので、
// スキャン結果に 0x75 が出るかどうかでバス自体の生死とセンサ側の問題を切り分けられる。

#include <M5Unified.h>
#include <Wire.h>
#include <DFRobot_UVIndex240370Sensor.h>

namespace {

constexpr int PIN_SDA = 21;  // Grove Port A
constexpr int PIN_SCL = 22;
constexpr uint8_t SENSOR_ADDR = 0x23;  // ライブラリ内で固定されている値と同じ
constexpr uint8_t ONBOARD_PMU_ADDR = 0x75;  // Core 内蔵 IP5306。バス生存確認の目印
constexpr uint32_t I2C_FREQ = 100000;
constexpr uint32_t SAMPLE_INTERVAL_MS = 500;

DFRobot_UVIndex240370Sensor g_sensor(&Wire);

struct RiskStyle {
  const char* label;
  uint8_t r, g, b;
};

// readRiskLevelData() の 0-4 に対応。WHO の UV インデックス区分に合わせた配色。
constexpr RiskStyle RISK[] = {
    {"LOW", 0, 200, 80},        // 0
    {"MODERATE", 240, 210, 0},  // 1
    {"HIGH", 255, 140, 0},      // 2
    {"VERY HIGH", 240, 40, 40}, // 3
    {"EXTREME", 170, 60, 220},  // 4
};
constexpr size_t RISK_COUNT = sizeof(RISK) / sizeof(RISK[0]);

enum View : uint8_t { VIEW_UV, VIEW_DIAG };
View g_view = VIEW_UV;

// 直前に描いた値。変化したときだけ描き直して無駄な再描画を避ける。
int g_last_index = -1;
int g_last_level = -1;
int g_last_mv = -1;
bool g_last_online = false;
bool g_needs_full_redraw = true;

uint16_t riskColor(int level) {
  const RiskStyle& s = RISK[(level >= 0 && (size_t)level < RISK_COUNT) ? level : 0];
  return M5.Display.color565(s.r, s.g, s.b);
}

const char* riskLabel(int level) {
  return (level >= 0 && (size_t)level < RISK_COUNT) ? RISK[level].label : "---";
}

// センサが今も応答するかを ACK で確かめる。ライブラリの read 系はエラーを返さず、
// 抜けていても 0 や 0xFFFF をそのまま返してくるので、表示前にこれで判定する。
bool acks(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// 7bit アドレス空間を舐めて応答したものを集める。戻り値は見つかった数。
size_t scanBus(uint8_t* found, size_t max_found) {
  size_t n = 0;
  for (uint8_t addr = 0x08; addr <= 0x77 && n < max_found; ++addr) {
    if (acks(addr)) {
      found[n++] = addr;
    }
  }
  return n;
}

void drawTitle(const char* text, uint16_t bg) {
  auto& d = M5.Display;
  d.fillRect(0, 0, d.width(), 34, bg);
  d.setFont(&fonts::FreeSansBold12pt7b);
  d.setTextColor(TFT_WHITE, bg);
  d.setTextDatum(middle_center);
  d.setTextPadding(0);
  d.drawString(text, d.width() / 2, 17);
}

void drawUvChrome() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  drawTitle("UV INDEX  240-370nm", d.color565(20, 40, 90));
  d.drawFastHLine(0, 202, d.width(), d.color565(60, 60, 60));
}

void drawIndex(int index, bool online) {
  auto& d = M5.Display;
  d.setFont(&fonts::Font7);  // 7 セグ風。数字しか入っていないので数値表示専用
  d.setTextSize(2);
  d.setTextDatum(middle_center);
  d.setTextPadding(260);

  if (online) {
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.drawString(String(index).c_str(), d.width() / 2, 105);
  } else {
    d.setTextColor(d.color565(80, 80, 80), TFT_BLACK);
    d.drawString("--", d.width() / 2, 105);
  }

  d.setTextSize(1);
  d.setTextPadding(0);
}

void drawRisk(int level, bool online) {
  auto& d = M5.Display;
  const uint16_t color = online ? riskColor(level) : d.color565(70, 70, 70);

  // インデックスの下に危険度を色帯で出す。色だけでも一目で分かるようにしている。
  d.fillRect(40, 150, d.width() - 80, 4, color);

  d.setFont(&fonts::FreeSansBold18pt7b);
  d.setTextDatum(middle_center);
  d.setTextPadding(d.width());
  d.setTextColor(color, TFT_BLACK);
  d.drawString(online ? riskLabel(level) : "NO SENSOR", d.width() / 2, 178);
  d.setTextPadding(0);
}

void drawFooter(int mv, bool online) {
  auto& d = M5.Display;
  d.setFont(&fonts::FreeSans9pt7b);
  d.setTextColor(d.color565(150, 150, 150), TFT_BLACK);

  d.setTextDatum(middle_left);
  d.setTextPadding(160);
  d.drawString(online ? (String(mv) + " mV").c_str() : "BtnA: I2C diag", 10, 221);

  d.setTextDatum(middle_right);
  d.setTextPadding(150);
  d.setTextColor(online ? d.color565(0, 200, 80) : d.color565(240, 40, 40), TFT_BLACK);
  d.drawString(online ? "I2C 0x23 OK" : "I2C 0x23 LOST", d.width() - 10, 221);
  d.setTextPadding(0);
}

// 配線の切り分け用。バス上に何がいるかをそのまま出す。
void drawDiag() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  drawTitle("I2C DIAGNOSTICS", d.color565(90, 50, 10));

  uint8_t found[16];
  const size_t n = scanBus(found, sizeof(found));

  bool saw_pmu = false;
  bool saw_sensor = false;
  String list;
  for (size_t i = 0; i < n; ++i) {
    if (found[i] == ONBOARD_PMU_ADDR) saw_pmu = true;
    if (found[i] == SENSOR_ADDR) saw_sensor = true;
    if (i) list += " ";
    list += "0x" + String(found[i], HEX);
  }

  d.setFont(&fonts::FreeSans9pt7b);
  d.setTextDatum(top_left);
  d.setTextPadding(0);

  int y = 46;
  const int line = 21;

  d.setTextColor(d.color565(160, 160, 160), TFT_BLACK);
  d.drawString("Port A  SDA=G" + String(PIN_SDA) + "  SCL=G" + String(PIN_SCL), 10, y);
  y += line;
  d.drawString("board id " + String((int)M5.getBoard()) + "   " + String(I2C_FREQ / 1000) + " kHz",
               10, y);
  y += line + 6;

  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString("devices found: " + String(n), 10, y);
  y += line;
  d.setTextColor(d.color565(120, 200, 255), TFT_BLACK);
  d.drawString(n ? list : String("(none)"), 10, y);
  y += line + 8;

  // ここが切り分けの肝。0x75 は Core 内蔵の電源 IC なので、外部センサとは無関係に見えるはず。
  d.setTextColor(saw_pmu ? d.color565(0, 200, 80) : d.color565(240, 40, 40), TFT_BLACK);
  d.drawString(saw_pmu ? "0x75 onboard PMU: OK -> bus alive"
                       : "0x75 onboard PMU: MISSING -> bus dead",
               10, y);
  y += line;

  d.setTextColor(saw_sensor ? d.color565(0, 200, 80) : d.color565(240, 40, 40), TFT_BLACK);
  d.drawString(saw_sensor ? "0x23 SEN0636: OK" : "0x23 SEN0636: not responding", 10, y);
  y += line + 6;

  d.setTextColor(d.color565(150, 150, 150), TFT_BLACK);
  if (!saw_sensor) {
    d.drawString(saw_pmu ? "check cable / mode switch = I2C" : "Wire bus itself is down", 10, y);
  } else {
    d.drawString("BtnA: back to UV view", 10, y);
  }

  Serial.printf("[uv] scan: %u device(s)%s%s\n", (unsigned)n,
                saw_pmu ? ", PMU 0x75 present" : ", PMU 0x75 MISSING",
                saw_sensor ? ", sensor 0x23 present" : ", sensor 0x23 absent");
  for (size_t i = 0; i < n; ++i) {
    Serial.printf("[uv]   0x%02X\n", found[i]);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[uv] boot");

  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.printf("[uv] M5 ready, board=%d, display=%dx%d\n", (int)M5.getBoard(), M5.Display.width(),
                M5.Display.height());

  // Grove ポート A を I2C として開く。ライブラリの begin() も内部で Wire.begin() を
  // 引数なしで呼ぶが、m5stack-core-esp32 の既定 SDA/SCL がちょうど 21/22 なので衝突しない。
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);

  drawUvChrome();

  Serial.printf("[uv] probing SEN0636 on SDA=%d SCL=%d addr=0x%02X\n", PIN_SDA, PIN_SCL,
                SENSOR_ADDR);
  if (!g_sensor.begin()) {
    Serial.println("[uv] sensor not found - press BtnA for an I2C scan");
  } else {
    Serial.println("[uv] sensor ready");
  }
}

void loop() {
  M5.update();

  if (M5.BtnA.wasPressed()) {
    g_view = (g_view == VIEW_UV) ? VIEW_DIAG : VIEW_UV;
    if (g_view == VIEW_DIAG) {
      drawDiag();
    } else {
      drawUvChrome();
      g_needs_full_redraw = true;
    }
  }

  if (g_view == VIEW_DIAG) {
    delay(10);
    return;
  }

  static uint32_t next_sample = 0;
  const uint32_t now = millis();
  if ((int32_t)(now - next_sample) < 0) {
    delay(5);
    return;
  }
  next_sample = now + SAMPLE_INTERVAL_MS;

  const bool online = acks(SENSOR_ADDR);
  int index = 0;
  int level = 0;
  int mv = 0;

  if (online) {
    mv = (int)g_sensor.readUvOriginalData();
    index = (int)g_sensor.readUvIndexData();
    level = (int)g_sensor.readRiskLevelData();
    Serial.printf("[uv] %d mV, index %d, risk %d (%s)\n", mv, index, level, riskLabel(level));
  } else {
    Serial.println("[uv] no ACK from 0x23");
  }

  if (g_needs_full_redraw || online != g_last_online || index != g_last_index) {
    drawIndex(index, online);
  }
  if (g_needs_full_redraw || online != g_last_online || level != g_last_level) {
    drawRisk(level, online);
  }
  if (g_needs_full_redraw || online != g_last_online || mv != g_last_mv) {
    drawFooter(mv, online);
  }

  g_last_index = index;
  g_last_level = level;
  g_last_mv = mv;
  g_last_online = online;
  g_needs_full_redraw = false;
}
