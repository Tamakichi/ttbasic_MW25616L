//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// イベント処理（タイマー割込み、ピン変化割り込み）
// 2019/07/09 by たま吉さん 
//

#include "Arduino.h"
#include "basic.h"

#if USE_TIMEREVENT == 1
#include "src/lib/TimerOne.h"

// タイマーイベント管理情報
typedef struct _TEventInfo {
int16_t  period;               // 周期(ミリ秒)
uint8_t  action;               // 0:未登録、 1:I_GOTO、2:I_GOSUB
uint16_t param;                // 呼び出し行|ラベル位置
uint8_t  flgActive;            // タイマー割り込み実行状態 (0:未実行、1:実行中)
volatile uint8_t flgEvent;     // 発生フラグ（キュー）
} TEventInfo;

TEventInfo tevt;
uint8_t sw = 0;

// タイマーイベントハンドラ
void handleTimerEvent() {
  //digitalWrite(13,sw);
  //sw = sw?0:1;
  tevt.flgEvent = 1 ; // 割り込み発生
}

// タイマーイベント利用クリア
void clerTimerEvent() {
  Timer1.stop();      // 初期は停止状態
  tevt.action = 0;    // 0:未登録、 1:I_GOTO、2:I_GOSUB
  tevt.flgActive = 0; // タイマー割り込み実行状態 (0:未実行)
  tevt.flgEvent = 0;  // 発生フラグ（キュー） なし
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
void iOnTimer() {
  int16_t tm;

  // 'TIMER'のチェック
  if (*cip != I_TIMER) {
      err = ERR_SYNTAX;
      return;
  }

  // 周期の取得
  cip++;
  if ( getParam(tm, 1,32767, false) ) 
    return; 

  // GOTO|GOSUBのチェック
  if (*cip == I_GOTO) {
    tevt.action = 1;
  } else if (*cip == I_GOSUB) {
    tevt.action = 2;
  } else {
    tevt.action = 0;
    err = ERR_SYNTAX;
    return;
  }

  cip++;
  tevt.period = tm;     // 周期 
  tevt.flgActive = 0;   // タイマー割り込み実行状態 (0:未実行)
  tevt.flgEvent = 0;    // 発生フラグ（キュー） なし

  // 飛び先行ポインタ
  uint8_t* lp = getJumplp();
  if (!err)
    tevt.param = lp;
}

// TIMER ON|OFF
void iTimer() {
  int16_t sw;
  if ( getParam(sw, 0,1,false) ) 
    return; 

  // タイマーイベント登録チェック
  if (!tevt.action) {
    err = ERR_NOTIMER; // タイマーイベント未設定
    return;
  }

  // タイマー割込みの設定
  if (sw) {
    tevt.flgActive = 1;
    tevt.flgEvent = 0;
    Timer1.initialize(((uint32_t)tevt.period)*1000L); 
    Timer1.start();
    delay(10);
    tevt.flgEvent = 0 ;      // 割り込み発生リセット（初回クリア）
  } else {
    Timer1.stop();
    tevt.flgActive = 0;
    tevt.flgEvent = 0;
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
#endif
