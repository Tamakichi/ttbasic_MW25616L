//
// 豊四季Tiny BASIC for Arduino uno 構築コンフィグレーション
//

#ifndef __ttconfig_h__
#define __ttconfig_h__

#define MYDEBUG 0

/*
 * デフォルトの設定は Arduino UNO 用の設定になっています。
 * 「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」
 * で利用する場合は下記の変更を行って下さい。
 *   USE_CMD_VFD  0 ==> 1
 * また、ローカル変数領域不足により動作が不安定になる場合があります。
 * その場合は、プログラム領域サイズを512～1024の間で調整して下さい。
 */

// ** シリアルポート通信速度 *************************************************
#define SERIALBAUD 115200

// ** 利用ピン設定 **********************************************************
#ifdef ARDUINO_AVR_MEGA2560
  // Arduino MEGA2560
  #define   TonePin 49  // Tone用出力ピン（圧電スピーカー接続）
  #define   AutoPin 53  // 自動起動チェックピン
#else
  // Arduino Uno/nano/pro mini
  #define   TonePin 8  // Tone用出力ピン（圧電スピーカー接続）
  #define   AutoPin 7  // 自動起動チェックピン
#endif

// ** プログラム領域サイズ ***************************************************
#ifdef ARDUINO_AVR_MEGA2560
  // Arduino MEGA2560
  #define   PRGAREASIZE 2048 // プログラム領域サイズ(Arduino Mega 512 ～ 4096 デフォルト:2048)
  #define   ARRYSIZE    100  // 配列領域
#else
  // Arduino Uno/nano/pro mini
  #define   PRGAREASIZE 1024 // プログラム領域サイズ(Arduino Uno  512 ～ 1024 デフォルト:1024)
  #define   ARRYSIZE    16   // 配列領域
#endif
// ** 機能利用オプション設定 *************************************************
#define USE_CMD_PLAY   1  // PLAYコマンドの利用(0:利用しない 1:利用する デフォルト:1)
#define USE_CMD_I2C    1  // I2Cコマンドの利用(0:利用しない 1:利用する デフォルト:1)
#define USE_CMD_VFD    0  // VFDモジュールコマンドの利用(0:利用しない 1:利用する デフォルト:0)
#define USE_RTC_DS3231 1  // I2C接続RTC DS3231の利用(0:利用しない 1:利用する デフォルト:1)
#define USE_I2CEEPROM  1  // I2C EEPROM対応(0:利用しない 1:利用する デフォルト:1)
#define USE_SYSINFO    0  // SYSINFOコマンド(0:利用しない 1:利用する デフォルト:0)
#define USE_GRADE      1  // GRADE関数(0:利用しない 1:利用する デフォルト:1)
#define USE_DMP        1  // DMP$関数(0:利用しない 1:利用する デフォルト:1)
#define USE_IR         1  // IR関数(0:利用しない 1:利用する デフォルト:1)
#define USE_ANADEF     1  // アナログピン定数A0～A7orA15(0:利用しない 1:利用する デフォルト:1)
#define USE_SO1602AWWB 0  // 有機ELキャラクタディスプレイ SO1602AWWB(0:利用しない 1:利用する デフォルト:0)
#endif
