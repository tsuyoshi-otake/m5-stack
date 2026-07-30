# m5-stack

M5Stack 系デバイス（Cardputer / StickC Plus2 / Atom / Capsule / Core 系）のプロジェクトを
まとめて置くモノレポ。PlatformIO の単一 `platformio.ini` で全機種を管理する。

## 構成

```
m5-stack/
├── platformio.ini          # 全機種・全プロジェクトの定義（唯一の設定ファイル）
├── projects/
│   ├── _common/            # 全機種共通で焼けるもの
│   │   └── hello/main.cpp
│   ├── core/               # 以下、機種固有プロジェクトを機種ごとに置く
│   │   └── uv/             # DFRobot SEN0636 の UV インデックスを画面表示
│   ├── cardputer/
│   ├── capsule/
│   ├── stickc-plus2/
│   ├── atom-lite/
│   ├── atom-matrix/
│   ├── core2/
│   └── cores3/
├── lib/                    # プロジェクト横断の共有ライブラリ
└── CLAUDE.md               # Claude Code 向けの作業メモ
```

**命名規則**: env 名もソースのパスも `<device>/<project>` の順で揃える。

| ソース | project セクション | env |
|---|---|---|
| `projects/cardputer/totp/` | `[project:cardputer-totp]` | `cardputer-totp` |
| `projects/_common/hello/` | `[project:hello]` | `cardputer-hello`, `core2-hello`, … |

`platformio.ini` は 3 層に分かれている:

| セクション | 役割 |
|---|---|
| `[device:*]` | 機種ごとの board / flash / partitions / `DEVICE_NAME` |
| `[project:*]` | プロジェクトごとの `build_src_filter` と追加 `lib_deps` |
| `[env:*]` | 上 2 つを `extends` で掛け合わせたビルド対象 |

機種固有プロジェクトは env が 1 つ、共通プロジェクトは機種分の env を並べる、という使い分けになる。

## プロジェクト一覧

| env | 内容 |
|---|---|
| `<device>-hello` | 疎通確認。M5Unified で全機種共通のソース 1 本 |
| `core-uv` | DFRobot SEN0636 の UV インデックスを表示 → [projects/core/uv/](projects/core/uv/) |

配線や機種固有の注意点は各プロジェクトの README に書く。

## ビルド・書き込み

`pio` は PATH に入っていないのでフルパスで呼ぶ（`~/.platformio/penv/Scripts/pio.exe`）。

```bash
PIO="$HOME/.platformio/penv/Scripts/pio.exe"

$PIO run                                       # default_envs (cardputer-hello) をビルド
$PIO run -e atom-lite-hello                    # env 指定
$PIO run -e cardputer-hello -t upload          # 書き込み
$PIO run -e cardputer-hello -t upload -t monitor
$PIO run -t clean                              # 成果物削除
$PIO device list                               # シリアルポート確認
```

ポートを固定したいときは該当 `[device:*]` に `upload_port = COM5` を足す。

## 対象機種と board ID

espressif32 7.0.1 に Cardputer / Capsule / StickC Plus2 の専用 board ID は無いため、
実ハードに合わせて既存 board を上書きして使っている。

| device | board ID | 備考 |
|---|---|---|
| `cardputer` | `m5stack-stamps3` | Cardputer の中身は StampS3（ESP32-S3, 8MB） |
| `capsule` | `m5stack-stamps3` | Capsule も StampS3 ベース |
| `stickc-plus2` | `m5stick-c` | ESP32-PICO-V3-02。`m5stick-c` は 4MB 想定なので 8MB に上書き |
| `atom-lite` / `atom-matrix` | `m5stack-atom` | 両者ボードは同一、差は本体 LED のみ |
| `core` | `m5stack-core-esp32` | Basic 4MB。Gray/16M は `m5stack-grey` に差し替え |
| `core2` | `m5stack-core2` | |
| `cores3` | `m5stack-cores3` | |

## プロジェクトを追加する

### 機種固有のもの

1. `projects/<device>/<name>/` を作って `main.cpp` を置く
2. `platformio.ini` に追記:

```ini
[project:<device>-<name>]
build_src_filter = -<*> +<<device>/<name>/>
lib_deps = ${env.lib_deps}          ; 追加ライブラリがあればここに並べる

[env:<device>-<name>]
extends = device:<device>, project:<device>-<name>
```

### 全機種共通のもの

`projects/_common/<name>/` に置き、`build_src_filter = -<*> +<_common/<name>/>` にして
焼きたい機種分だけ `[env:<device>-<name>]` を並べる。

`build_src_filter` で自分のディレクトリだけを選ぶのが肝。これを書き忘れると
`projects/` 配下の全ソースがコンパイル対象になり `setup()` 重複で落ちる。

## 既存スケッチについて

`../arduino/` 配下の M5 スケッチ（Atom Lite LED、Atom Matrix INA219/SSD1306、
Capsule RTC）は Arduino IDE 用のまま残してある。このモノレポは新規プロジェクト用。
移行する場合は `.ino` を `projects/<device>/<name>/main.cpp` にリネームして
`#include <Arduino.h>` を先頭に足す（`.ino` の暗黙プロトタイプ生成が無いため、
関数は使用前に定義するか前方宣言が要る）。
