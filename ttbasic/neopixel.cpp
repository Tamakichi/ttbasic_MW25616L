//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// NeoPixel関連
// 2019/06/26 by たま吉さん 
//

#include "Arduino.h"
#include "basic.h"

// NeoPixelの利用
#if USE_NEOPIXEL == 1
  #include "src/lib/NeoPixel.h"
  NeoPixel np;
#endif
#if USE_MISAKIFONT == 1
  #include "src/lib/misakiSJIS500.h"
#endif

// NeoPixel初期化設定
// NINIT バッファアドレス,ピクセル数(1～64)
void ininit() {
  int16_t  vadr;  // バッファアドレス
  int16_t  num;   // ピクセル数
  uint8_t* ptr;   // バッファ実アドレス
  int16_t  rc;  

  // 引数の取得
  if (getParam(vadr,0, 32767, true)) return;
  if (getParam(num, 1, 64,false))  return;

  // バッファ実アドレスの取得
  ptr  = v2realAddr(vadr);
  if (ptr == 0 || v2realAddr(vadr+num-1) == 0)  {
    err = ERR_VALUE;
    return;
  }  

  // NeoPixelの設定
  np.setBuffer(ptr, num);
  np.init();
  np.cls();
}

// NeoPixelバッファ反映
// NUPDATE
void inupdate() {
  np.update();
}

// NeoPixel輝度設定
// NBRIGHT 輝度(0～5)
void inbright() {
  int16_t level;
  if (getParam(level, 0, 5, false))  return;
  np.setBrightness(level);
}

// NeoPixe表示クリア
// NCLS [更新flg]
void incls() {
  int16_t flg = 1;

  // 更新flg
  if (*cip != I_EOL && *cip != I_COLON)
    if (getParam(flg,0,1,false)) return;

  np.cls(flg);
}

// NeoPixel 8ビット色コード取得
// RGB(赤,青,緑)
int16_t iRGB() {
  int16_t R,G,B;
  uint16_t rc;

  if (checkOpen() ||
      getParam(R, 0, 7, true) ||
      getParam(G, 0, 7, true) ||
      getParam(B, 0, 3, false) ||
      checkClose()
  ) return 0;

  rc = ((uint16_t)G)<<5 | ((uint16_t)R)<<2 | B;
  return (int16_t)rc;
}

// NeoPixel LED色設定
// NSET 番号,色[,更新flg]
void inset() {
  int16_t no;
  uint16_t color;
  int16_t flg = 1;

  if (getParam(no, 0, 64, true)) return;
  if (getParam(color,false)) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }
  np.setRGB(no,color,flg);
}

// NeoPixel 8x8マトリックスの指定位置にピクセル設定
// NPSET X,Y,色[,更新flg]
void inpset() {
  int16_t x,y;
  uint16_t color;
  int16_t flg = 1;

  if (getParam(x, 0, 7, true)) return;
  if (getParam(y, 0, 7, true)) return;
  if (getParam(color,false)) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }
  np.setPixel(x,y,color,flg);
}

// NeoPixel LEDの表示をシフト
// NSHIFT 方向[,更新flg]
void inshift() {
  int16_t dir;
  int16_t flg = 1;

  if (getParam(dir, 0, 1, false)) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }
  np.shiftPixel(dir, flg);
}

// メッセージの表示
void nmsg(const char* msg, uint16_t color, uint16_t tm) {
  uint8_t  fnt[8];
  char *str = (char*)msg;
  np.cls();
  while(*str) {
    if (! (str = getFontData(fnt, str)) )  {
         break;
    }
    np.scrollInChar(fnt, color, tm);         
  }
}

// NeoPixel メッセージ表示
// NMSG 色,速度,メッセージ文
void inmsg() {
  int16_t tm;
  uint16_t color;
  
  if ( getParam(tm, 0, 1024, true)) return;
  if ( getParam(color, true)) return;
  
  // メッセージ部をバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,1);
  if (err)
    return;

  // メッセージ部の表示
  nmsg(lbuf,color,tm);
}