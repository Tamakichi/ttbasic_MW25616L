//
// 有機ＥＬキャラクタディスプレイモジュール 制御関数
// 最終更新日 2019/05/28 by たま吉さん
//

#include "SO1602AWWB.h"
#include <Wire.h>

#define DEVADR 0x3c  // デバイススレーブアドレス

// ディスプレイの初期化
uint8_t OLEDinit() {
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(0x01); // Clear Display (表示部をクリア）
  Wire.write(0x02); // Return Home (カーソル位置を先頭に移動）
  Wire.write(0x0c); // Send Display on command (表示ON、カーソル表示なし)
  Wire.write(0x01); // Clear Display (表示部をクリア、取りあえず説明書通りにもう一度クリア）  
  return Wire.endTransmission();
}

// コントラスト設定
uint8_t OLEDcontrast(int cont){
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(0x2a); // RE=1
  Wire.write(0x79); // SD=1
  Wire.write(0x81); // コントラストセット
  Wire.write(cont); // コントラスト値
  Wire.write(0x78); // SDを0に戻す
  Wire.write(0x28); // 2C=高文字　28=ノーマル
  return Wire.endTransmission();   
}

// カーソル移動
uint8_t OLEDPos(uint8_t x, uint8_t y) {
  uint8_t adr = 0x80+y*32+x;
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(adr);  // アドレス
  return Wire.endTransmission();
}

// 文字列の表示
uint8_t OLEDprint(uint8_t* text) {
  Wire.beginTransmission(DEVADR);
  Wire.write(0x40);                      // Set DDRAM Address
  Wire.write(text,strlen((char*)text));  // 表示データ
  return Wire.endTransmission();
}

// 文字の表示
uint8_t OLEDputch(uint8_t c) {
  Wire.beginTransmission(DEVADR);
  Wire.write(0x40);               // Set DDRAM Address
  Wire.write(c);                  // 表示データ
  return Wire.endTransmission();
}

// 表示のクリア
uint8_t OLEDcls() {
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(0x01); // Clear Display (表示部をクリア）
  Wire.write(0x02); // Return Home (カーソル位置を先頭に移動）  
  Wire.write(0x01); // Clear Display (表示部をクリア）
  return Wire.endTransmission();   
}

// 表示のON/OFF指定
uint8_t OLEDisplay(uint8_t sw) {
  uint8_t mode = 0b1000 | ((sw?1:0)<<2);
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(mode); // Display ON/OFF
  return Wire.endTransmission();     
}

// カーソル表示指定
uint8_t OLEDCurs(uint8_t sw) {
  uint8_t mode = 0b1100 | (sw?3:0);
  Wire.beginTransmission(DEVADR);
  Wire.write(0x00); // データ送信
  Wire.write(mode); // Display ON/OFF
  return Wire.endTransmission();       
}
