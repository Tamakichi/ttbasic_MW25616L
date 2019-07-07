//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// GPIO機能の実装
// 2019/06/08 by たま吉さん 

#include "Arduino.h"
#include "basic.h"

#define IsPWM_PIN(N) IsUseablePin(N,FNC_PWM)      // 指定ピンPWM利用可能判定
#define IsADC_PIN(N) IsUseablePin(N,FNC_ANALOG)   // 指定ピンADC利用可能判定
#define IsIO_PIN(N)  IsUseablePin(N,FNC_IN_OUT)   // 指定ピンデジタル入出力利用可能判定

// ピン機能利用設定フラグ(ORによる重複指定OK)
#define FNC_IN_OUT  1  // デジタルIN/OUT
#define FNC_PWM     2  // PWM
#define FNC_ANALOG  4  // アナログIN

#ifdef ARDUINO_AVR_MEGA2560
// Arduino Mrga2560 ピン機能チェックテーブル
const PROGMEM uint8_t  pinFunc[] = {
  0,0,1,1|2,1|2,1|2,1|2,1|2,1|2,1|2,        // ポート0 -  9: 
  1|2,1|2,1|2,1|2,1,1,1,1,1,1,              // ポート10 - 19: 
  1,1,1,1,1,1,1,1,1,1,                      // ポート20 - 29: 
  1,1,1,1,1,1,1,1,1,1,                      // ポート30 - 39: 
  1,1,1,1,1|2,1|2,1|2,1,1,1,                // ポート40 - 49:   
  1,1,1,1,1|4,1|4,1|4,1|4,1|4,1|4,          // ポート50 - 59: 
  1|4,1|4,1|4,1|4,1|4,1|4,1|4,1|4,1|4,1|4,  // ポート60 - 69:  
};
#else
// Arduino Uno/nano/Pro mini ピン機能チェックテーブル
const PROGMEM uint8_t  pinFunc[] = {
  0,0,1,1|2,1,1|2,1|2,1,1,1|2,          // ポート0 -  9: 
  1|2,1|2,1,1,1|4,1|4,1|4,1|4,1|4,1|4,  // ポート10 - 19: 
  1|4,1|4,
};
#endif

// ピン利用可能チェック
inline uint8_t IsUseablePin(uint8_t pinno, uint8_t fnc) {
  return pgm_read_byte_near(&pinFunc[pinno]) & fnc;
}

// **** I2Cライブラリの利用設定(オプション) ********
#if USE_CMD_I2C == 1
  #include <Wire.h>
#endif 

#if USE_SO1602AWWB == 1
//*** I2C接続キャラクタディスプレイ **************
#include "src/lib/SO1602AWWB.h"
#endif 

// GPIO ピン機能設定
void igpio() {
  int16_t pinno;  // ピン番号
  int16_t pmode;  // 入出力モード

  // 入出力ピンの指定
  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) || // ピン番号取得
       getParam(pmode, 0, 2, false) ) return;     // 入出力モードの取得

  // デジタル入出力として利用可能かチェック
  if (!IsIO_PIN(pinno)) {
    err = ERR_GPIO;
    return;    
  }
  pinMode(pinno, pmode);
}

// GPIO ピンデジタル出力
void idwrite() {
  int16_t pinno,  data;

  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) || // ピン番号取得
       getParam(data, false) ) return;            // データ指定取得
  data = data ? HIGH: LOW;

  if (! IsIO_PIN(pinno)) {
    err = ERR_GPIO;
    return;
  }
  
  // データ出力
  digitalWrite(pinno, data);
}

// GPIO ピンデジタル入力
// IN(ピン番号)
int16_t idread() {
  int16_t value;
  if ( !(checkOpen()|| getParam(value,0,TT_MAX_PINNUM, false) || checkClose()) ) {
    if ( !IsIO_PIN(value) )  {
      err = ERR_GPIO;
    } else {
      value = digitalRead(value);  // 入力値取得
    }
  }
  return value;
}

// GPIO アナログ入力
// ANA(ピン番号)
int16_t iana() {
  int16_t value;
  if ( !(checkOpen()|| getParam(value,0,TT_MAX_PINNUM, false) || checkClose()) ) {
    if ( !IsADC_PIN(value) ) {
      err = ERR_GPIO;
    } else {
      value = analogRead(value);    // 入力値取得
    }
  }
  return value; 
}

