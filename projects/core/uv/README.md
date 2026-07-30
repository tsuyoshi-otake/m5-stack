# core/uv — SEN0636 UV インデックス表示

M5Stack Core に DFRobot [SEN0636](https://wiki.dfrobot.com/sen0636)
(Gravity: 240370 Ultraviolet Index Sensor) を I2C で繋ぎ、UV インデックスを画面に出す。

env: `core-uv`

## 必要なもの

- M5Stack Core（Basic / Gray）
- DFRobot SEN0636
- Grove(HY2.0-4P) ↔ Gravity(PH2.0) を **信号ごとに繋ぎ替えられる**ケーブル
  （M5Stack「Grove to 4Pin」変換 + DFRobot の Gravity 4Pin-DuPont ケーブルなど）

## 配線

**ストレートのケーブルで直結してはいけない。** 両者ともに 2.0mm ピッチ 4 ピンで
物理的には挿さるが、コネクタ上の信号の並びが違うため全ての線がズレる。

| ピン | M5Stack Core Port A | DFRobot SEN0636 |
|---|---|---|
| 1 | 黒 GND | 緑 D/R (SDA) |
| 2 | 赤 5V | 青 C/T (SCL) |
| 3 | 黄 G21 (SDA) | 黒 GND |
| 4 | 白 G22 (SCL) | 赤 VCC |

直結すると M5 の GND がセンサの SDA へ、M5 の 5V がセンサの SCL へ繋がり、
センサの VCC には SCL ラインしか来ないので**そもそも電源が入らない**。

正しい対応は位置ではなく信号で合わせる:

```
センサ (DFRobot)          M5Stack Core Port A
  緑  SDA (D/R)    →→→    黄  G21 (SDA)
  青  SCL (C/T)    →→→    白  G22 (SCL)
  赤  VCC          →→→    赤  5V
  黒  GND          →→→    黒  GND
```

色が食い違うのが罠。DFRobot は SDA=緑 / SCL=青、M5Stack は SDA=黄 / SCL=白 で、
**色を合わせると動かない**。赤と黒だけは両社一致している。

その他:

- 挿すのは**赤い Port A** のみ。Port B (G36/G26) と Port C (G16/G17) は I2C ではない
- センサ基板の **Communication Mode Switch を I2C 側**に。UART 側だと配線が正しくても無応答
- 電源は 5V で問題ない（センサは 3.3-5V 対応）
- I2C アドレスは **0x23 固定**で変更できない

## ビルド・書き込み

```bash
PIO="$HOME/.platformio/penv/Scripts/pio.exe"
$PIO run -e core-uv -t upload
$PIO run -e core-uv -t monitor      # -e は必須（後述）
```

## 画面

| 位置 | 内容 |
|---|---|
| 上部バー | `UV INDEX 240-370nm` |
| 中央 | UV インデックス（0-11）を 7 セグ風フォントで大きく |
| その下 | 危険度の色帯とラベル（LOW / MODERATE / HIGH / VERY HIGH / EXTREME） |
| 下部 | 生電圧 mV と I2C 接続状態 |

配色は WHO の UV インデックス区分に合わせてある（緑→黄→橙→赤→紫）。

**BtnA で I2C 診断画面に切り替わる。** バス上の全アドレスをスキャンして表示する。

## センサの値について

センサ側が UV インデックスと危険度を算出済みで返すので、こちら側での換算は不要。

| API | 戻り値 |
|---|---|
| `readUvOriginalData()` | UV 電圧 0-3300 mV |
| `readUvIndexData()` | UV インデックス 0-11 の整数 |
| `readRiskLevelData()` | 危険度 0-4 |

**室内では index 0 が正常。** 240-370nm を見ているセンサで、窓ガラスは UVB をほとんど
通さず室内照明の UV もほぼゼロなので 0 になる。屋外の直射日光か UV ライト(365nm)で上がる。

## トラブルシュート

画面に `NO SENSOR` が出るときは BtnA で診断画面を開く。判定の要は **0x75**
（Core 内蔵の電源 IC IP5306。外部センサとは無関係に同じ G21/G22 バス上にいる）。

| 診断画面の表示 | 意味 | 対処 |
|---|---|---|
| 0x75 OK / 0x23 なし | バスは生きている | センサ側の配線かモードスイッチ |
| 0x75 も無い | バス自体が死んでいる | Wire と M5Unified の I2C ドライバ競合を疑う。`M5.In_I2C.release()` を Wire.begin() の前に入れる |
| 両方 OK | 正常 | BtnA で UV 表示に戻る |

SDA と SCL の入れ替えは壊れる配線ではないので、迷ったら逆も試してよい。

## 動作確認済みログ

```
[uv] boot
[uv] M5 ready, board=1, display=320x240
[uv] probing SEN0636 on SDA=21 SCL=22 addr=0x23
[uv] sensor ready
[uv] 0 mV, index 0, risk 0 (LOW)     ← 室内なので 0
```

## 実装メモ

- `DFRobot_UVIndex240370Sensor` は PlatformIO レジストリに無いので git から直接取っている。
  同ライブラリは `DFRobot_RTU` を継承しているため、そちらも `lib_deps` に明示している
- ライブラリの `begin()` は内部で引数なしの `Wire.begin()` を呼ぶが、
  `m5stack-core-esp32` の既定 SDA/SCL がちょうど 21/22 なので衝突しない
- ライブラリの read 系はエラーを返さず、センサが抜けていても値を返してしまう。
  そのため読む前に 0x23 への ACK を確認して `NO SENSOR` 表示に落としている
- **`M5.begin()` は `Serial.begin()` を呼ばない。** 明示的に呼ばないとシリアル出力が
  一切出ず、画面は正常なのにログだけ無言という状態になる
- **`pio device monitor` には `-e core-uv` を付ける。** 省くと `default_envs` の
  firmware.elf を例外デコーダが掴んでエラーを出す
