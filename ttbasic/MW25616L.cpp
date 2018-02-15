#include "MW25616L.h"
#include <avr/pgmspace.h>    //PROGMEM定義ライブラリ

// デストラクタ
MW25616L::~MW25616L(void) {
/*
  if (flgextbuf) {    
    free(fbuf); // 動的獲得メモリの開放
  }
*/
}

// MW25616Lモジュールの利用開始
uint8_t MW25616L::begin() {

  // 利用ピンモードの設定
  pinMode(mw_si, OUTPUT);
  pinMode(mw_ck, OUTPUT);
  pinMode(mw_lat, OUTPUT);
  pinMode(mw_en, OUTPUT);
  pinMode(mw_pwm, OUTPUT);
  delay(1);

  // 利用リンの出力設定
  mw_digitalWrite(mw_si, HIGH);
  mw_digitalWrite(mw_ck, HIGH);
  mw_digitalWrite(mw_lat, LOW);
  mw_digitalWrite(mw_en, LOW);    //EN=0(disp OFF)  

  // 電源モータドライバ入力
  // PWM出力
  TCCR1B &= B11111000;
  TCCR1B |= B00000001;        // pin9,10/PWM分周比1(=31.37kHz)
  analogWrite(mw_pwm,127);    // 50%(31kHz)
  //参考：http://theoriesblog.blogspot.jp/2014/05/arduino-pwm.html
  delay(500);

  mw_clr();                      // 画面クリア(念のため起動時の不確定データを消去)
  mw_digitalWrite(mw_en, HIGH);  // EN=1(disp ON)

  // フォントROMドライバの利用開始
  FROM.begin(); 

  return true;
}

// 高速版シリアルビット出力
void MW25616L::mw_shiftOut(uint8_t dataPin,uint8_t clockPin,uint8_t bitOrder,byte val) {
  uint8_t i;
  uint8_t bit_data = digitalPinToBitMask(dataPin);
  uint8_t bit_clock = digitalPinToBitMask(clockPin);
  volatile uint8_t *out_data = portOutputRegister(digitalPinToPort(dataPin));
  volatile uint8_t *out_clock = portOutputRegister(digitalPinToPort(clockPin));

  for (i = 0; i < 8; i++)  {
    if (bitOrder == LSBFIRST) {
      if(val & (1 << i)) {
        *out_data |= bit_data;
      } else {
        *out_data &= ~bit_data;
      }
    } else {
      if(val & (1 << (7 - i))) {
        *out_data |= bit_data;
      } else {
        *out_data &= ~bit_data;
      }
    }
    *out_clock |= bit_clock;
    *out_clock &= ~bit_clock;
  }
}

// digitalWrite高速化版
void MW25616L::mw_digitalWrite(uint8_t pin, uint8_t val) {
  uint8_t bit = digitalPinToBitMask(pin);
  volatile uint8_t *out = portOutputRegister(digitalPinToPort(pin));
  if (val == LOW)
    *out &= ~bit;
  else
    *out |= bit;  
}

// 1byteデータ書き込み
void MW25616L::mw_put(unsigned char onebyte) {
  mw_shiftOut(mw_si, mw_ck, LSBFIRST, onebyte) ;    //シフトアウト機能
}

// LATパルス
void MW25616L::wr_lat() {
  mw_digitalWrite(mw_lat, HIGH);
  mw_digitalWrite(mw_lat, LOW);
}

// 画面クリア
void MW25616L::mw_clr() {
  for (int i=0; i <= MW25616L_WIDTH; i++) {
      mw_put(0x00);
      mw_put(0x00);
  }
  wr_lat();
}

// テーブルデータの連続書き込み
void MW25616L::mw_put_t(uint8_t*  tablename,int dn) {    //dn=データ数
  int k=0;
  while (k < dn) {
    mw_put(tablename[k]);
    k++;
  }
}

// テーブルデータのスクロール表示
// dn=データ数,ck=1:下段,2:上段,stime=スクロールスピード(delay_time)
void MW25616L::mw_put_s(uint8_t* tablename, int dn,int stime) {
  int k=0;
  int j=0;
  while (k < (dn/2)) {
    j= k * 2;
    mw_put(tablename[j]);
    mw_put(tablename[j+1]);
    wr_lat();
    delay(stime);
    k++;
  }
}

//空白表示（n:横ドット数）
void MW25616L::disp_non(int n) {
  int k = 0;
  while (k < n) {
      mw_put(0x00);
      mw_put(0x00);
      k++;
  }
}