// PWMコマンド
// PWM ピン番号, DutyCycle
void ipwm() {
  int16_t pinno;      // ピン番号
  int16_t duty;       // デューティー値 0～255

  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) ||    // ピン番号取得
       getParam(duty,  0, 255, false) ) return;      // デューティー値

  // PWMピンとして利用可能かチェック
  if (!IsPWM_PIN(pinno)) {
    err = ERR_GPIO;
    return;    
  }
  analogWrite(pinno, duty);
}

// shiftOutコマンド SHIFTOUT dataPin, clockPin, bitOrder, value
void ishiftOut() {
#if USE_SHIFTOUT == 1
  int16_t dataPin, clockPin;
  int16_t bitOrder;
  int16_t data;

  if (getParam(dataPin, 0,TT_MAX_PINNUM, true)) return;
  if (getParam(clockPin,0,TT_MAX_PINNUM, true)) return;
  if (getParam(bitOrder,0,1, true)) return;
  if (getParam(data, 0,255,false)) return;

  if ( !IsIO_PIN(dataPin) ||  !IsIO_PIN(clockPin) ) {
    err = ERR_GPIO;
    return;
  }
  shiftOut(dataPin, clockPin, bitOrder, data);
#endif
}

#if USE_CMD_I2C == 1
void i2c_init() {
  Wire.begin();                      // I2C利用開始 
}
#endif

// I2Cデータ送受信
// 引数 mode:
// 1： I2CW関数  I2CW(I2Cアドレス, コマンドアドレス, コマンドサイズ, データアドレス, データサイズ)
// 0： I2CR関数  I2CR(I2Cアドレス, コマンドアドレス, コマンドサイズ, 受信データアドレス,受信データサイズ)
int16_t ii2crw(uint8_t mode) {
#if USE_CMD_I2C == 1
  int16_t  i2cAdr, ctop, clen, top, len;
  uint8_t* ptr;
  uint8_t* cptr;
  int16_t  rc;

  if (checkOpen() ||
      getParam(i2cAdr, 0,  0x7f, true) ||
      getParam(ctop,   0, 32767, true) ||
      getParam(clen,   0, 32767, true) ||
      getParam(top,    0, 32767, true) ||
      getParam(len,    0, 32767,false) ||
      checkClose()
   ) return 0;

  ptr  = v2realAddr(top);
  cptr = v2realAddr(ctop);
  if (ptr == 0 || cptr == 0 || v2realAddr(top+len) == 0 || v2realAddr(ctop+clen) == 0) 
     { err = ERR_VALUE; return 0; }

  if (mode) {
  // I2Cデータ送信
    Wire.beginTransmission(i2cAdr);
    if (clen) Wire.write(cptr, clen);
    if (len)  Wire.write(ptr, len);
    rc =  Wire.endTransmission();
    return rc;
  } else {
    // I2Cデータ送受信
    Wire.beginTransmission(i2cAdr);
    if (clen) Wire.write(cptr, clen);
    rc = Wire.endTransmission();
    if (len) {
      if (rc!=0)
        return rc;
      Wire.requestFrom(i2cAdr, len);
      while (Wire.available()) {
        *(ptr++) = Wire.read();
      }
    }  
    return rc;    
  }
#else
  return 1;
#endif
}

#if USE_SHIFTIN ==1
uint8_t _shiftIn( uint8_t ulDataPin, uint8_t ulClockPin, uint8_t ulBitOrder, uint8_t lgc){
  uint8_t value = 0 ;
  uint8_t i ;
  for ( i=0 ; i < 8 ; ++i ) {
    digitalWrite( ulClockPin, lgc ) ;
    if ( ulBitOrder == LSBFIRST )  value |= digitalRead( ulDataPin ) << i ;
    else  value |= digitalRead( ulDataPin ) << (7 - i) ;
    digitalWrite( ulClockPin, !lgc ) ;
  }
  return value ;
}
#endif

// SHIFTIN関数 SHIFTIN(データピン, クロックピン, オーダ[,ロジック])
int16_t ishiftIn() {
#if USE_SHIFTIN == 1
  int16_t rc;
  int16_t dataPin, clockPin;
  int16_t bitOrder;
  int16_t lgc = HIGH;

  if (checkOpen() ||
      getParam(dataPin, 0,TT_MAX_PINNUM, true) ||
      getParam(clockPin,0,TT_MAX_PINNUM, true) ||
      getParam(bitOrder,0,1, false)
   ) return 0;
 
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(lgc,LOW, HIGH, false)) return 0;
  }
  if (checkClose()) return 0;
  rc = _shiftIn((uint8_t)dataPin, (uint8_t)clockPin, (uint8_t)bitOrder, lgc);
  return rc;
