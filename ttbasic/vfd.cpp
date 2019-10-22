//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// MW25616L VFDディスプレイ(オプション) 機能
// 2019/06/08 by たま吉さん 
//
// 追加コマンド
//   VMSG ウェイト,メッセージ文字列
//   VPUT 仮想アドレス,バイト数,ウェイト
//   VCLS
//   VSCROLL スクロール量,ウェイト
//   VBRIGHT 輝度
//   VDISPLAY モード
//

#include "Arduino.h"
#include "basic.h"

#if USE_CMD_VFD == 1
  #include "src/lib/MW25616L.h" // MW25616L VFDディスプレイ
  MW25616L VFD;

void VFD_init() {
    VFD.begin();       // VFDドライバ
}
#endif

// MSG tm,..
// VFDメッセージ表示
void ivmsg(uint8_t flgZ) {
#if USE_CMD_VFD == 1
  int16_t tm,lat=1;
  if ( getParam(tm, -1, 1024, true) )  return;
  if (tm == -1) {
    lat = 0;
    tm = 0;
  } 
  // メッセージ部をバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,CDEV_VFD);
  if (err)
    return;

  // メッセージ部の表示
  VFD.dispSentence((const uint8_t*)lbuf, tm,flgZ,lat);
#endif
}

// VPUT 仮想アドレス,バイト数,時間
//
void ivput() {
#if USE_CMD_VFD == 1
  int16_t vadr;
  uint8_t* adr;
  int16_t len;
  int16_t tm;

  if ( getParam(vadr, 0, 32767, true )  ||        // データアドレス
       getParam(len,  0, 32767, true )  ||        // データ長
       getParam(tm, - 1,  1024, false)            // 時間
   )  return;       

  // 実アドレス取得
  adr  = v2realAddr(vadr);
  if (adr == NULL || v2realAddr(vadr+len) == NULL) {
     err = ERR_VALUE;
     return;    
  }
  if (tm>0)
     VFD.mw_put_s(adr,len,tm);
   else {
     VFD.mw_put_t(adr,len);
     if (tm==0)
        VFD.wr_lat();
   }
#endif
}

// VCLS
// VFD画面クリア
void ivcls() {
#if USE_CMD_VFD == 1
  VFD.mw_clr();
#endif
}

// VSCROLL w,tm
// VFDスクロール
void ivscroll() {
#if USE_CMD_VFD == 1
  int16_t w,tm;
  if ( getParam(w, 1, 256, true) )  return;
  if ( getParam(tm,-1,1024,false) ) return;  

  // スクロール表示
  if (tm>0) {
    VFD.disp_non_t(w, tm);
  } else {
    VFD.disp_non(w);
    if (tm==0)
      VFD.wr_lat();
  }
#endif
}

// VBRIGHT level
// VFD輝度設定
void ivbright() {
#if USE_CMD_VFD == 1
  int16_t level;
  if ( getParam(level,0,255,false) ) return;    
  VFD.setBright(level);
#endif
}

// VDISPLAY sw
// VFD表示設定
void ivdisplay() {
#if USE_CMD_VFD == 1
  int16_t sw;
  if ( getParam(sw,0,1,false) ) return;    
  VFD.display(sw);  
#endif
}