//空白表示スクロール（n:横ドット数, t:停止時間）
void MW25616L::disp_non_t(int n, int t) {
  int k = 0;
  while (k < n) {
      mw_put(0x00);
      mw_put(0x00);
      delay(t);
      wr_lat();
      k++;
  }
}

// 輝度の設定
//  EN信号による輝度調整
//  (注)MW25616Lの仕様書には、スイッチングノイズによる誤動作防止のため、
//      データSI書き込み中にEN信号を変化させないこととの記載があります。
//      そのような使い方になる場合は注意してください。
void MW25616L::setBright(uint8_t level) {
  mw_bright = level;
  if (mw_display) {
    if (level == 0) {
      digitalWrite(mw_en, LOW);
    } else if (level == 255) {
      digitalWrite(mw_en, HIGH);
    } else {
      analogWrite(mw_en,level);
    }
  }
}

// 表示設定
void MW25616L::display(uint8_t sw) {
  mw_display = sw ? HIGH:LOW;
  if (mw_display) {
    setBright(mw_bright);
  } else {
    //  EN信号による全画面点灯・消灯
    digitalWrite(mw_en, LOW);  
  }
}
   
// VFDに16x16フォントを表示（1文字分のデータ送信のみ/LATなし）
void MW25616L::dispFont16x16(uint8_t* matrixdata32) {
  byte mdata = 0;
  for (int i = 0; i < 16; i++)  {
    mw_put(matrixdata32[i]);
    mw_put(matrixdata32[i + 16]);
  }
}

// VFDに16x16フォントを表示（1文字分のデータ送信のみ/LATなし）*/
void MW25616L::dispFont16x16d2(uint8_t* matrixdata32) {
  byte mdata = 0;
  for (int i = 0; i < 16; i++) {
    unsigned int fontdata1 = (matrixdata32[i+16] << 8) + (matrixdata32[i]);     //列=32bitデータに置換え
    fontdata1 = fontdata1 << 2;         //ROMフォントから2dot下へずらす
    mw_put(fontdata1);                  //下位バイト送信(文字の下半分)
    mw_put(fontdata1 >>8);              //上位バイト送信(文字の上半分)
  }
}

// VFDに16x16フォントを表示（スクロール表示）
void MW25616L::dispFont16x16s(uint8_t* matrixdata32, int t) {
  byte mdata = 0;
  for (int i = 0; i < 16; i++) {
    mw_put(matrixdata32[i]);
    mw_put(matrixdata32[i + 16]);
    wr_lat();
    delay(t);
  }
}

// VFDに16x16フォントを表示（スクロール表示）
void MW25616L::dispFont16x16sd2(uint8_t* matrixdata32, int t) {
  byte mdata = 0;
  for (int i = 0; i < 16; i++) {
    unsigned int fontdata1 = (matrixdata32[i+16] << 8) + (matrixdata32[i]);     //列=32bitデータに置換え
    fontdata1 = fontdata1 << 2;         //ROMフォントから2dot下へずらす
    mw_put(fontdata1);                  //下位バイト送信(文字の下半分)
    mw_put(fontdata1 >>8);              //上位バイト送信(文字の上半分)
    wr_lat();
    delay(t);
  }
}

// VFDに8x16フォントを表示（1文字分のデータ送信のみ/LATなし）
void MW25616L::dispFont8x16(uint8_t* matrixdata16) {
  byte mdata = 0;
  for (int i = 0; i < 8; i++) {
    unsigned int fontdata1 = (matrixdata16[i+8] << 8) + (matrixdata16[i]);     //列=32bitデータに置換え
    fontdata1 = fontdata1 << 1;         //ROMフォントから1dot下へずらす
    mw_put(fontdata1);                  //下位バイト送信(文字の下半分)
    mw_put(fontdata1 >>8);              //上位バイト送信(文字の上半分)
  }
}

// VFDに8x16フォントを表示（スクロール表示）
void MW25616L::dispFont8x16s(uint8_t* matrixdata16, int t) {
  byte mdata = 0;
  for (int i = 0; i < 8; i++) {
    unsigned int fontdata1 = (matrixdata16[i+8] << 8) + (matrixdata16[i]);     //列=32bitデータに置換え
    fontdata1 = fontdata1 << 1;         //ROMフォントから1dot下へずらす
    mw_put(fontdata1);                  //下位バイト送信(文字の下半分)
    mw_put(fontdata1 >>8);              //上位バイト送信(文字の上半分)
    wr_lat();
    delay(t);
  }
}