#else
  return 0;
#endif
}

// PULSEIN関数 PULSEIN(ピン, 検出モード, タイムアウト時間[,単位指定])
int16_t ipulseIn() {
#if USE_PULSEIN == 1
  int32_t rc=0;
  int16_t dataPin;       // ピン
  int16_t mode;          // 検出モード
  int16_t tmout;         // タイムアウト時間(0:ミリ秒 1:マイクロ秒)
  int16_t scale =1;      // 戻り値単位指定 (1～32767)

  // コマンドライン引数の取得
  if (checkOpen() ||                              // '('のチェック
      getParam(dataPin, 0,TT_MAX_PINNUM, true) || // ピン
      getParam(mode, LOW, HIGH, true) ||          // 検出モード
      getParam(tmout,0,32767, false)              // タイムアウト時間(ミリ秒)
   ) return 0;           

  if (*cip == I_COMMA) {
    cip++;
    if (getParam(scale,1, 32767, false)) return 0;        // 戻り値単位指定 (1～32767)
  }
  if (checkClose()) return 0;                             // ')'のチェック
  
  rc = pulseIn(dataPin, mode, ((uint32_t)tmout)*1000)/scale;  // パルス幅計測
  if (rc > 32767) rc = -1; // オーバーフロー時は-1を返す
  return rc;
#else
  return 0;
#endif  
}

#if USE_IR == 1
// IR関数  IR(ピン番号[,リピート])
int16_t iir() {
  static int16_t code = 0; // ボタンコード
  int16_t pinno;           // ピン番号
  int16_t rpt = 1;         // リピート機能
  uint32_t rc;

  if (checkOpen()) return 0;                              // '('のチェック
  if (getParam(pinno, 0,TT_MAX_PINNUM, false)) return 0;  // ピン
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(rpt,0, 1, false)) return 0;              // リピート機能
  }
  if (checkClose()) return 0;                             // ')'のチェック

  pinMode(pinno,INPUT);
  rc = Read_IR(pinno,100);  // IR受信
  if (rc > 0 && rc <= 3 )
    code = -rc;
  else if (rpt == 0 && rc == 0)
    code = -1;
  else if (rc)
    code = (rc >>8)&0xff;
  return code;
}
#endif

#if USE_CMD_I2C == 1 && USE_RTC_DS3231 == 1
#define BCD(c) (uint8_t)((c/10)*16+(c%10))
// SETDATEコマンド  SETDATE 年,月,日,曜日,時,分,秒
void isetDate() {
  int16_t t[7];
  if ( getParam(t[6], 2000,2099, true) ) return; // 年
  if ( getParam(t[5],    1,  12, true) ) return; // 月
  if ( getParam(t[4],    1,  31, true) ) return; // 日
  if ( getParam(t[3],    1,  7 , true) ) return; // 曜日
  if ( getParam(t[2],    0,  23, true) ) return; // 時
  if ( getParam(t[1],    0,  59, true) ) return; // 分
  if ( getParam(t[0],    0,  59, false)) return; // 秒  
  t[6]-=2000;
  
  // RTCの設定
  Wire.beginTransmission(0x68);
  Wire.write(0x00);

  for (uint8_t i=0; i<7; i++)
     Wire.write( BCD(t[i]));
  if (Wire.endTransmission())
    err = ERR_I2CDEV;
}

// 該当変数に値をセット
void setValueTo(uint16_t* rcv,uint8_t n) {
  int16_t index;  
  for (uint8_t i=0; i <n; i++) {    
    if (*cip == I_VAR) {          // 変数の場合
      cip++; index = *cip;        // 変数インデックスの取得
      var[index] = rcv[i];        // 変数に格納
      cip++;
    } else if (*cip == I_ARRAY) { // 配列の場合
      cip++;
      index = getparam();         // 添え字の取得
      if (err) return;  
      if (index >= SIZE_ARRY || index < 0 ) {
         err = ERR_SOR;
         return; 
      }
      arr[index] = rcv[i];        // 配列に格納
    } else {
      err = ERR_SYNTAX;           // 変数・配列でない場合はエラーとする
      return;   
    }     
    if(i != n-1) {
      if (*cip != I_COMMA) {      // ','のチェック
         err = ERR_SYNTAX;
         return; 
      }
      cip++;
    }
  }
}

