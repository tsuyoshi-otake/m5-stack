# m5-stack — 作業メモ

M5Stack 系デバイスのモノレポ。PlatformIO 単一 `platformio.ini` 構成。
リポジトリの全体像・機種表・プロジェクト追加手順は [README.md](README.md) を見ること。

## ツールチェーン

- PlatformIO Core 6.1.13、`pio` は **PATH に無い**。必ず
  `C:\Users\developer\.platformio\penv\Scripts\pio.exe` をフルパスで呼ぶ。
  bash からは `"$HOME/.platformio/penv/Scripts/pio.exe"`。
- プラットフォームは `espressif32@7.0.1`（Arduino ESP32 core 3.x）。`platformio.ini` で
  バージョン固定済み。7.x は core 2.x と API 差があるので、ネット上の古い作例を貼るときは注意。
- ライブラリは `m5stack/M5Unified@^0.2.7`。機種差は M5Unified の実行時オートデテクトに
  任せる方針で、`ARDUINO_M5STACK_*` 系マクロは意図的に定義していない。
  Cardputer / Capsule で誤検出が出たら該当 `[device:*]` にボードマクロを足して切り分ける。

## ビルド

```bash
PIO="$HOME/.platformio/penv/Scripts/pio.exe"
cd /c/Codes/tsuyoshi-otake/m5-stack
$PIO run -e cardputer-hello                     # ビルド
$PIO run -e cardputer-hello -t upload -t monitor
$PIO device list                                # ポート確認
```

env 名・ソースのパスとも `<device>/<project>` の順で揃える規約。
機種固有は `projects/<device>/<name>/`、全機種共通は `projects/_common/<name>/`。

初回ビルドは toolchain-xtensa-esp32s3 等の DL が走って数分かかる。以後はキャッシュされる。
ESP32 と ESP32-S3 は別toolchainなので、`core` 系を初めて焼くときにもう一度DLが入る。

## この構成でハマりやすい点

- **`build_src_filter` 必須**。`[platformio] src_dir = projects` なので、フィルタが無い env は
  `projects/` 配下の全プロジェクトをコンパイルし `setup()`/`loop()` 多重定義で落ちる。
  新プロジェクトの `[project:*]` には必ず `build_src_filter = -<*> +<device/name/>` を書く。
- **`build_flags` は継承されない**。`[device:*]` で `build_flags` を定義すると `[env]` の
  同名キーを丸ごと上書きするので、各 device は `${env.build_flags}` を先頭に展開している。
  `lib_deps` を `[project:*]` で足すときも同様に `${env.lib_deps}` を書くこと。
- **Cardputer / Capsule / StickC Plus2 に専用 board ID は無い**。既存 board を上書き流用
  している（README の対応表参照）。flash_size / partitions の上書きを消すと 8MB 機で
  パーティションが足りず書き込みに失敗する。
- **`board_upload.maximum_size` は書かない**。app サイズの上限は `board_build.partitions` の
  csv から自動算出される。ここをフラッシュ全体（8388608 等）に手で書くと、app パーティション
  (default_8MB.csv なら 0x330000) を超えたバイナリがサイズチェックを素通りし、
  書き込み時に初めて失敗する。
- **`M5.begin()` は `Serial.begin()` を呼ばない**。setup の冒頭で明示的に呼ばないと
  シリアル出力が一切出ない。画面は正常に動くのでログだけ無言という紛らわしい状態になる。
- **`pio device monitor` には `-e <env>` を付ける**。省くと `default_envs` の firmware.elf を
  例外デコーダが掴んで `does not exist, rebuild the project?` と出る。
- **`pio` の出力が UnicodeEncodeError で落ちることがある**（`pkg show` 等）。
  `export PYTHONIOENCODING=utf-8` を付けて実行すれば通る。

## 書き込み

ESP32 系は BOOT ボタン不要で自動リセット書き込みが効く（USB-CDC or CP210x/CH9102）。
失敗する場合のみ、BOOT を押しながら RESET → BOOT 離す でダウンロードモードに入る。
StickC Plus2 は電源ボタン長押しで切れている場合があるので、書き込み前に電源を入れておく。

## スコープ外

`../arduino/` の Arduino IDE 用スケッチと `../test/ch552e` 等の CH552 プロジェクトは
このリポジトリの管理対象外。CH552 のビルド手順はグローバル CLAUDE.md 側にある。
