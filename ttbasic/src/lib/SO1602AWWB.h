//
// 有機ＥＬキャラクタディスプレイモジュール 制御関数
//

#ifndef __SO1602AWWB_H_
#define __SO1602AWWB_H_

#include <Arduino.h>

uint8_t OLEDinit();                     // ディスプレイの初期化
uint8_t OLEDcontrast(int cont);         // コントラスト設定
uint8_t OLEDPos(uint8_t x, uint8_t y);  // カーソル移動
uint8_t OLEDprint(uint8_t* text);       // 文字列の表示
uint8_t OLEDputch(uint8_t c);           // 文字の表示
uint8_t OLEDcls();                      // 表示のクリア
uint8_t OLEDisplay(uint8_t sw);         // 表示のON/OFF指定
uint8_t OLEDCurs(uint8_t sw);           // カーソル表示指定
uint8_t OLEnewLine();                   // 改行
#endif