// RTCからのデータ取得
uint8_t readRTC(uint16_t* rcv, uint8_t pos, uint8_t num){
  Wire.beginTransmission(0x68);
  Wire.write(pos);
  if (Wire.endTransmission()) {
    err = ERR_I2CDEV;
    return 1;
  }
  Wire.requestFrom((uint8_t)0x68, num);
  uint8_t d,cnt = 0;
  while (Wire.available()) {
    d = Wire.read();
    rcv[num-1-cnt] = (d>>4)*10 + (d & 0xf);
    cnt++;
    if (cnt == num) break;
  }
  return 0;
}

// GETDATEコマンド  GETDATE 年格納変数,月格納変数, 日格納変数, 曜日格納変数
void igetDate() {
  uint16_t rcv[4];
  if (readRTC(rcv,3,4))
    return;
  rcv[0]+=2000;
  setValueTo(rcv, 4);
}

// GETDATEコマンド  SETDATE 時格納変数,分格納変数, 秒格納変数
void igetTime() {
  uint16_t rcv[3];
  if (readRTC(rcv,0,3))
    return;
  setValueTo(rcv, 3);
}

// DATEコマンド
KW(w1,"Sun");KW(w2,"Mon");KW(w3,"Tue");KW(w4,"Wed");KW(w5,"Thr");KW(w6,"Fri");KW(w7,"Sat");
const char*  const weekstr[] PROGMEM = {w1,w2,w3,w4,w5,w6,w7};
void idate(uint8_t devno) {
  uint16_t rcv[7];

  if (readRTC(rcv,0,7))
    return;
       
   putnum((int16_t)rcv[0]+2000, -4 ,devno);
   c_putch('/',devno);
   putnum((int16_t)rcv[1], -2,devno);
   c_putch('/',devno);
   putnum((int16_t)rcv[2], -2,devno);
   c_puts_P((const char*)F(" ["),devno);
   c_puts_P((const char*)pgm_read_word(&weekstr[rcv[3]-1]),devno);
   c_puts_P((const char*)F("] "),devno);
   putnum((int16_t)rcv[4], -2,devno);
   c_putch(':',devno);
   putnum((int16_t)rcv[5], -2,devno);
   c_putch(':',devno);
   putnum((int16_t)rcv[6], -2,devno);
   newline(devno);
}

// DATE$()
void idatestr(uint8_t devno) {
  if (checkOpen()) return;
  if (checkClose()) return;
  idate(devno);    
}
#endif

// *** 有機ELキャラクタディスプレイ SO1602AWWB  ***
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
// OLEDキャラクタディスプレイのカーソル移動
// CLOCATE x,y
void iclocate() {
  int16_t x, y;
  if ( getParam(x, 0,15, true) ) return;
  if ( getParam(y, 0,1, false) ) return;

  // カーソル移動
  if (OLEDPos(x,y)) {
    err = ERR_I2CDEV;
  }
}

// OLEDキャラクタディスプレイ 画面クリア
// CCLS
void iccls() {
  if (OLEDcls())
    err = ERR_I2CDEV;
}

// OLEDキャラクタディスプレイ コントラスト設定
// CCONS n
void iccons() {
  int16_t cons;
  if ( getParam(cons, 0,255, false) ) return;
  if (OLEDcontrast(cons))
    err = ERR_I2CDEV;  
}

// OLEDキャラクタディスプレイ 画面表示・非表示設定
// CDISP n
void icdisp() {
  int16_t sw;
  if ( getParam(sw, 0,1, false) ) return;
  if (OLEDisplay(sw))
    err = ERR_I2CDEV;  
}

// OLEDキャラクタディスプレイ カーソル表示・非表示設定
// CCURS n
void iccurs() {
  int16_t sw;
  if ( getParam(sw, 0,1, false) ) return;
  if (OLEDCurs(sw))
    err = ERR_I2CDEV;  
}

// OLEDキャラクタディスプレイ 文字列表示
// CPRINT
void icprint() {
  iprint(CDEV_CLCD);
}
#endif
