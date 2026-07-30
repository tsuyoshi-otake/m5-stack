// hello — 全機種共通の疎通確認スケッチ。
// M5Unified が機種差を吸収するので、ソースは 1 本で env だけ切り替えて焼ける。
// ディスプレイの無い機種（Atom 系）では M5.Display は no-op になるため分岐不要。

#include <M5Unified.h>

#ifndef DEVICE_NAME
#define DEVICE_NAME "unknown"
#endif

static uint32_t press_count = 0;

static void draw() {
  M5.Display.startWrite();
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setCursor(0, 0);
  M5.Display.setTextColor(TFT_GREEN);
  M5.Display.printf("%s\n", DEVICE_NAME);
  M5.Display.setTextColor(TFT_WHITE);
  M5.Display.printf("%ux%u\n", M5.Display.width(), M5.Display.height());
  M5.Display.printf("BtnA: %u\n", press_count);
  M5.Display.endWrite();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setTextSize(M5.Display.width() > 200 ? 2 : 1);
  Serial.printf("[hello] %s up, board_t=%d, display=%ux%u\n", DEVICE_NAME,
                (int)M5.getBoard(), M5.Display.width(), M5.Display.height());
  draw();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) {
    ++press_count;
    Serial.printf("[hello] BtnA pressed (%u)\n", press_count);
    draw();
  }
  delay(10);
}
