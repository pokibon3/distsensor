# distsensor

UIAPduino(CH32V003系) + VL53L0X/VL53L1X + 160x80 ST7735 TFT 用の PlatformIO / ch32fun プロジェクトです。

## 配線

- `VL53L0X/VL53L1X SDA` -> `PC1`
- `VL53L0X/VL53L1X SCL` -> `PC2`
- `ST7735 SCK` -> `PC5`
- `ST7735 MOSI` -> `PC6`
- `ST7735 CS` -> `PC3`
- `ST7735 DC` -> `PD0`
- `ST7735 RST` -> `PC7`
- `ST7735 BLK` -> `3V3` (常時点灯)
- `3V3` / `GND` は各モジュールへ接続

`PD1` は `SWIO` なので使っていません。

### SW1 (MODE)

- `SW1` は `PA1` を使用（内部プルアップ入力）
- 押下ごとに ROI を `16x16 -> 8x8 -> 4x4 -> 1x1` で切り替え
- デバウンスはソフトウェアで約30ms

## ビルド

```sh
pio run
```

書き込み:

```sh
pio run -t upload
```

## センサー切り替え（define）

- デフォルトは `platformio.ini` の `-DSENSOR_VL53L1X`
- `VL53L0X` を使う場合は `build_flags` を `-DSENSOR_VL53L0X` に変更
- どちらか1つだけ有効にしてください
- `SENSOR_VL53L1X` 時は ROI 走査ヒートマップ表示
- `SENSOR_VL53L0X` 時は単点距離ゲージ表示

## VL53L1X 動作概要

- ROIモード: `16x16 -> 8x8 -> 4x4 -> 1x1` を `SW1(MODE)` で循環切替
- 表示グリッドは ROI に追従
- 現在走査セルは白枠でハイライト
- 1スキャン面の最小距離を上段に表示
- 下段に現在セルの距離 (`CUR:xxxx`) をリアルタイム表示
- ヒートマップは左右反転表示（見た目補正）

## 現在の主要設定（VL53L1X）

- `DistanceMode`: `Short`
- `MeasurementTimingBudget`: `20000us`
- `Timeout`: `500ms`
- `I2C Clock`: `400kHz` (`Wire.setClock(400000)`)
- 起動時ウォームアップ: 単発計測を最大4回試行

## コード構成

- `src/main.cpp`  
  エントリポイント（`appInit()` / `appLoopStep()` 呼び出しのみ）
- `src/sensor_app.cpp`, `src/sensor_app.h`  
  センサ制御、ROI走査、状態更新
- `src/display_ui.cpp`, `src/display_ui.h`  
  ST7735描画処理

## 実装メモ / 注意点

- `VL53L0X` は Pololu の Arduino ドライバを `Arduino.h` / `Wire.h` 互換層で移植
- `VL53L1X` も define 切り替えで使用可能（Pololu ドライバ）
- `Wire` 互換層は `I2C1` ハードウェアI2Cを使用（SDA=`PC1`, SCL=`PC2`）
- ST7735 ドライバは `cw_decoder3_for_uiap` の実装をそのまま使用
- ST7735 の塗りつぶし転送は SPI DMA (`DMA1_Channel3`) を使用
- `CPU RESET` のみではセンサ状態が残る場合があり、まれに `SENSOR ERROR` が発生します（電源再投入で復帰）

## 既知の制約

- VL53L1X はイメージセンサではないため、近距離小物体の形状識別は困難
- `SENSOR ERROR` が連発する場合は配線・電源・I2C波形（特に起動直後）を確認してください
