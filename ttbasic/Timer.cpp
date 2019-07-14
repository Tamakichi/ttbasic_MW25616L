//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// タイマー割込み
// 2019/07/09 by たま吉さん 
//

#include "Arduino.h"
#include "basic.h"

#if USE_TIMEREVENT == 1
#include "src/lib/TimerOne.h"

// タイマーイベント管理情報
int16_t  te_period;               // 周期(ミリ秒)
uint8_t   te_action;              // 0:未登録、 1:I_GOTO、2:I_GOSUB
uint16_t  te_param;               // 呼び出し行|ラベル位置
uint8_t   te_flgActive;           // タイマー割り込み実行状態 (0:未実行、1:実行中)
volatile uint8_t te_flgEvent;     // 発生フラグ（キュー）

uint8_t sw = 0;

// タイマーイベントハンドラ
void handleTimerEvent() {
  //digitalWrite(13,sw);
  //sw = sw?0:1;
  te_flgEvent = 1 ; // 割り込み発生
}

// タイマーイベント利用クリア
void clerTimerEvent() {
  Timer1.stop();      // 初期は停止状態
  te_action = 0;      // 0:未登録、 1:I_GOTO、2:I_GOSUB
  te_flgActive = 0;   // タイマー割り込み実行状態 (0:未実行)
  te_flgEvent = 0;    // 発生フラグ（キュー） なし
}

// タイマーイベント利用のための初期化
void initTimerEvent() {
  Timer1.initialize(100000);                 // マイクロ秒単位で設定(0.1秒仮設定)
  Timer1.attachInterrupt(handleTimerEvent);  // タイマーイベントハンドラの登録
  clerTimerEvent();
  delay(10);
  te_flgEvent = 0 ;                          // 割り込み発生リセット(初回をクリア)
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
    te_action = 1;
  } else if (*cip == I_GOSUB) {
    te_action = 2;
  } else {
    te_action = 0;
    err = ERR_SYNTAX;
    return;
  }

  cip++;
  te_period = tm;     // 周期 
  te_flgActive = 0;   // タイマー割り込み実行状態 (0:未実行)
  te_flgEvent = 0;    // 発生フラグ（キュー） なし

  // 飛び先行ポインタ
  uint8_t* lp = getJumplp();
  if (!err)
    te_param = lp;
}

// TIMER ON|OFF
void iTimer() {
  int16_t sw;
  if ( getParam(sw, 0,1,false) ) 
    return; 

  // タイマーイベント登録チェック
  if (!te_action) {
    err = ERR_NOTIMER; // タイマーイベント未設定
    return;
  }

  // タイマー割込みの設定
  if (sw) {
    te_flgActive = 1;
    te_flgEvent = 0;
    Timer1.initialize(((uint32_t)te_period)*1000L); 
    Timer1.start();
    delay(10);
    te_flgEvent = 0 ;      // 割り込み発生リセット（初回クリア）
  } else {
    Timer1.stop();
    te_flgActive = 0;
    te_flgEvent = 0;
  }
}

// タイマーイベントの実行
void doTimerEvent() {
  if (te_flgActive && te_flgEvent) {
    te_flgEvent = 0;
    if (te_action == 1) {
      // GOTO文の場合
      iGotoGosub(MODE_ONGOTO,te_param);
    } else if (te_action == 2) {
      // GOSUB文の場合
      iGotoGosub(MODE_ONGOSUB,te_param);
    }
  }
}
#endif
