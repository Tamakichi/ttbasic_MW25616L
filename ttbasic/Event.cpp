//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// イベント処理（タイマー割込み、ピン変化割り込み）
// 作成 2019/07/09 by たま吉さん 
// 修正 2019/08/07 外部割込みイベント機能の追加(ON PIN)
// 修正 2019/08/12 SLEEP機能の追加(SLEEPコマンド)
// 修正 2019/08/19 SLEEP機能の仕様変更(ウオッチドックタイマ利用)
// 修正 2019/08/30 ON PIN.. の仕様変更、ピンモードの引数の追加
// 修正 2019/08/31 MEGA2560でのSLEEP BOD部コンパイルエラー不具合対応

#include <avr/sleep.h> 
#include "Arduino.h"
#include "basic.h"

#if USE_EVENT == 1
#include "src/lib/TimerOne.h"
//#include <MsTimer2.h>

// タイマーイベント管理情報
typedef struct _TEventInfo {
  int16_t  period;               // 周期(ミリ秒)
  uint8_t  flgActive;            // (1ビット) タイマー割り込み実行状態 (0:未実行、1:実行中)
  uint8_t  action;               // (2ビット) 0:未登録、 1:I_GOTO、2:I_GOSUB
  volatile uint8_t flgEvent;     // (1ビット) 発生フラグ（キュー）
  uint16_t param;                // 呼び出し行|ラベル位置
} TEventInfo;
TEventInfo tevt;

// 外部割込みイベント管理情報
typedef struct _EEventInfo {
  uint32_t prevInterrupt;        // 直前の割り込み
  uint8_t  mode;                 // モード(4ビット)
  uint8_t  action;               // 0:未登録、 1:I_GOTO、2:I_GOSUB(2ビット)
  volatile uint8_t flgEvent;     // 発生フラグ（キュー）(1ビット)
  uint16_t param;                // 呼び出し行|ラベル位置
} EEventInfo;
EEventInfo eevt[2]; 

// タイマーイベントハンドラ
void handleTimerEvent() {
  tevt.flgEvent = 1 ; // 割り込み発生
}

// 外部割込み1イベントハンドラサブルーチン
// 10ミリ秒以内の連続変化は無視する（チャタリング防止）
void handleExt1EventSub(uint8_t no) {
  //Serial.println("Fire!");
  if (millis() - eevt[no].prevInterrupt <= 10) 
    return;
  detachInterrupt(0);     // 割り込み停止
  eevt[no].flgEvent = 1 ; // 割り込み発生
  eevt[no].prevInterrupt = millis();    
}

// 外部割込み0イベントハンドラ
void handleExt0Event() {
  handleExt1EventSub(0);
}

// 外部割込み1イベントハンドラ
void handleExt1Event() {
  handleExt1EventSub(1);
}

// タイマーイベント利用クリア
void clerTimerEvent() {
  Timer1.stop();      // 初期は停止状態
//  MsTimer2::stop();   // 初期は停止状態
  tevt.action = 0;    // 0:未登録、 1:I_GOTO、2:I_GOSUB
  tevt.flgActive = 0; // タイマー割り込み実行状態 (0:未実行)
  tevt.flgEvent = 0;  // 発生フラグ（キュー） なし
}

// 外部割込みイベント利用クリア
void clerExtEvent() {
  eevt[0].prevInterrupt = 0;    // 0:未登録、 1:I_GOTO、2:I_GOSUB
  eevt[0].action = 0;           // 0:未登録、 1:I_GOTO、2:I_GOSUB
  eevt[0].flgEvent = 0;         // 発生フラグ（キュー） なし
  eevt[1].prevInterrupt = 0;    // 0:未登録、 1:I_GOTO、2:I_GOSUB
  eevt[1].action = 0;           // 0:未登録、 1:I_GOTO、2:I_GOSUB
  eevt[1].flgEvent = 0;         // 発生フラグ（キュー） なし
}

// タイマーイベント利用のための初期化
void initTimerEvent() {
  Timer1.initialize(100000);                 // マイクロ秒単位で設定(0.1秒仮設定)
  Timer1.attachInterrupt(handleTimerEvent);  // タイマーイベントハンドラの登録
  clerTimerEvent();
  delay(10);
  tevt.flgEvent = 0 ;                        // 割り込み発生リセット(初回をクリア)
}

