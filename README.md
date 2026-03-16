# distsensor

UIAPduino(CH32V003系) + VL53L0X/VL53L1X + 160x80 ST7735 TFT 用の PlatformIO / ch32fun プロジェクトです。

## 配線

- `VL53L0X SDA` -> `PC1`
- `VL53L0X SCL` -> `PC2`
- `ST7735 SCK` -> `PC5`
- `ST7735 MOSI` -> `PC6`
- `ST7735 CS` -> `PC3`
- `ST7735 DC` -> `PD0`
- `ST7735 RST` -> `PC7`
- `ST7735 BLK` -> `3V3` (常時点灯)
- `3V3` / `GND` は各モジュールへ接続

`PD1` は `SWIO` なので使っていません。

## ビルド

```sh
pio run
```

## センサー切り替え（define）

- デフォルトは `platformio.ini` の `-DSENSOR_VL53L1X`
- `VL53L0X` を使う場合は `build_flags` を `-DSENSOR_VL53L0X` に変更
- どちらか1つだけ有効にしてください
- `SENSOR_VL53L1X` 時は ROI を高速スキャンし、16x16 ヒートマップ表示モードになります

## ヒートマップ解像度（VL53L1X）

- `-DHEATMAP_GRID_8` で 8x8 スキャン
- `-DHEATMAP_GRID_16` で 16x16 スキャン
- define 未指定時は 8x8
- 8x8 時は表示セルサイズを自動で拡大して、表示領域（64x64 px）を同じ大きさで使います

## 実装メモ

- `VL53L0X` は Pololu の Arduino ドライバを `Arduino.h` / `Wire.h` 互換層で移植
- `VL53L1X` も define 切り替えで使用可能（Pololu ドライバ）
- `Wire` 互換層は `I2C1` ハードウェアI2Cを使用（SDA=`PC1`, SCL=`PC2`）
- ST7735 ドライバは `cw_decoder3_for_uiap` の実装をそのまま使用
- ST7735 の塗りつぶし転送は SPI DMA (`DMA1_Channel3`) を使用
