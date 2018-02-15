#ifndef _MW252616L_H_
#define _MW252616L_H_

//#include <Adafruit_GFX.h>
#include <Arduino.h>
#include "FontGT20L16J1Y.h"

#define MW25616L_BUFSIZE 512 // フレームバッファサイズ
#define MW25616L_WIDTH   256 // 横ドット数
#define MW25616L_HIGHT   16 //  縦ドット数

 //ピン配置
#define mw_si    A2    // SI
#define mw_ck    A1    // CLK
#define mw_lat   A0    // LAT
#define mw_en    3     // EN
#define mw_pwm   9     // 電源モータードライバ入力 

//class MW25616L : public Adafruit_GFX {
class MW25616L {
 private:
/*
   uint8_t* fbuf;        // フレームバッファポインタ
   uint8_t flgextbuf;    // 外部獲得バッファ利用フラグ
   unsigned char matrixdata32[32];   //16×16用表示データ
   unsigned char matrixdata16[16];   //16×8用表示データ
*/
   FontGT20L16J1Y FROM; // フォントROMドライバ

   uint8_t mw_bright  = 255;  // 明るさ
   uint8_t mw_display = HIGH; // 表示
 public:
   //MW25616L(uint8_t* framebuffer=NULL);
   MW25616L(void) { };
   ~MW25616L(void);
   uint8_t begin(void);
#if 0
   void drawPixel(int16_t x, int16_t y, uint16_t color); // 点の描画
   uint16_t getPixel(int16_t x, int16_t y);              // 指定座標のドット色の取得
   void fillScreen(uint16_t color);                      // 全画面塗りつぶし
   void update();                                        // バッファ内容を表示に反映
   void mw_claerbuf();

#endif
   void mw_put(unsigned char onebyte);   // 1byteデータ書き込み
   void mw_clr();                        // 画面クリア      
   void wr_lat();                        // LATパルス

   void setBright(uint8_t level);        // 輝度の設定
   void display(uint8_t sw);             // 表示設定

   void mw_put_t(uint8_t* tablename,int dn);                  // テーブルデータの連続書き込み
   void mw_put_s(uint8_t* tablename,int dn,int stime);        // テーブルデータのスクロール表示
   void disp_non(int n);                                      // 空白表示（n:横ドット数）
   void disp_non_t(int n, int t);                             // 空白表示スクロール（n:横ドット数, t:停止時間）
 
   void dispFont16x16(uint8_t* matrixdata32);                 // VFDに16x16フォントを表示（1文字分のデータ送信のみ/LATなし）
   void dispFont16x16d2(uint8_t* matrixdata32);               // VFDに16x16フォントを表示（1文字分のデータ送信のみ/LATなし）
   void dispFont16x16s(uint8_t* matrixdata32,int t);          // VFDに16x16フォントを表示（スクロール表示）
   void dispFont16x16sd2(uint8_t* matrixdata32,int t);        // VFDに16x16フォントを表示（スクロール表示）
   void dispFont8x16(uint8_t* matrixdata16);                  // VFDに8x16フォントを表示（1文字分のデータ送信のみ/LATなし）
   void dispFont8x16s(uint8_t* matrixdata16, int t);          // VFDに8x16フォントを表示（スクロール表示）

   void dispSJIS1byte(uint16_t sjis, int t);                          // 1byteのSJISをスクロール表示する
   void dispSJIS2byte(uint16_t sjis, int t) ;                         // 2byteのSJISをスクロール表示する
   void dispSentence(const uint8_t* tablename,int t, uint8_t flgZ=0,uint8_t flgLat=0); // SJISコードの文字列をVFDにスクロール表示する
   
 public:
   static void mw_shiftOut(uint8_t dataPin,uint8_t clockPin,uint8_t bitOrder,byte val);
   static void mw_digitalWrite(uint8_t pin, uint8_t val);
};

#endif


