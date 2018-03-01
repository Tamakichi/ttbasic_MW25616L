//
// 豊四季Tiny BASIC for Arduino uno 構築コンフィグレーション
//

#ifndef __ttconfig_h__
#define __ttconfig_h__

#define MYDEBUG 0

// ** ターミナルモード時のデフォルト スクリーンサイズ  *************************
// ※ 可動中では、WIDTHコマンドで変更可能  (デフォルト:40x8)
#define SC_CW    48  // スクリーン横文字数
#define SC_CH    8   // スクリーン縦文字数

// ** 利用ピン設定 **********************************************************
#define   TonePin 8  // Tone用出力ピン（圧電スピーカー接続）
#define   AutoPin 7  // 自動起動チェックピン

// ** 機能利用オプション設定 *************************************************
#define USE_CMD_PLAY   1  // PLAYコマンドの利用(0:利用しない 1:利用する デフォルト:1)
#define USE_CMD_I2C    1  // I2Cコマンドの利用(0:利用しない 1:利用する デフォルト:1)
#define USE_CMD_VFD    1  // VFDモジュールコマンドの利用(0:利用しない 1:利用する デフォルト:1)
#define USE_SCREEN     0  // スクリーン制御の利用(0:利用しない 1:利用する デフォルト:0)
#define USE_RTC_DS3231 1  // I2C接続RTC DS3231の利用(0:利用しない 1:利用する デフォルト:1)
#define USE_I2CEPPROM  1  // I2C EPPROM対応(0:利用しない 1:利用する デフォルト:1)
#define USE_SYSINFO    0  // SYSINFOコマンド(0:利用しない 1:利用する デフォルト:0)

#endif

