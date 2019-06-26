//
// NeoPixel制御ライブラリ（256色バージョン）
//

#include <SPI.h>
#include "NeoPixel.h"

#define PIN         11           // Neopixel 制御用ピン番号
#define NEOSPI_0    0b11100000   // 1ビット 値0
#define NEOSPI_1    0b11111000   // 1ビット 値1
#define NEOSPI_RST  0b00000000   // REST

//
// LED１ピクセルあたり8ビットの色情報を持つ
//  8ビット：GGGRRRBB
// 別途輝度情報 0 ～ 5 をもつ
// NeoPixel１ピクセル24ビット色のRGB各8ビットは、輝度情報をもとに次ぎの計算で生成する
//   G = ((0b11100000&C)>>5)<<brightness
//   R = ((0b00011100&C)>>2)<<brightness
//   B = (0b00000011&C)<<(brightness+1)
//

//***************
// 関数
//***************

// フレームバッファの登録
// 引数
//  ptr : フレームバッファ先頭アドレス
//  sz  : フレームバッファサイズ（バイト）
//
void NeoPixel::setBuffer(uint8_t* ptr,uint8_t sz) {
  buf = ptr;
  n   = sz;
  memset(buf, 0, sz); // バッファの初期化
};

// Neopixel初期化
void NeoPixel::init() {
  // SPIの初期化
  SPI.setBitOrder(MSBFIRST);            // 最上位ビットから送信
  SPI.setClockDivider(SPI_CLOCK_DIV2);  // クロック 8MHz
  SPI.setDataMode(SPI_MODE1);           // アイドル時 LOW、立上りエッジ時送信
  SPI.begin();                          // 開始
}

// Neopixelへのデータ送信
void NeoPixel::update() {
  uint8_t d[3];                         // LED1個分24ビットデータ

  // RESET送信
  SPDR = NEOSPI_RST;                    // SPIデータ送信
  while(!(SPSR & (1 << SPIF))) ;        // 送信完了待ち
  delayMicroseconds(50);

  // ピクセル数分送信
  for (uint8_t i = 0; i < n; i++) {
    // 1ピクセル8ビット色から24ビット色に変換
    d[0] = ((0b11100000&buf[i])>>5)<<brightness;  // G
    d[1] = ((0b00011100&buf[i])>>2)<<brightness;  // R
    d[2] = (0b00000011&buf[i])<<(brightness+1);   // B
    
    // 3バイト分（24ビット）送信
    for (uint8_t k = 0; k < 3; k++) {
      // 1バイトデータ送信
      for (uint8_t j = 0; j < 8; j++) {
        SPDR = d[k] & (0x80>>j) ? NEOSPI_1:NEOSPI_0; // SPIデータ送信
        while(!(SPSR & (1 << SPIF))) ;                 // 送信完了待ち
      }
    }
  }
}

// Neopixelの表示クリア
void NeoPixel::cls(uint8_t flgUpdate) {
    memset(buf, 0, n); // バッファの初期化
  if (flgUpdate)
    update();
}
/*
// 指定したピクセルの色を設定
void NeoPixel::setRGB(uint8_t no, uint8_t R, uint8_t G, uint8_t B, uint8_t flgUpdate) {
  if (no < n) {
    buf[no] = color8(R,G,B);
  }
  if (flgUpdate)
    update();
}
*/
void NeoPixel::setRGB(uint8_t no, uint16_t color, uint8_t flgUpdate) {
  if (no < n) {
    buf[no] = color;
  }
  if (flgUpdate)
    update();
}


// 8x8ドットマトリックス用指定座標にピクセル設定
void NeoPixel::setPixel(uint8_t x, uint8_t y,uint8_t color, uint8_t flgUpdate) {
/*
  uint8_t R,G,B;
  G = (0b11100000 & color)>>5;
  R = (0b00011100 & color)>>2;
  B = 0b00000011 & color;
  //setRGB(XYtoNo(x,y), R,G,B, flgUpdate);
*/
  setRGB(XYtoNo(x,y), color, flgUpdate);
}
  
// ピクセルのシフト
void NeoPixel::shiftPixel(uint8_t dir, uint8_t flgUpdate) {
  uint8_t tmpbuf;
  if (dir) {
     tmpbuf = buf[0];
     memmove(buf, buf+1, n-1);
     buf[n-1] = tmpbuf;
  } else {
     tmpbuf = buf[n-1];
     memmove(buf+1, buf, n-1);
     buf[0] = tmpbuf;
  }
  if (flgUpdate)
    update();
}

// ドットマトリックス 左スクロール
void NeoPixel::scroll(uint8_t flgUpdate) {
  for (uint8_t i=0; i < 8; i++) {
    if ( i&1 ) {
      memmove(&buf[i*8], &buf[i*8+1],7);
      //memset(&buf[i*8+7], 0, 1);
      buf[i*8+7] = 0;
    } else {
      memmove(&buf[i*8+1], &buf[i*8],7);
      //memset(&buf[i*8], 0, 1);
      buf[i*8] = 0;
    }
  }
  if (flgUpdate)
    update();
}

// 1文字左スクロール挿入
void NeoPixel::scrollInChar(uint8_t *fnt, uint16_t color, uint16_t tm) {
  for (uint8_t i = 0; i < 8; i++) {
    // 左1ドットスクロール
    scroll(false);

    // フォントパターン1列分のセット
    for (uint8_t j = 0; j < 8; j++) {
      if (fnt[j] & (0x80 >> i)) {
         setRGB(XYtoNo(7,j), color, false);
      } else {
         setRGB(XYtoNo(7,j), 0, false);        
      }
    }
    update();
    delay(tm);
  }
}

