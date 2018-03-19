//
// NEC方式 赤外線リモコン信号読み取り 2018/03/18 by たま吉さん
// 参考にしたサイト
//   Arduinoで遊ぶページ 赤外線リモコンコード解析(NECフォーマット)
//   http://garretlab.web.fc2.com/arduino/make/infrared_sensor_nec_format/
//

#include <Arduino.h>

#define RC_RDH_TS      6000    // リーダコードOFF間隔  9ms判定用
#define RC_RDL_TS      3000    // リーダコードON間隔   4.5ms判定用
#define RC_BITLOW_TS   1000    // ビットデータON間隔   1.69ms判定用  
#define RC_TMOVER      8000    // タイムオバー

// タイムアウト付きビット検出
uint8_t readBit(uint8_t pinNo, uint8_t d,uint16_t tm) {
  uint32_t t = millis()+tm;
  uint8_t rc = 1;
  while (millis()<t) {
    if (digitalRead(pinNo) == d) {
      rc = 0;
      break;
    }
  }
  return rc;
}

//
// 赤外線リモコンコード取得
// 4バイトのデータを返す
// CCCCDDdd
//    CCCC カスタムコード
//    DD   データコード
//    dd   データコードのビット反転(データチェック用)
//　ただし、
//    リピートコードの場合  0
//    タイムオーバー       1
//    エラーの場合         2
//  を返す.
//
uint32_t Read_IR(uint8_t pinNo,uint16_t tm) {
  uint8_t  repeat = 0;  // リピートコード検出フラグ
  uint32_t  dt    = 0;  // 赤外線リモコン読み取りデータ
  uint32_t t ;          // 信号長計測用

  if (readBit(pinNo,0,tm))      // ON検出受信待ち   
      return 1;                 // タイムオーバー

  // リーダーコード/リピートコードの検出チェック
  // ~~~~~~~~~~~~~|_________________
  //   9ms           4.5ms or 2.25ms
  t = micros();                 // ONの時間間隔取得
  while(!digitalRead(pinNo));   // ONの間待ち
  t = micros() -t;              // ON->OFFの時間間隔取得
  if (t < RC_RDH_TS) {          // リーダコードのHIGH長さ判定
     return 2;                  // エラー
  } 
  // リピートコード判定(LOWが2.25ms)
  t = micros();                 // OFF検出時刻取得
  while(digitalRead(pinNo));    // OFF間隔待ち  
  t = micros() -t;              // OFF->ON時間取得        
  if (t < RC_RDL_TS) {
    // 0N->OFF がリピートコードの場合、データ取得はスキップ
    repeat = 1;
  } else {
     // データ部取得
     // |~~~~~~~|____________________|
     //  0.56ms   0:0.56ms or 1:1.69ms
     // 0N->OFF がリダーコードの場合、データを取得
     for (uint8_t i = 0; i <32; i++) {  //32ビット分取得ループ
        // ビット開始待ち
        while(!digitalRead(pinNo));     // OFF待ち  
        t = micros();
        while(digitalRead(pinNo));      // OFFの間待ち  
        t = micros() -t;
        if (t>RC_TMOVER)  // LOWの長さ異常
          return 3;       // エラー
        dt<<=1;
        dt |= (t<RC_BITLOW_TS) ? 0:1; // LOWの長さによるビット値判定
    }
  }

  // ストップビットの待ち
  // |~~~~~~~|__
  //  0.56ms     
  while(!digitalRead(pinNo));   // ON間待ち
  return dt;
}
