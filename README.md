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