// ON TIMER 周期 GOTO|GOSUB 行番号|ラベル
// ON PIN ピン番号,ピンモード,検出モード GOTO|GOSUB 行番号|ラベル
void iOnPinTimer() {
  int16_t tm, pin,pmode, smode, fnc = 0;

  if (*cip == I_TIMER) {   // 'TIMER'のチェック
      fnc = I_TIMER;
      cip++;
      if ( getParam(tm, 1,32767, false) )  return;  // 周期の取得
      tevt.period = tm;     // 周期 
      tevt.flgActive = 0;   // タイマー割り込み実行状態 (0:未実行)
      tevt.flgEvent = 0;    // 発生フラグ（キュー） なし
  } else if (*cip == I_PIN) { // 'PIN'のチェック
      fnc = I_PIN;        
      cip++;
      if ( getParam(pin,  2,3, true) )  return;      // ピン番号の取得
      if ( getParam(pmode, true) )      return;      // ピンモードの取得
      if ( getParam(smode, 0,3, false) ) return;     // 検出モードの取得
      if (pmode != INPUT && pmode != INPUT_PULLUP) return;       
      eevt[pin-2].mode = smode;    // 検出モード
      eevt[pin-2].flgEvent = 0;    // 発生フラグ（キュー） なし
      pinMode(pin, pmode);
  } else {
      err = ERR_SYNTAX;
      return;
  }

  // GOTO|GOSUBのチェック
  if (*cip == I_GOTO) {
    if (fnc == I_TIMER)
      tevt.action = 1;
    else
      eevt[pin-2].action = 1;    
  } else if (*cip == I_GOSUB) {
    if (fnc == I_TIMER)
      tevt.action = 2;
    else {
      eevt[pin-2].action = 2;
    }
  } else {
    err = ERR_SYNTAX;
    return;
  }

  // 飛び先行ポインタ
  cip++;
  uint8_t* lp = getJumplp();
  if (!err) {
    if (fnc == I_TIMER)
      tevt.param = lp;
    else 
      eevt[pin-2].param = lp;
  }
}

// TIMER ON|OFF
void iTimer() {
  int16_t sw;
  if ( getParam(sw, 0,1,false) ) 
    return; 

  // タイマーイベント登録チェック
  if (!tevt.action) {
    err = ERR_NOEDEF; // タイマーイベント未設定
    return;
  }

  // タイマー割込みの設定
  if (sw) {
    tevt.flgActive = 1;
    tevt.flgEvent = 0;
    Timer1.initialize(((uint32_t)tevt.period)*1000L); 
    Timer1.start();
//    MsTimer2::set(tevt.period, handleTimerEvent); 
//    MsTimer2::start();
    delay(10);
    tevt.flgEvent = 0 ;      // 割り込み発生リセット（初回クリア）
  } else {
    Timer1.stop();
//    MsTimer2::stop();
    tevt.flgActive = 0;
    tevt.flgEvent = 0;
  }
}

// Pin ピン番号,ON|OFF
void iPin() {
  int16_t pin,sw;
  if ( getParam(pin, 2,3,true) ) return; 
  if ( getParam(sw, 0,1,false) ) return; 

  // 外部割込みの設定
  if (sw) {
    if (!eevt[pin-2].action) {
      // ON PIN未設定
      err = ERR_NOEDEF; // タイマーイベント未設定
      return;      
    }
    eevt[pin-2].flgEvent = 0;
    // 外部割込みの設定
    eevt[pin-2].prevInterrupt =  millis();
    if (pin == 2)  { attachInterrupt(0, handleExt0Event, eevt[0].mode);  }
    else           { attachInterrupt(1, handleExt1Event, eevt[1].mode); }
  } else {
    eevt[pin-2].flgEvent = 0;
    detachInterrupt(pin-2);
  }
}

// タイマーイベントの実行
void doTimerEvent() {
  if (tevt.flgActive && tevt.flgEvent) {
    tevt.flgEvent = 0;
    if (tevt.action == 1) {
      // GOTO文の場合
      iGotoGosub(MODE_ONGOTO,tevt.param);
    } else if (tevt.action == 2) {
      // GOSUB文の場合
      iGotoGosub(MODE_ONGOSUB,tevt.param);
    }
  }
}

