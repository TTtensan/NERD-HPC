# [NERD-HPC](https://github.com/TTtensan/NERD-HPC)

## アップデート方法
1. 本体とPCをUSBで接続する。
2. BOOTを押しながらPowerをONにする。もしくはPowerをONにした後にBOOTを押しながらRESETボタンを押す。
3. パソコンにRPI-RP2として認識されるのでsoftware/build/src/NERD_HPC.uf2をドラッグ&ドロップする。

## ファームウェアのビルド方法
どちらの方法でも、ビルド成果物は`software/build/src/NERD_HPC.uf2`に出力されます。

### コマンドラインでビルドする場合
```
(Ubuntu Desktop 24.04.1 LTS)
sudo apt install cmake python3 build-essential gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```
```
git clone --recursive https://github.com/TTtensan/NERD-HPC.git
cd NERD-HPC/software
cmake -B build
cd build
make
```
この方法では、リポジトリに同梱されている`software/libraries/pico-sdk`(サブモジュール)を使ってビルドします。`--recursive`を忘れずに。

### VS Codeでビルドする場合
[Raspberry Pi Pico拡張機能](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)を使います。

1. リポジトリをクローンし、`software`フォルダをVS Codeで開く。
2. 推奨拡張機能のインストールを促されるので、インストールする(`.vscode/extensions.json`に定義しています)。
3. Pico拡張機能が必要なツールチェーンを`~/.pico-sdk`以下に自動でダウンロードします。使用するバージョンは`software/CMakeLists.txt`の先頭で指定しています(Pico SDK 2.2.0 / GCC 14_2_Rel1 / picotool 2.2.0-a4)。
4. サイドバーのPicoアイコンから`Compile Project`を実行する。

`.vscode`以下の設定はリポジトリで共有しています。パスはすべて`${userHome}`などの変数で書いてあるため、Windows・macOS・Linuxのいずれでもそのまま動作します。

なお、この方法ではコマンドラインの場合と違い、同梱のサブモジュールではなく`~/.pico-sdk`にダウンロードされたSDKが使われます。

## その他リンク
- 本家豊四季タイニーBASIC(Arduino版)。素晴らしいプログラムをオープンソースライセンスで公開いただいていること、大変感謝しています。製作者様に本オープンソースプロジェクトについて連絡済みです。GPLのバージョンはGPLv3とのこと。<br>[ttbasic\_arduino](https://github.com/vintagechips/ttbasic_arduino)

- CHR、2進数、16進数の処理はこちらを参考にしました。大変参考になりました。<br>[ttbasic\_arduino\_stm32](https://github.com/Tamakichi/ttbasic_arduino_stm32)

- フォントデータ。こちらをベースに一部変更しています。これによって少ないデータでフォントデータを実装することができました。ありがとうございます。<br>[ＡＱＭ１２４８Ａ グラフィックＬＣＤ (for Arduino)](https://hatakekara.com/aqm1248a/)
