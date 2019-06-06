#include "Arduino.h"
#include "basic.h"

// **** PALYコマンドのサポート（オプション） ***
#if USE_CMD_PLAY == 1
  #include "src/lib/MML.h"
  MML mml;             // MML文演奏管理

  // デバイス初期化関数
  void dev_toneInit() {
  }

  // デバッグ出力用
  void dev_debug(uint8_t c) {
     c_putch(c);
  }
#endif

// 単音出力関数
void dev_tone(uint16_t freq, uint16_t tm, uint16_t vol=0) {
  tone(TonePin,freq);
  if (tm) {
    delay(tm);
    noTone(TonePin);
  }
}

// 単音出力停止関数
void dev_notone() {
  noTone(TonePin);
}

// TONE 周波数 [,音出し時間]
void itone() {
  int16_t freq;   // 周波数
  int16_t tm = 0; // 音出し時間

  if ( getParam(freq, 0, 32767, false) ) return;
  if(*cip == I_COMMA) {
    cip++;
    if ( getParam(tm, 0, 32767, false) ) return;
  }
  dev_tone(freq, tm);
}

//　NOTONE
void inotone() {
  dev_notone();
}

// *** サウンド(Tone/PLAY) *************************
#if USE_CMD_PLAY == 1

// TEMPO テンポ
void itempo() {
  int16_t tempo;  
  if ( getParam(tempo, 32, 500, false) ) return; // テンポの取得
  mml.tempo(tempo);
}

// PLAY 文字列
void iplay() {
  // 引数のMMLをバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,1);
  if (err)
    return;
  uint8_t* ptr = lbuf; // MML文格納アドレス先頭
  mml.setText((char *)ptr);
  mml.playBGM();

  while (mml.isBGMPlay()) {
    if (mml.available()) 
      mml.playTick();  
    //強制的な中断の判定
    if (isBreak()) {
      mml.stop();
      break;
    }  
  }
  if (mml.isError()) {
    err = ERR_PLAY_MML;
  }
}

void mml_init() {
  mml.init(dev_toneInit, dev_tone, dev_notone, dev_debug);
}
#endif