// 外部割込みイベントの実行
void doExtEvent() {
  for (uint8_t i=0; i <2; i++) {
    if (eevt[i].flgEvent) {
      eevt[i].flgEvent = 0;
      if (eevt[i].action == 1) {
        // GOTO文の場合
        iGotoGosub(MODE_ONGOTO,eevt[i].param);
      } else if (eevt[i].action == 2) {
        // GOSUB文の場合
        iGotoGosub(MODE_ONGOSUB,eevt[i].param);
      }
    }
  }
}

#if USE_SLEEP == 1
// WDT利用のための定義
#define WDT_reset() __asm__ __volatile__ ("wdr")
#define sleep()     __asm__ __volatile__ ("sleep")
#define WDT_16MS    B000000
#define WDT_32MS    B000001
#define WDT_64MS    B000010
#define WDT_125MS   B000011
#define WDT_250MS   B000100
#define WDT_500MS   B000101
#define WDT_1S      B000110
#define WDT_2S      B000111
#define WDT_4S      B100000
#define WDT_8S      B100001

// WDTタイマー開始
// wt:間隔 0:永遠 1～8000 
void WDT_start(uint16_t wt) {
  uint8_t t;                        // タイムアウト時間設定値
  if (wt >= 8000)
    t = WDT_8S;
  else if (wt >= 4000) 
    t = WDT_4S;
  else if (wt >= 2000)
    t = WDT_2S;
  else if (wt >= 1000) 
    t = WDT_1S;
  else if (wt >= 500) 
    t = WDT_500MS;
  else if (wt >= 250) 
    t = WDT_250MS;
  else if (wt >= 125) 
    t = WDT_125MS;
  else if (wt >= 64) 
    t = WDT_64MS;
  else if (wt >= 32) 
    t = WDT_32MS;
  else 
    t = WDT_16MS;

  MCUSR &= ~(1 << WDRF);                // MCU Status Reg. Watchdog Reset Flag ->0
  WDTCSR |= (1 << WDCE) | (1 << WDE);   // ウォッチドッグ変更許可（WDCEは4サイクルで自動リセット）
  WDTCSR = t;                           // 制御レジスタを設定
  WDTCSR |= _BV(WDIE);
}

// WDTタイマー停止
void WDT_stop() {
  //cli();                           // 全割り込み停止
  WDT_reset();                       // ウォッチドッグタイマリセット
  WDTCSR &= ~_BV(WDRF);              // ウォッチドッグリセットフラグ解除
  WDTCSR |= _BV(WDCE)|_BV(WDE);      // WDCEとEDEに1をセット
  WDTCSR =  0;                       // ウォッチドッグ禁止
  //sei();                           // 全割り込み許可
}

// WDTタイマー割り込みで呼び出される関数
ISR(WDT_vect) {
  WDT_stop();
}

// SLEEPコマンド
// SLEEP [時間]
// 時間：待ち時間 0 ～ 8000ms(デフォルト値 8000、0指定で無限待ち）
void isleep() {
  int16_t tm = 8000; // 待ち時間

  //表示開始行番号の設定
  if (*cip != I_EOL && *cip != I_COLON) {
    if (getParam(tm,0,8000,false)) return;  // 待ち時間
  }  

  Serial.end();
  if (tm != 0) {
    // 無限待ちでない場合、ウオッチドックタイマの設定
    WDT_start(tm);
  }

  ADCSRA &= ~(1 << ADEN);  // ADCを停止
  set_sleep_mode(SLEEP_MODE_PWR_SAVE);
  //set_sleep_mode(SLEEP_MODE_PWR_DOWN);  // パワーダウンモード指定
  sleep_enable();
  //sleep_mode();

#ifndef ARDUINO_AVR_MEGA2560  // MEGA2560はBOD停止未サポート
  // BODを停止（消費電流 27→6.5μA）
  MCUCR |= (1 << BODSE) | (1 << BODS);   // MCUCRのBODSとBODSEに1をセット
  MCUCR = (MCUCR & ~(1 << BODSE)) | (1 << BODS);  // すぐに（4クロック以内）BODSSEを0, BODSを1に設定
#endif

  asm("sleep");                         // 3クロック以内にスリープ sleep_mode();では間に合わなかった
  sleep_disable();                      // WDTがタイムアップでここから動作再開
  if (tm != 0) {
    // 無限待ちでない場合、ウオッチドックタイマの設定
    WDT_stop();
  }
  ADCSRA |= (1 << ADEN);                // ADCをON
  Serial.begin(SERIALBAUD);
  delay(20);                            // シリアル通信受信用のウェイト
}
#endif // USE_SLEEP
#endif // USE_EVENT
