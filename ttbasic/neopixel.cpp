//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// NeoPixel関連
// 作成 2019/06/26 by たま吉さん 
// 修正 2019/08/31 未初期化エラーチェックの追加、ピクセル番号指定チェックの追加

#include "Arduino.h"
#include "basic.h"

// NeoPixelの利用
#if USE_NEOPIXEL == 1
  #include "src/lib/NeoPixel.h"
  NeoPixel np;
#endif
#if USE_MISAKIFONT != 0
  #include "src/lib/misakiSJIS500.h"
#endif

#if USE_NEOPIXEL == 1
// NeoPixel初期化設定
// NINIT バッファアドレス,ピクセル数(1～64)
void ininit() {
  int16_t  vadr;  // バッファアドレス
  int16_t  num;   // ピクセル数
  uint8_t* ptr;   // バッファ実アドレス

  // 引数の取得
  if (getParam(vadr,0, 32767, true) ||
      getParam(num, 1, 64,false)
  )  return;

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

// NeoPixel初期化チェック
uint8_t chkNinit() {
  if (!np.getPixelNum()) {
    // 未初期化
    err = ERR_NINIT;
  }
  return !np.getPixelNum();
}

// NeoPixelバッファ反映
// NUPDATE
void inupdate() {
  if (chkNinit()) return;   
  np.update();
}

// NeoPixel輝度設定
// NBRIGHT 輝度(0～5)[,更新flg]
void inbright() {
  int16_t level;
  int16_t flg = 1;  
  
  if (chkNinit()) return;   
  
  if (getParam(level, 0, 5, false))  return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }  
  np.setBrightness(level);
  if (flg)
    np.update();
}

// NeoPixe表示クリア
// NCLS [更新flg]
void incls() {
  int16_t flg = 1;
  
  if (chkNinit()) return;   
  
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

// NeoPixel 8x8マトリックス 指定座標色コード取得
// NPOINT(X,Y)
// x,yが有効範囲内 0 ~ 255、指定範囲外 -1
int16_t inpoint() {
  int16_t x,y;
  uint16_t rc = -1;

  if (chkNinit()) return;   
  
  if (checkOpen() ||
      getParam(x,true) || getParam(y,false) ||
      checkClose()
  ) return 0;

  if (x>=0 && x < 8 && y>=0 && y < 8) 
    rc = (np.getBuffer())[np.XYtoNo(x,y)];
  return  rc;
}

// NeoPixel LED色設定
// NSET 番号,色[,更新flg]
void inset() {
  int16_t no;
  int16_t color;
  int16_t flg = 1;
  
  if (chkNinit()) return;   
  if (getParam(no, 0, np.getPixelNum()-1, true) ||
      getParam(color,0,255,false)
  ) return;
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
  int16_t color;
  int16_t flg = 1;

  if (chkNinit()) return;   
  if (getParam(x, 0, 7, true) ||
      getParam(y, 0, 7, true) ||
      getParam(color,0,255,false)
   ) return;
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

  if (chkNinit()) return;   
  if (getParam(dir, 0, 1, false)) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }
  np.shiftPixel(dir, flg);
}
#endif

#if (USE_NEOPIXEL == 1) && (USE_MISAKIFONT != 0)
// メッセージの表示
void nmsg(const char* msg, uint16_t color, uint16_t tm) {
  uint8_t  fnt[8];
  char *str = (char*)msg;
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
  int16_t color;

  if (chkNinit()) return;     
  if ( getParam(color, 0,255, true) ||
       getParam(tm, 0, 5000, true)       
  ) return;
  
  // メッセージ部をバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,1);
  if (err)
    return;

  // メッセージ部の表示
  nmsg(lbuf,color,tm);
}
#endif

#if USE_NEOPIXEL == 1
// 直線を引く
inline int16_t _v_sgn(int16_t val) { return (0 < val) - (val < 0);}
void drawline(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t color){
   int16_t dx=abs(x1-x0), dy=abs(y1-y0),sx=_v_sgn(x1-x0),sy=_v_sgn(y1-y0);
   int16_t err=dx-dy; 
   if((x0!=x1)||(y0!=y1)) 
       np.setPixel(x1,y1,color, 0);
   do{
       np.setPixel(x0,y0,color, 0);
       int16_t e2=2*err;
       if (e2 > -dy){err-=dy;x0+=sx;}
       if (e2 <  dx){err+=dx;y0+=sy;}
   }   while ((x0!=x1)||(y0!=y1));
}

// NeoPixel 直線描画
// NLINE X1,Y1,X2,Y2,色[,モード[,更新flg]]
void inLine() {
  int16_t x1, y1, x2, y2, color, mode = 0;
  int16_t flg = 1;

  if (chkNinit()) return;   
  if ( getParam(x1, 0, 7, true) ||
       getParam(y1, 0, 7, true) ||
       getParam(x2, 0, 7, true) ||
       getParam(y2, 0, 7, true) ||
       getParam(color, 0, 255, false)
  ) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(mode, 0, 2, false)) return;
    if(*cip == I_COMMA) {
       cip++;
       if ( getParam(flg, 0, 1, false)) return;
    } 
  }
  if(mode == 0) {
    // 直線
    drawline(x1,y1,x2,y2,color);
  } else if (mode == 1) {
    // 矩形
    drawline(x1,y1,x2,y1,color);
    drawline(x1,y1,x1,y2,color);
    drawline(x1,y2,x2,y2,color);
    drawline(x2,y2,x2,y1,color);
  } else {
    // 矩形塗りつぶし
    int16_t w,h,x,y;
    w = abs(x1-x2); h = abs(y1-y2);
    if (x1>=x2) x=x2; else x=x1;
    if (y1>=y2) y=y2; else y=y1;    
    for (int16_t i=0; i<=h; i++)
      drawline(x,y+i,x+w,y+i,color);
  }
  
  if(flg)
    np.update();
}

// NeoPixel スクロール
// NSCROLL 方向[,更新flg]
void inscroll() {
  int16_t dir;
  int16_t flg = 1;

  if (chkNinit()) return;   
//  if (getParam(dir, 0, 3, false)) return;
  if (getParam(dir,false)) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(flg, 0, 1, false) ) return;
  }
  if (dir == 28) dir = 2; // Up
  if (dir == 29) dir = 3; // down
  if (dir == 30) dir = 1; // Right
  if (dir == 31) dir = 0; // left  
  if (dir <0 || dir >3) {
      err = ERR_VALUE;
      return;
  }
  np.scroll(dir, flg);
}
#endif
