# このレポジトリについて

- M5StackCore2 x LittleVGL x Arduino x PlatformIOのテンプレート

- 動作確認済バージョン

  - M5StackCore2
  - M5Core2サポートライブラリ v0.1.5
  - M5Stack GPS
  - M5Stack ENV. IV
  - LVGL v8.3

- M5Stack Core2について

  - https://m5stack.com/
  - https://www.switch-science.com/collections/m5stack

- M5Stack GPS

  - https://docs.m5stack.com/ja/unit/gps

- M5Stack ENV. IV

  - https://docs.m5stack.com/ja/unit/ENV%E2%85%A3%20Unit

- LVGLについて

  - https://lvgl.io/

- MPlusフォントについて

  - https://mplusfonts.github.io/

# ファイルについての説明

- このレポジトリについて

  - このレポジトリはクローン後、リネームして利用すること
  - VSCodeにPlatformIOプラグインをインストールして使用すること

- src/main.cpp

  - 本ファイルはsetup関数を書き換えて使用すること
  - 他の関数を置き換える場合は慎重に実施すること

- lv_port_fs_sd.(c | h)pp

  - M5StackCore2に搭載されているSDカードスロットにLVGL FileSystemからアクセスできるようにするためのドライバ

- lv_conf.h

  - 本ファイルは.pio/libdeps/m5stack-core-esp32/lvglフォルダに移動すること

- fonts/mplus1_STYLE/mplus1_STYLE_POINT.c

  - 日本語表示が必要な場合のみ下記の手順で利用すること
  - 本ファイルは.pio/libdeps/m5stack-core-esp32/lvgl/src/fontフォルダに移動すること
  - 末尾の数字はフォントのポイント数。必要なフォントのみ移動すること

- fonts/fonts.mk

  - 日本語表示が必要な場合のみ下記の手順で利用すること
  - 含まれている文字はこちらを参照 : https://gist.github.com/masahirosuzuka/7814737cb875b7ae0840cbf117de97da
  - 本ファイルは.pio/libdeps/m5stack-core-esp32/lvgl/src/fontフォルダの同名ファイルと入れ替えること
  - 末尾にmplus1_STYLE_POINT.cをSRCSに追記している。手作業で追記する場合は入れ替えは不要
  - 例）Lightスタイルの14ポイントを追加する場合は
  
  1. 下記を.pio/libdeps/m5stack-core-esp32/lvgl/src/font/fonts.mkの32行目付近に追記

  ```makefile
  CSRCS += mplus1_light_14.c
  ```

  2. 下記を.pio/libdeps/m5stack-core-esp32/lvgl/src/font/fonts.hの246行目付近に追加
  
  ```c
  LV_FONT_DECLARE(mplus1_light_14)
  ```