// 1byteのSJISをスクロール表示する
void MW25616L::dispSJIS1byte(uint16_t sjis, int t) {
  uint8_t matrixdata32[32];   //16×16用表示データ  
  
  /*読み出し*/
  FROM.getFontDataBySJIS(matrixdata32, sjis);
  /*VFD表示*/
  if (t)
    dispFont8x16s(matrixdata32, t);
  else
    dispFont8x16(matrixdata32);
}

// 2byteのSJISをスクロール表示する
void MW25616L::dispSJIS2byte(uint16_t sjis, int t) {
  uint8_t matrixdata32[32];   //16×16用表示データ  
  uint16_t jis = FROM.SJIS2JIS(sjis);

  /*読み出し*/

  FROM.getFontDataBySJIS(matrixdata32, sjis);

  if ( jis>>8 == 0x23 ) {                  // JISコードの上位バイトが0x23の場合
    if (t)
      dispFont16x16sd2(matrixdata32,t);    // フォントROMデータを2ドット下にシフトして表示
    else
      dispFont16x16d2(matrixdata32);       // フォントROMデータを2ドット下にシフトして表示
  } else {                                 // そうでなければ
    if (t) 
      dispFont16x16s(matrixdata32,t);      // フォントROMデータのまま表示
    else
      dispFont16x16(matrixdata32);         // フォントROMデータのまま表示    
  }
}

// SJISコードの文字列をVFDにスクロール表示する
void MW25616L::dispSentence(const uint8_t* tablename,int t, uint8_t flgZ,uint8_t flgLat) {
  byte xcode = 0;
  uint16_t code1 = 0;
  uint16_t code2 = 0;
  while(*tablename) {
    byte msbdata = *tablename++;
    if (xcode == 0) {
      //SJISの1バイトコードか否か判定
      if ( (msbdata < 0x80) || ((0xA0 < msbdata) && (msbdata <= 0xdF)) ) {
        if (flgZ) {
          dispSJIS2byte(FROM.HantoZen(msbdata), t);  //2バイトコードのフォント表示(16x16dot)
        } else {
          dispSJIS1byte(msbdata, t);    //1バイトコードのフォント表示(8x16dot)
        }
      } else {
        code1 = msbdata;                //2バイトコードの場合の1バイト目
        xcode = 1;
      }
    } else {
      code2 = msbdata;                  //2バイトコードの場合の2バイト目
      dispSJIS2byte(code1<<8|code2, t);   //2バイトコードのフォント表示(16x16dot)
      xcode = 0;
    }
  }
  if(!t & flgLat)
   wr_lat();
}
#if 0
// フレームバッファのクリア
void MW25616L::mw_claerbuf() {
  memset(fbuf,0,MW25616L_BUFSIZE);
}

// 点の描画
void MW25616L::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if((x < 0) || (y < 0) || (x >= _width) || (y >= _height)) return;
  if (color == 1) {// 点灯
    fbuf[(x<<1) + (y>>3)] |= 1<<(y&7);  
  } else if (color == 2) { // 反転
    if (fbuf[(x<<1) + (y>>3)] & (1<<(y&7)))
       fbuf[(x<<1) + (y>>3)] &= ~(1<<(y&7));  
    else
       fbuf[(x<<1) + (y>>3)] |= 1<<(y&7);  
  } else {// 消灯
    fbuf[(x<<1) + (y>>3)] &= ~(1<<(y&7));  
  }
}

// 指定座標のドット色の取得
uint16_t MW25616L::getPixel(int16_t x, int16_t y) {
   return (fbuf[(x<<1) + (y>>3)] & (1<<(y&7)));
}

// 全画面塗りつぶし
void MW25616L::fillScreen(uint16_t color) {
  color = color? 0xff:0;
  memset(fbuf,color, MW25616L_BUFSIZE);  
}

void MW25616L::update(void) {
  for (uint16_t i = 0; i< _width; i++) {
    mw_shiftOut(mw_si, mw_ck, LSBFIRST, fbuf[i<<1]) ;    //シフトアウト機能    
    mw_shiftOut(mw_si, mw_ck, LSBFIRST, fbuf[(i<<1)+1]) ;  //シフトアウト機能    
  }
  wr_lat();  
}
#endif
