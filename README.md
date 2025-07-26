# NERD-HPC

## アップデート方法
1. 本体とPCをUSBで接続する。
2. BOOTを押しながらPowerをONにする。もしくはPowerをONにした後にBOOTを押しながらRESETボタンを押す。
3. パソコンにRPI-RP2として認識されるのでsoftware/_build/src/NERD_HPC.uf2をドラッグ&ドロップする。

## ファームウェアのビルド方法
```
(Ubuntu Desktop 24.04.1 LTS)
sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```
```
git clone --recursive https://github.com/TTtensan/NERD-HPC.git
cd NERD-HPC/software/_build
cmake ..
make
```

## その他リンク
- 本家豊四季タイニーBASIC(Arduino版)。製作者様に本オープンソースプロジェクトについて連絡済みです。GPLのバージョンはGPLv3とのこと。<br>[ttbasic\_arduino](https://github.com/vintagechips/ttbasic_arduino)

- 2進数、16進数の処理はこちらを参考にしました。大変参考になりました。<br>[ttbasic\_arduino\_stm32](https://github.com/Tamakichi/ttbasic_arduino_stm32)

- フォントデータ。こちらをベースに一部変更しています。<br>[ＡＱＭ１２４８Ａ グラフィックＬＣＤ (for Arduino)](https://hatakekara.com/aqm1248a/)
