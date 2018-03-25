/*
 (C)2012 Tetsuya Suzuki
 GNU General Public License
 */

//
//  2018/02/11 SRAM節約修正&機能拡張 by たま吉さん
//   (キーワード、エラーメッセージをフラッシュメモリに配置)
//  修正 2018/02/14 「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
//  修正 2018/02/15 EDITコマンドでスクリーンエディタON/OFF設定対応
//  修正 2018/02/16 SAVE,LOAD,FILESの高速化(EEPROMクラスライブラリ利用からavr/eepromライブラリ利用に変更)
//  修正 2018/02/17 EDIT OFF時の行編集時の改善(CLS不具合,全角文字削除対応)
//  修正 2018/02/21 tTermscreenを使わないコンパイルを可能に修正(フラッシュメモリ節約)
//  修正 2018/02/22 ラインエディタで[TAB],[UP][DOWN][↑][↓][←][→][F1][F2][DEL][BS][HOME][END]キー対応
//  修正 2018/02/24 TEMPOコマンドの追加
//  修正 2018/02/24 RTC DS323対応コマンドの追加、MAP(),GRADE()等関数の追加
//  修正 2018/02/28 I2C EEPROM プログラム保存対応
//  修正 2018/03/01 WSTR$()とCHR$()を統合,WASC()とASC()を統合,WSTR$()とSTR$()を統合
//  修正 2018/03/01 スクリーンエディタの廃止
//  修正 2018/03/13 ABS関数で-32768はOverflowエラーに修正,整数値の有効範囲修正(toktoi()の修正)
//  修正 2018/03/13 WLEN()をLEN()に変更,LEN()をBYTE()に変更
//  修正 2018/03/19 16進数定数の不具合修正
//  修正 2018/03/19 IR()関数の追加(NEC方式 赤外線リモコン受信)
//  修正 2018/03/21 Arduino Mega2560暫定対応
//  修正 2018/03/23 全角判定不具合修正(isJMS()で厳密判定)
//  修正 2018/03/24 Arduino Mega2560対応
//

#include <Arduino.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

//*** 機器依存識別 ****************************
#define STR_EDITION "ARDUINO"
#define STR_VARSION "Extended version 0.06"
#define MYSIGN      "TBU5" // EPPROM識別用シグニチャ

//*** デバッグ用 *******************************
#if MYDEBUG == 1
#include <SoftwareSerial.h>
SoftwareSerial SSerial(10, 11); 
#endif

//*** 環境設定ファイル **************************
#include "ttconfig.h"          

#if USE_SO1602AWWB == 1
//*** I2C接続キャラクタディスプレイ **************
#include "src/lib/SO1602AWWB.h"
#endif 

//*** プロトタイプ宣言 **************************
short iexp(void);
char* getParamFname();
int16_t getNextLineNo(int16_t lineno);
void mem_putch(uint8_t c);
void clearlbuf() ;
void putHexnum(int16_t value, uint8_t d, uint8_t devno);
void igetTime();
char* getLineStr(int16_t lineno);
void iprint(uint8_t devno=0,uint8_t nonewln=0);
void idelete(); 

#if USE_IR == 1
uint32_t Read_IR(uint8_t pinNo,uint16_t tm) ;
#endif

//***  TOYOSHIKI TinyBASIC 利用ワークメモリサイズ等定義 ***
#define LINELEN   64          // 1行の文字数
#define SIZE_LINE 64          // コマンドラインテキスト有効長さ
#define SIZE_IBUF 64          // 行当たりの中間コード有効サイズ
#define SIZE_LIST PRGAREASIZE // BASICプログラム領域サイズ
#define SIZE_ARRY ARRYSIZE    // 配列利用可能数 @(0)～@(定義数-1)
#define SIZE_GSTK 6           // GOSUB stack size(2/nest)
#define SIZE_LSTK 15          // FOR stack size(5/nest)

//*** シリアルコンソール画面制御用 *****************

// mcursesライブラリ(修正版)
// オリジナル版を修正して組み込み：https://github.com/ChrisMicro/mcurses
#include "src/lib/mcurses.h"   

uint8_t flgCurs = 0;
// シリアル経由1文字出力(mcursesから利用）
void Arduino_putchar(uint8_t c) {
  Serial.write(c);
}

// シリアル経由1文字入力(mcursesから利用）
char Arduino_getchar() {
  while (!Serial.available());
  return Serial.read();
}

// カーソル表示制御
inline void c_show_curs(uint8_t mode) {
    flgCurs = mode;
    curs_set(mode);
}

// キャラクタ入出力デバイス選択定義 
#define CDEV_SCREEN   0  // メインスクリーン
#define CDEV_CLCD     2  // キャラクターLCD/OLCDディスプレイ
#define CDEV_MEMORY   3  // メモリー

// 指定デバイスへの文字の出力
//  c     : 出力文字
//  devno : デバイス番号 0:メインスクリーン 3:、メモリ
void c_putch(uint8_t c, uint8_t devno = CDEV_SCREEN) {
  if (devno == CDEV_SCREEN ) {
     addch(c);
  } else if (devno == CDEV_MEMORY) {
     mem_putch(c); // メモリーへの文字列出力
  } 
#if USE_SO1602AWWB == 1
  else if (devno == CDEV_CLCD) {
    OLEDputch(c);  // OLEDキャラクタディスプレイへの文字列出力
  }
#endif
} 

//  改行
//  devno : デバイス番号 0:メインスクリーン 3:、メモリ
void newline(uint8_t devno=CDEV_SCREEN) {
 if (devno== CDEV_SCREEN ) {
    int16_t x,y;
    getyx(y,x);
    if (y>=MCURSES_LINES-1) {
      scroll();
      move(y,0);
    } else {
      move(y+1,0);  
    }
  } else if (devno == CDEV_MEMORY ) {
    mem_putch('\n'); // メモリーへの文字列出力
  }
}

// 出力制御他版との互換用
#define c_getch() getch()
#define c_kbhit() (Serial.available()?getch():0)
#define c_getHeight()  MCURSES_LINES
#define c_getWidth()   MCURSES_COLS
#define c_beep()       addch(0x07)
#define c_IsCurs()     flgCurs

// **** 仮想メモリ定義 ****************************
#define V_VRAM_TOP  0x0000    // VRAM領域先頭
#define V_VAR_TOP   0x1900    // 変数領域先頭
#define V_ARRAY_TOP 0x1AA0    // 配列領域先頭
#define V_PRG_TOP   0x1BA0    // プログラム領域先頭
#define V_MEM_TOP   0x2BA0    // ユーザー作業領域先頭

// 定数
#define CONST_HIGH   1   // HIGH
#define CONST_LOW    0   // LOW
#define CONST_ON     1   // ON
#define CONST_OFF    0   // OFF
#define CONST_LSB    0   // LSB
#define CONST_MSB    1   // MSB

// *** MW25616L VFDディスプレイ(オプション)**********
#if USE_CMD_VFD == 1
  #include "src/lib/MW25616L.h" // MW25616L VFDディスプレイ
  MW25616L VFD;         // VFDドライバ
#endif

// *** 内部EEPROMフラッシュメモリ管理 ***************
#include <avr/eeprom.h>
#ifdef ARDUINO_AVR_MEGA2560
  #if PRGAREASIZE <= 512
    #define EEPROM_PAGE_NUM         8      // 全ページ数
    #define EEPROM_PAGE_SIZE        512    // ページ内バイト数
    #define EEPROM_SAVE_NUM         8      // プログラム保存可能数
  #elif PRGAREASIZE <= 1024
    #define EEPROM_PAGE_NUM         4      // 全ページ数
    #define EEPROM_PAGE_SIZE        1024   // ページ内バイト数
    #define EEPROM_SAVE_NUM         5      // プログラム保存可能数
  #elif PRGAREASIZE <= 2048
    #define EEPROM_PAGE_NUM         2      // 全ページ数
    #define EEPROM_PAGE_SIZE        2048   // ページ内バイト数
    #define EEPROM_SAVE_NUM         2      // プログラム保存可能数
  #elif PRGAREASIZE <= 4096
    #define EEPROM_PAGE_NUM         1      // 全ページ数
    #define EEPROM_PAGE_SIZE        4096   // ページ内バイト数
    #define EEPROM_SAVE_NUM         1      // プログラム保存可能数
  #endif
#else
  #if PRGAREASIZE <= 512
    #define EEPROM_PAGE_NUM         2      // 全ページ数
    #define EEPROM_PAGE_SIZE        512    // ページ内バイト数
    #define EEPROM_SAVE_NUM         2      // プログラム保存可能数
  #elif PRGAREASIZE <= 1024
    #define EEPROM_PAGE_NUM         1      // 全ページ数
    #define EEPROM_PAGE_SIZE        1024   // ページ内バイト数
    #define EEPROM_SAVE_NUM         1      // プログラム保存可能数
  #endif
#endif

// **** GPIOピンに関する定義 ***********************
// ピン数の定義
#ifdef ARDUINO_AVR_MEGA2560
 #define TT_MAX_PINNUM 69
#else
 #define TT_MAX_PINNUM 21
#endif 

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
  int16_t ii2cw();
  int16_t ii2cr();
#endif 

// **** I2C外部接続EPPROMライブラリの利用設定(オプション) ***
#if USE_CMD_I2C == 1 && USE_I2CEEPROM == 1
  #include "src/lib/TI2CEPPROM.h"
  TI2CEPPROM rom(0x50);
#endif

// *** 中間コード・キーワード定義 **************************

// キーワード定義マクロ(k:キーワード用変数名 、s:キーワード文字列)
#define KW(k,s) const char k[] PROGMEM=s  

// キーワード定義(AVR SRAM消費軽減対策:フラシュメモリに配置）
KW(k000,"GOTO"); KW(k001,"GOSUB"); KW(k002,"RETURN"); KW(k069,"END");
KW(k003,"FOR"); KW(k004,"TO"); KW(k005,"STEP"); KW(k006,"NEXT");
KW(k007,"IF"); KW(k068,"ELSE"); KW(k008,"REM");
KW(k010,"INPUT"); KW(k011,"PRINT"); KW(k042,"?"); KW(k012,"LET");
KW(k013,","); KW(k014,";"); KW(k036,":"); KW(k070,"\'");
KW(k015,"-"); KW(k016,"+"); KW(k017,"*"); KW(k018,"/"); KW(k019,"("); KW(k020,")");KW(k035,"$"); ;KW(k114,"`");
KW(k021,">=");KW(k022,"#"); KW(k023,">"); KW(k024,"="); KW(k025,"<="); KW(k026,"<");
KW(k056,"!");KW(k057,"~"); KW(k058,"%");KW(k059,"<<");KW(k060,">>");KW(k061,"|");KW(k062,"&");KW(k063,"AND");KW(k064,"OR");KW(k065,"^");
KW(k066,"!="); KW(k067,"<>");
KW(k027,"@"); KW(k028,"RND"); KW(k029,"ABS"); KW(k030,"FREE");
KW(k031,"LIST"); KW(k032,"RUN"); KW(k033,"NEW"); KW(k034,"CLS"); KW(k142,"DELETE"); 
#if USE_CMD_VFD == 1
KW(k037,"VMSG");KW(k038,"VCLS"); KW(k039,"VSCROLL"); KW(k040,"VBRIGHT"); KW(k041,"VDISPLAY"); KW(k106,"VPUT"); 
#endif
KW(k043,"SAVE"); KW(k044,"LOAD"); KW(k045,"FILES"); KW(k046,"ERASE"); KW(k047,"WAIT");
KW(k048,"CHR$"); KW(k050,"HEX$"); KW(k051,"BIN$"); KW(k072,"STR$");
KW(k074,"BYTE"); KW(k075,"LEN"); KW(k076,"ASC");
KW(k052,"COLOR"); KW(k053,"ATTR"); KW(k054,"LOCATE"); KW(k055,"INKEY");
KW(k078,"GPIO"); KW(k079,"OUT"); KW(k080,"POUT");
KW(k081,"OUTPUT"); KW(k082,"INPUT_PD"); KW(k083,"INPUT_FL");
KW(k084,"OFF"); KW(k085,"ON"); KW(k086,"IN"); KW(k087,"ANA"); KW(k088,"LOW"); KW(k089,"HIGH");
KW(k090,"RENUM");
KW(k091,"TONE"); KW(k092,"NOTONE");
#if USE_CMD_PLAY == 1
KW(k093,"PLAY"); KW(k107,"TEMPO");
#endif
KW(k094,"SYSINFO");
KW(k095,"MEM");/* KW(k096,"VRAM");*/ KW(k097,"VAR"); KW(k098,"ARRAY"); KW(k099,"PRG");
KW(k100,"PEEK"); KW(k101,"POKE"); KW(k102,"I2CW"); KW(k103,"I2CR"); KW(k104,"TICK"); 
KW(k108,"MAP"); KW(k109,"GRADE"); KW(k110,"SHIFTOUT"); KW(k111,"PULSEIN"); ; KW(k112,"DMP$");KW(k113,"SHIFTIN");

// キーボードコード
KW(k115,"KUP"); KW(k116,"KDOWN"); KW(k117,"KRIGHT");KW(k118,"KLEFT"); KW(k119,"KSPACE");KW(k120,"KENTER"); 
KW(k121,"LSB"); KW(k122,"MSB"); KW(k123,"CW"); KW(k124,"CH");

#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
KW(k125,"DATE"); KW(k126,"GETDATE"); KW(k127,"GETTIME"); KW(k128,"SETDATE"); KW(k129,"DATE$");  // RTC関連コマンド(5)
#endif
KW(k134,"FORMAT"); KW(k141,"DRIVE");
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
KW(k135,"CPRINT");KW(k136,"CCLS");KW(k137,"CCURS");KW(k138,"CLOCATE");KW(k139,"CCONS");KW(k140,"CDISP");
#endif
#if USE_ANADEF == 1
KW(k143,"A0");KW(k144,"A1");KW(k145,"A2");KW(k146,"A3");KW(k147,"A4");KW(k148,"A5");KW(k149,"A6");KW(k150,"A7");
  #ifdef ARDUINO_AVR_MEGA2560
    KW(k151,"A8");KW(k152,"A9");KW(k153,"A10");KW(k154,"A11");KW(k155,"A12");KW(k156,"A13");KW(k157,"A14");KW(k158,"A15");
  #endif
#endif
#if USE_IR == 1
KW(k159,"IR");
#endif
KW(k071,"OK");

// キーワードテーブル(並び位置が中間コードになることに注意）
const char*  const kwtbl[] PROGMEM = {
  k000,k001,k002,k069,                               // "GOTO","GOSUB,"RETURN","END"
  k003,k004,k005,k006,                               // "FOR","TO","STEP","NEXT"
  k007,k068,k008,                                    // "IF","ELSE","REM"
  k010,k011,k042,k012,                               // "INPUT","PRINT","?","LET"
  k013,k014,k036,k070,                               //",",";",":","\'"
  k015,k016,k017,k018,k019,k020,k035,k114,           // "-","+","*","/","(",")","$","`"
  k021,k022,k023,k024,k025,k026,                     // ">=","#",">","=","<=","<"
  k056,k057,k058,k059,k060,k061,k062,k063,k064,k065, // "!","~","%","<<",">>","|","&","AND","OR","^"
  k066,k067,                                         // "!=","<>"
  k027,k028,k029,k030,                               // "@","RND","ABS","FREE"
  k031,k032,k033,k034,k142,                          // "LIST","RUN","NEW","CLS","DELETE"
#if USE_CMD_VFD == 1
  k037,k038,k039,k040,k041,k106,                     // "VMSG","VCLS","VSCROLL","VBRIGHT","VDISPLAY","VPUT"
#endif
  k043,k044,k045,k046,k047,                          // "SAVE","LOAD","FILES","ERASE","WAIT"
  k048,k050,k051,k072,                               // "CHR$","HEX$","BIN$","STR$","WSTR$"
  k074,k075,k076,                                    // "BYTE","LEN","ASC","WASC"
  k052,k053,k054,k055,                               // "COLOR","ATTR","LOCATE","INKEY"
  k078,k079,k080,                                    // "GPIO","OUT","POUT"
  k081,k082,k083,                                    // "OUTPUT","INPUT_PD","INPUT_FL"
  k084,k085,k086,k087,k088,k089,                     // "OFF","ON","IN","ANA","LOW","HIGH"
  k090,                                              // "RENUM"
  k091,k092,                                         // "TONE","NOTONE"
#if USE_CMD_PLAY == 1
  k093, k107,                                        // "PLAY","TEMPO"
#endif
  k094,                                              // "SYSINFO"
  k095,/*k096,*/k097,k098,k099,                      // "MEM","VRAM","VAR","ARRAY","PRG"
  k100,k101,k102,k103,k104,                          // "PEEK","POKE","I2CW","I2CR","TICK"
  k108,k109,k110,k111,k112,k113,                     // "MAP","GRADE","SHIFTOUT","PULSEIN","DMP$","SHIFTIN"
  k115,k116,k117,k118,k119,k120,                     // "KUP","KDOWN","KRIGHT","KLEFT","KSPACE","KENTER"
  k121,k122,k123,k124,                               // "LSB","MSB","CW","CH"
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
  k125,k126,k127,k128, k129,                         // "DATE","GETDATE","GETTIME","SETDATE","DATE$"
#endif

  k134,k141,                                         // "FORMAT","DRIVE"
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
  k135,k136,k137,k138,k139,k140,                     // "CPRINT","CCLS","CCURS","CLOCATE","CCONS","CDISP";  
#endif
#if USE_ANADEF == 1
  k143,k144,k145,k146,k147,k148,k149,k150,           // "A0","A1","A2","A3","A4","A5","A6","A7",
  #ifdef ARDUINO_AVR_MEGA2560
    k151,k152,k153,k154,k155,k156,k157,k158,         // "A8","A9","A10","A11","A12","A13","A14","A15",
  #endif
#endif
#if USE_IR == 1
  k159,                                              // "IR"
#endif
  k071,                                              // "OK"
};

// Keyword count
#define SIZE_KWTBL (sizeof(kwtbl) / sizeof(kwtbl[0]))

// i-code(Intermediate code) assignment
enum {
  I_GOTO, I_GOSUB, I_RETURN, I_END, 
  I_FOR, I_TO, I_STEP, I_NEXT,
  I_IF, I_ELSE, I_REM,
  I_INPUT, I_PRINT, I_QUEST, I_LET,
  I_COMMA, I_SEMI,I_COLON, I_SQUOT,
  I_MINUS, I_PLUS, I_MUL, I_DIV, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT,
  I_LNOT, I_BITREV,I_DIVR,I_LSHIFT, I_RSHIFT, I_OR, I_AND,I_LAND, I_LOR, I_XOR,
  I_NEQ, I_NEQ2,
  I_ARRAY, I_RND, I_ABS, I_SIZE,
  I_LIST, I_RUN, I_NEW, I_CLS, I_DELETE,
#if USE_CMD_VFD == 1
  I_VMSG, I_VCLS, I_VSCROLL, I_VBRIGHT, I_VDISPLAY, I_VPUT,
#endif
  I_SAVE, I_LOAD, I_FILES, I_ERASE, I_WAIT,
  I_CHR,  I_HEX, I_BIN, I_STRREF,
  I_BYTE, I_LEN, I_ASC,
  I_COLOR, I_ATTR, I_LOCATE, I_INKEY,
  I_GPIO, I_DOUT, I_POUT,
  I_OUTPUT, I_INPUT_PU, I_INPUT_FL,
  I_OFF, I_ON, I_DIN, I_ANA, I_LOW, I_HIGH,
  I_RENUM,
  I_TONE, I_NOTONE,
#if USE_CMD_PLAY == 1
  I_PLAY, I_TEMPO,
#endif
  I_SYSINFO,
  I_MEM, /*I_VRAM,*/ I_MVAR, I_MARRAY,I_MPRG,
  I_PEEK, I_POKE, I_I2CW, I_I2CR, I_TICK,
  I_MAP, I_GRADE, I_SHIFTOUT, I_PULSEIN, I_DMP, I_SHIFTIN,
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_LSB, I_MSB,I_CW, I_CH,
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
  I_DATE, I_GETDATE, I_GETTIME, I_SETDATE, I_DATESTR,  // RTC関連コマンド(5)  
#endif 
  I_FORMAT, I_DRIVE,
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
  I_CPRINT, I_CCLS, I_CCURS, I_CLOCATE, I_CCONS, I_CDISP,
#endif
#if USE_ANADEF == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif  
#endif
#if USE_IR == 1
  I_IR,
#endif
  I_OK, 
  I_NUM, I_VAR, I_STR, I_HEXNUM, I_BINNUM,
  I_EOL
};

// List formatting condition
// 後ろに空白を入れない中間コード
const PROGMEM unsigned char i_nsa[] = {
  I_RETURN, I_COMMA, I_SEMI, I_COLON,I_END,
  I_MINUS, I_PLUS, I_MUL, I_DIV,  I_DIVR, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,
  I_LSHIFT, I_RSHIFT, I_OR, I_AND, I_NEQ, I_NEQ2, I_XOR,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT, I_LNOT, I_BITREV, I_DIVR,
  I_ARRAY, I_RND, I_ABS, I_SIZE,I_CLS, I_QUEST,
  I_CHR, I_HEX, I_BIN,I_STRREF,
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
  I_DATESTR,
#endif
#if USE_CMD_VFD == 1
  I_VCLS,
#endif
  I_INKEY,
  I_LEN, I_BYTE, I_ASC,
  I_OUTPUT, I_INPUT_PU,I_INPUT_FL,
  I_OFF, I_ON, I_DIN, I_ANA,
  I_SYSINFO,
  I_MEM, /*I_VRAM,*/ I_MVAR, I_MARRAY,I_MPRG,
  I_PEEK, I_I2CW, I_I2CR, I_TICK,
  I_MAP, I_GRADE, I_SHIFTIN, I_PULSEIN, I_DMP,
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_LSB, I_MSB,I_CW, I_CH,  
#if USE_ANADEF == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif  
#endif
#if USE_IR == 1
  I_IR,
#endif
};

// 前が定数か変数のとき前の空白をなくす中間コード
const PROGMEM unsigned char i_nsb[] = {
  I_RETURN, I_COMMA,I_SEMI, I_COLON,I_SQUOT,
  I_MINUS, I_PLUS, I_MUL, I_DIV,  I_DIVR,I_OPEN, I_CLOSE,I_LSHIFT, I_RSHIFT, I_OR, I_AND,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE,  I_NEQ, I_NEQ2,I_LT, I_LNOT, I_BITREV, I_XOR,
  I_ARRAY, I_RND, I_ABS, I_SIZE  
};

// 必ず前に空白を入れる中間コード
const PROGMEM unsigned char i_sf[]  = {
  I_ATTR, I_CLS, I_COLOR, I_END, I_FILES, I_TO, I_STEP,I_QUEST,I_LAND, I_LOR,
  I_GOSUB,I_GOTO,I_INPUT,I_LET,I_LIST,I_ELSE,I_RENUM,
  I_LOAD,I_LOCATE,I_NEW,I_PRINT,
  I_RETURN,I_RUN,I_SAVE,I_WAIT,
#if USE_CMD_VFD == 1
  I_VMSG, I_VPUT,
#endif
  I_GPIO, I_DOUT,I_RENUM,
  I_TONE, I_NOTONE,I_SYSINFO,I_POKE,
#if USE_CMD_PLAY == 1  
  I_PLAY, I_TEMPO,
#endif
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
  I_DATE, I_GETDATE, I_GETTIME, I_SETDATE,   // RTC関連コマンド(4)  
#endif 
  I_FORMAT,I_DRIVE,
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
  I_CPRINT, I_CCLS, I_CCURS, I_CLOCATE, I_CCONS, I_CDISP,  
#endif
};

// 後ろが変数、数値、定数の場合、後ろに空白を空ける中間コード
const PROGMEM unsigned char i_sb_if_value[] = {
  I_NUM, I_STR, I_HEXNUM, I_VAR, I_BINNUM,
  I_OFF, I_ON, I_MEM,/* I_VRAM,*/ I_MVAR, I_MARRAY,I_MPRG,
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_LSB, I_MSB,I_CW, I_CH,  
#if USE_ANADEF == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif 
#endif
};

// exception search function
char sstyle(unsigned char code,
  const unsigned char *table, unsigned char count) {
  while(count--) //中間コードの数だけ繰り返す
    if (code == pgm_read_byte_near(&table[count])) //もし該当の中間コードがあったら
      return 1; //1を持ち帰る
  return 0; //（なければ）0を持ち帰る
}

// exception search macro
#define nospacea(c) sstyle(c, i_nsa, sizeof(i_nsa))
#define nospaceb(c) sstyle(c, i_nsb, sizeof(i_nsb))
#define spacef(c) sstyle(c, i_sf, sizeof(i_sf))                           // 必ず前に空白を入れる中間コードか？
#define spacebifValue(c) sstyle(c, i_sb_if_value, sizeof(i_sb_if_value))  // 後ろが変数、数値、定数の場合、後ろに空白を空ける中間コードか


// ***エラーメッセージ定義 **************************
// メッセージ(フラッシュメモリ配置)
uint8_t err;           // Error message index
int16_t errorLine = -1;  // 直前のエラー発生行番号
KW(e00,"OK");
KW(e01,"Devision by zero");
KW(e02,"Overflow");
KW(e03,"Subscript out of range");
KW(e04,"Icode buffer full");
KW(e05,"List full");
KW(e06,"GOSUB too many nested");
KW(e07,"RETURN stack underflow");
KW(e08,"FOR too many nested");
KW(e09,"NEXT without FOR");
KW(e10,"NEXT without counter");
KW(e11,"NEXT mismatch FOR");
KW(e12,"FOR without variable");
KW(e13,"FOR without TO");
KW(e14,"LET without variable");
KW(e15,"IF without condition");
KW(e16,"Undefined line number or label");
KW(e17,"\'(\' or \')\' expected");
KW(e18,"\'=\' expected");
KW(e19,"Illegal command");
KW(e20,"Syntax error");
KW(e21,"Internal error");
KW(e22,"Break");
KW(e23,"Illegal value");
KW(e24,"Cannot use GPIO function");
#if USE_CMD_PLAY == 1
KW(e25,"Illegal MML");
#endif
KW(e26,"I2C Device Error");
KW(e27,"Bad filename");
KW(e28,"Device full");

// エラーメッセージテーブル
const char*  const errmsg[] PROGMEM = {
  e00,e01,e02,e03,e04,e05,e06,e07,e08,e09,
  e10,e11,e12,e13,e14,e15,e16,e17,e18,e19,  
  e20,e21,e22,e23,e24,
#if USE_CMD_PLAY == 1
  e25,
#endif
  e26,e27,e28,
};

// エラーコードテーブル
enum {
  ERR_OK,
  ERR_DIVBY0,
  ERR_VOF,
  ERR_SOR,
  ERR_IBUFOF, ERR_LBUFOF,
  ERR_GSTKOF, ERR_GSTKUF,
  ERR_LSTKOF, ERR_LSTKUF,
  ERR_NEXTWOV, ERR_NEXTUM, ERR_FORWOV, ERR_FORWOTO,
  ERR_LETWOV, ERR_IFWOC,
  ERR_ULN,
  ERR_PAREN, ERR_VWOEQ,
  ERR_COM,
  ERR_SYNTAX,
  ERR_SYS,
  ERR_CTR_C,
  ERR_VALUE,
  ERR_GPIO,
  ERR_MML,
  ERR_I2CDEV,
  ERR_FNAME,
  ERR_NOFSPACE,
};

// RAM mapping
char lbuf[SIZE_LINE];              // Command line buffer
int16_t lbuf_pos = 0;
unsigned char ibuf[SIZE_IBUF];     // i-code conversion buffer
short var[26];                     // Variable area
short arr[SIZE_ARRY];              // Array area
unsigned char listbuf[SIZE_LIST];  // List area
unsigned char* clp;                // Pointer current line
unsigned char* cip;                // Pointer current Intermediate code
unsigned char* gstk[SIZE_GSTK];    // GOSUB stack
unsigned char gstki;               // GOSUB stack index
unsigned char* lstk[SIZE_LSTK];    // FOR stack
unsigned char lstki;               // FOR stack index

uint8_t prevPressKey = 0;          // 直前入力キーの値(INKEY()、[ESC]中断キー競合防止用)

// Standard C libraly (about) same functions
char c_toupper(char c) {
  return(c <= 'z' && c >= 'a' ? c - 32 : c);
}
char c_isprint(char c) {
  //return(c >= 32 && c <= 126);
  return (c);
}
char c_isspace(char c) {
  return(c == ' ' || (c <= 13 && c >= 9));
}
char c_isdigit(char c) {
  return(c <= '9' && c >= '0');
}
char c_isalpha(char c) {
  return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A'));
}

// 全角判定
inline uint8_t isZenkaku(uint8_t c){
   return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc))) ? 1:0;
}

// 文末空白文字のトリム処理
char* tlimR(char* str) {
  uint16_t len = strlen(str);
  for (uint16_t i = len - 1; i>0 ; i--) {
    if (str[i] == ' ') {
      str[i] = 0;
    } else {
      break;
    }
  }
  return str;
}

// コマンド引数取得(int16_t,引数チェックあり)
inline uint8_t getParam(int16_t& prm, int16_t v_min,int16_t v_max,uint8_t flgCmma) {
  prm = iexp(); 
  if (!err &&  (prm < v_min || prm > v_max)) 
    err = ERR_VALUE;
  else if (flgCmma && *cip++ != I_COMMA) {
    err = ERR_SYNTAX;
 }
  return err;
}

// コマンド引数取得(int16_t,引数チェックなし)
inline uint8_t getParam(uint16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(uint16_t,引数チェックなし)
inline uint8_t getParam(int16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(int32_t,引数チェックなし)
inline uint8_t getParam(int32_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// '('チェック関数
inline uint8_t checkOpen() {
  if (*cip != I_OPEN) err = ERR_PAREN;
  else cip++;
  return err;
}

// ')'チェック関数
inline uint8_t checkClose() {
  if (*cip != I_CLOSE) err = ERR_PAREN;
  else cip++;
  return err;
}

//強制的な中断の判定
uint8_t isBreak() {
  uint8_t c = c_kbhit();
  if (c) {
      if (c == KEY_CTRL_C || c==KEY_ESCAPE ) { // 読み込んでもし[ESC],［CTRL_C］キーだったら
        err = ERR_CTR_C;                       // エラー番号をセット
        prevPressKey = 0;
      } else {
        prevPressKey = c;
      }
   }
   return err;
}
// 仮想アドレスを実アドレスに変換
//  引数   :  vadr 仮想アドレス
//  戻り値 :  NULL以外 実アドレス、NULL 範囲外
//
uint8_t* v2realAddr(uint16_t vadr) {
  uint8_t* radr = NULL; 
  if ((vadr >= V_VAR_TOP) && (vadr < V_ARRAY_TOP)) {               // 変数領域
    radr = vadr-V_VAR_TOP+(uint8_t*)var;
  } else if ((vadr >= V_ARRAY_TOP) && (vadr < V_PRG_TOP)) {        // 配列領域
    radr = vadr - V_ARRAY_TOP+(uint8_t*)arr;
  } else if ((vadr >= V_PRG_TOP) && (vadr < V_MEM_TOP)) {          // プログラム領域
    radr = vadr - V_PRG_TOP + (uint8_t*)listbuf;
  } else if ((vadr >= V_MEM_TOP) && (vadr < V_MEM_TOP+LINELEN)) {  // ユーザーワーク領域
    radr = vadr - V_MEM_TOP + lbuf;    
  }
  return radr;
}

// 1桁16進数文字を整数に変換する
uint16_t hex2value(char c) {
  if (c <= '9' && c >= '0')
    c -= '0';
  else if (c <= 'f' && c >= 'a')
    c = c + 10 - 'a';
  else if (c <= 'F' && c >= 'A')
    c = c + 10 - 'A' ;
  return c;
}

// 文字列出力
inline void c_puts(const char *s, uint8_t devno=0) {
  while (*s) c_putch(*s++, devno); //終端でなければ出力して繰り返す
}

// PROGMEM参照バージョン
void c_puts_P(const char *s,uint8_t devno=0) {
  char c;
  while( c = pgm_read_byte(s++) ) {
    c_putch(c,devno); //終端でなければ出力して繰り返す
  }
}

// 16進文字出力 'HEX$(数値,桁数)' or 'HEX$(数値)'
void ihex(uint8_t devno=CDEV_SCREEN) {
  int16_t value; // 値
  int16_t d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,4,false)) return;  
  }
  if (checkClose()) return;  
  putHexnum(value, d, devno);    
}

// 2進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : 出力先デバイスコード
// 機能
// 'BBBBBBBBBBBBBBBB' B:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putBinnum(int16_t value, uint8_t d, uint8_t devno=0) {
  uint16_t  bin = (uint16_t)value; // 符号なし16進数として参照利用する
  uint16_t  b;                     // 指定ビット位置の値(0 or 1)
  uint16_t  dig = 1;               // 先頭が1から始まる桁数

  // 最初に1が現れる桁を求める
  for (uint8_t i=0; i < 16; i++) {
    if ( (0x8000>>i) & bin ) {
      dig = 15 - i;
      break;
    }
  }
  dig++;
  
  // 実際の桁数が指定表示桁数を超える場合は、実際の桁数を採用する
  if (d > dig) 
    dig = d;

  // ビット文字列の出力処理
  for (int8_t i=dig-1; i>=0; i--)
   c_putch((bin & (1<<i)) ? '1':'0', devno);

}

// 2進数出力 'BIN$(数値, 桁数)' or 'BIN$(数値)'
void ibin(uint8_t devno=CDEV_SCREEN) {
  int16_t value; // 値
  int16_t d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,16,false)) return;  
  }
  if (checkClose()) return;
  putBinnum(value, d, devno);    
}

// CHR$() 全角対応
void ichr(uint8_t devno=CDEV_SCREEN) {
  uint16_t value; // 値
  if (checkOpen()) return;
  for(;;) {
    if (getParam(value,false)) return;
    if (value <= 0xff) {
       c_putch(value, devno);
    } else {
       c_putch(value>>8,  devno);
       c_putch(value&0xff,devno);
    }
    if (*cip == I_COMMA) {
       cip++;
       continue;
    }
    break;
  }
  if (checkClose()) return;
}

// MAP(V,L1,H1,L2,H2)関数の処理
int16_t imap() {
  int32_t value,l1,h1,l2,h2,rc;
  if (checkOpen()) return 0;
  if ( getParam(value,true)||getParam(l1,true)||getParam(h1,true)||getParam(l2,true)||getParam(h2,false) ) 
    return 0;
  if (checkClose()) return 0;
  if (l1 >= h1 || l2 >= h2 || value < l1 || value > h1) {
    err = ERR_VALUE;
    return 0;
  }
  rc = (value-l1)*(h2-l2)/(h1-l1)+l2;
  return rc;  
}

#if USE_GRADE == 1
// 関数GRADE(値,配列番号,配列データ数)
int16_t igrade() {
  int16_t value,arrayIndex,len,rc=-1;

  if (checkOpen()) return 0;
  if ( getParam(value, true) )  return 0;
  if ( getParam(arrayIndex, 0, SIZE_ARRY-1, true) )  return 0;
  if ( getParam(len, 0, SIZE_ARRY, false) )  return 0;
  if ( checkClose()) return 0;    
  if ( arrayIndex+len > SIZE_ARRY ) {
    err = ERR_VALUE;
    return 0;    
  }
  for (uint16_t i=0; i < len; i++) {
    if (value >= arr[arrayIndex+i])  {
      rc = i;
      break;
    }
  }
  return rc;
}
#endif  

// ASC(文字列)
// ASC(文字列,文字位置)
// ASC(変数,文字位置)
int16_t iasc() {
  uint16_t value =0;
  int16_t len;     // 文字列長
  int16_t pos =1;  // 文字位置
  int16_t index;   // 配列添え字
  uint8_t* str;    // 文字列先頭位置
  
  if (checkOpen()) return 0;
  if ( *cip == I_STR) {  // 文字列定数の場合
     cip++;  len = *cip; // 文字列長の取得
     cip++;  str = cip;  // 文字列先頭の取得
     cip+=len;
  } else if ( *cip == I_VAR) {   // 変数の場合
     cip++;   str = v2realAddr(var[*cip]);
     len = *str;
     str++;
     cip++;     
  } else if ( *cip == I_ARRAY) { // 配列変数の場合
     cip++; 
     if (getParam(index, 0, SIZE_ARRY-1, false)) return 0;
     str = v2realAddr(arr[index]);
     len = *str;
     str++;
  } else {
    err = ERR_SYNTAX;
    return 0;
  }
  if ( *cip == I_COMMA) {
    cip++;
    if (getParam(pos,1,len,false)) return 0;
  }

  int16_t tmpPos = 0;
  int16_t i;
  for (i = 0; i < len; i++) {      
    if (pos == tmpPos+1)
      break;
    if (isZenkaku(str[i])) {
      i++;  
    }
    tmpPos++;
  }
  if (pos != tmpPos+1) {
    value = 0;
  } else {
    value = str[i];
    if(isZenkaku(str[i])) {
      value<<=8;
      value+= str[i+1];
    }
  }
  checkClose();
  return value;

}

// WLEN(文字列)
// 全角文字列長取得
int16_t iwlen(uint8_t flgZen=0) {
  int16_t len;     // 文字列長
  int16_t index;   // 配列添え字
  uint8_t* str;    // 文字列先頭位置
  int16_t wlen = 0;
  int16_t pos = 0;
  
  if (checkOpen()) 
    return 0;
    
  if ( *cip == I_VAR)  {
    // 変数の場合
     cip++;
     str = v2realAddr(var[*cip]);
     len = *str; // 文字列長の取得
     str++;      // 文字列先頭
     cip++;     
  } else if ( *cip == I_ARRAY) {
    // 配列変数の場合
     cip++; 
     if (getParam(index, 0, SIZE_ARRY-1, false)) return 0;
     str = v2realAddr(arr[index]);
     len = *str; // 文字列長の取得
     str++;      // 文字列先頭
  } else if ( *cip == I_STR) {
    // 文字列定数の場合
     cip++;  len = *cip; // 文字列長の取得
     cip++;  str = cip;  // 文字列先頭の取得
     cip+=len;
  } else {
    err = ERR_SYNTAX;
  }
  checkClose();
  if (flgZen) {
    // 文字列をスキャンし、長さを求める
    while(pos < len) {
      if (isZenkaku(*str)) {
        str++;
        pos++;  
      }
      wlen++;
      str++;
      pos++;
    }  
  } else {
    wlen = len;
  }
  return wlen;
}

// バッファ内の指定位置の文字を削除する
void line_delCharAt(uint8_t* str, uint8_t pos) {
  uint8_t len = strlen((char *)str); // 文字列長さ 
  if (pos == len-1) {
    // 行末の場合
     str[pos] = 0;
   } else {
    memmove(&str[pos], &str[pos+1],len-1-pos); 
    str[len-1] = 0;
  }
}

// バッファ内の指定位置に文字を挿入する
void line_insCharAt(uint8_t* str, uint8_t pos,uint8_t c) {
  uint8_t len = strlen(str); // 文字列長さ 
  if (len >= SIZE_LINE-1)
    return;
    
  if (pos == len) {
    // 行末の場合
     str[pos] = c;
     str[pos+1] = 0;
   } else {
    memmove(&str[pos+1], &str[pos],len-pos);
    str[pos] = c;
    str[len+1] = 0;
  }
}

// http://katsura-kotonoha.sakura.ne.jp/prog/c/tip00010.shtml
//*********************************************************
// 文字列 str の str[nPos] について、
//   ０ …… １バイト文字
//   １ …… ２バイト文字の一部（第１バイト）
//   ２ …… ２バイト文字の一部（第２バイト）
// のいずれかを返す。
//*********************************************************
#define jms1(c) (((0x81<=c)&&(c<=0x9F))||((0xE0<=c)&&(c<=0xFC))) 
#define jms2(c) ((0x7F!=c)&&(0x40<=c)&&(c<=0xFC))
int isJMS( uint8_t *str, uint16_t nPos ) {
  int i;
  int state; // { 0, 1, 2 }

  state = 0;
  for( i = 0; str[i] != '\0'; i++ ) {
    if      ( ( state == 0 ) && ( jms1( str[i] ) ) ) state = 1; // 0 -> 1
    else if ( ( state == 1 ) && ( jms2( str[i] ) ) ) state = 2; // 1 -> 2
    else if ( ( state == 2 ) && ( jms1( str[i] ) ) ) state = 1; // 2 -> 1
    else                                             state = 0; // 2 -> 0, その他
    // str[nPos] での状態を返す。
    if ( i == nPos ) return state;
  }
  return 0;
}

// カーソルを１文字前に移動
void line_movePrevChar(uint8_t* str, uint8_t ln, uint8_t& pos) {
  uint8_t len = strlen(str); // 文字列長さ 
  if (pos > 0) {
    pos--;
  }
//  if (pos > 0 && isZenkaku(lbuf[pos-1])) {
  if ( (pos-2 >= 0) && (isJMS(lbuf,pos-1) !=2) ) {
    // 全角文字の2バイト目の場合、全角1バイト目にカーソルを移動する
    pos--;
  } 
  move(ln,pos+1);
}

// カーソルを１文字次に移動
void line_moveNextChar(uint8_t* str, uint8_t ln, uint8_t& pos) {
  uint8_t len = strlen(str); // 文字列長さ 
  if (pos < len ) {
    pos++;
  }
  if (pos < len && isZenkaku(lbuf[pos-1])) {
    // 全角文字の2バイト目の場合、全角1バイト目にカーソルを移動する
    pos++;
  }
  move(ln,pos+1);  
}

// ラインエディタ内容の再表示
void line_redrawLine(uint8_t ln,uint8_t pos,uint8_t len) {
  move(ln,0);
  deleteln();
  c_putch('>');
  c_puts(lbuf);
  move(ln,pos+1);  
}

// Print numeric specified columns
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し),負の値字0埋め
//  devno : 出力先デバイスコード
// 機能
// 'SNNNNN' S:符号 N:数値 or 空白
//  dで桁指定時は空白補完する
//
void putnum(int16_t value, int16_t d, uint8_t devno=0) {
  uint8_t dig;  // 桁位置
  uint8_t sign; // 負号の有無（値を絶対値に変換した印）
  uint16_t new_value;
  char str[7];
  char c = ' ';
  if (d < 0) {
    d = -d;
    c = '0';
  }

  if (value < 0) {     // もし値が0未満なら
    sign = 1;          // 負号あり
    new_value = -value;
  } else {
    sign = 0;          // 負号なし
    new_value = value;
  }

  str[6] = 0;         // 終端を置く
  dig = 6;             // 桁位置の初期値を末尾に設定
  do { //次の処理をやってみる
    str[--dig] = (new_value % 10) + '0';  // 1の位を文字に変換して保存
    new_value /= 10;                      // 1桁落とす
  } while (new_value > 0);                // 値が0でなければ繰り返す

  if (sign) { //もし負号ありなら
    if (c == ' ') {
      str[--dig] = '-'; // 負号を保存
    } else {
      c_putch('-',devno);
      dig--;
    }
  }
  while (6 - dig < d) { // 指定の桁数を下回っていれば繰り返す
    c_putch(c,devno);   // 桁の不足を空白で埋める
    d--;                // 指定の桁数を1減らす
  }  
  c_puts(&str[dig],devno);   // 桁位置からバッファの文字列を表示
}

// 16進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : 出力先デバイスコード
// 機能
// 'XXXX' X:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putHexnum(int16_t value, uint8_t d, uint8_t devno=0) {
  uint16_t  hex = (uint16_t)value; // 符号なし16進数として参照利用する
  uint16_t  h;
  uint16_t dig;
  
  // 表示に必要な桁数を求める
  if (hex >= 0x1000) 
    dig = 4;
  else if (hex >= 0x100) 
    dig = 3;
  else if (hex >= 0x10) 
    dig = 2;
  else 
    dig = 1;

  if (d != 0 && d > dig) 
    dig = d;

  for (int8_t i = dig-1; i >= 0; i--) {
    h = (hex >>(i*4)) & 0x0f;
    c_putch((h >= 0 && h <= 9) ? h + '0': h + 'A' - 10, devno);
  }
}

// 数値の入力
int16_t getnum() {
  int16_t value, tmp; //値と計算過程の値
  char c; //文字
  uint8_t len; //文字数
  uint8_t sign; //負号
  uint8_t str[7];
  
  len = 0; //文字数をクリア
  for(;;) {
    c = c_getch();
    if (c == KEY_CR && len) {
        break;
    } else if (c == KEY_CTRL_C || c==KEY_ESCAPE) {
      err = ERR_CTR_C;
        break;
    } else 
    //［BackSpace］キーが押された場合の処理（行頭ではないこと）
    if (((c == KEY_BACKSPACE) || (c == KEY_DC)) && (len > 0)) {
      len--; //文字数を1減らす
      c_putch(KEY_BACKSPACE); c_putch(' '); c_putch(KEY_BACKSPACE); //文字を消す
    } else
    //行頭の符号および数字が入力された場合の処理（符号込みで6桁を超えないこと）
    if ((len == 0 && (c == '+' || c == '-')) ||
      (len < 6 && c_isdigit(c))) {
      str[len++] = c; //バッファへ入れて文字数を1増やす
      c_putch(c); //表示
    } else {      
      c_beep();
    }
  }
  newline(); //改行
  str[len] = 0; //終端を置く

  if (str[0] == '-') {
    sign = 1; //負の値
    len = 1;  //数字列はstr[1]以降    
  } else if (str[0] == '+') {
    sign = 0; //正の値
    len = 1;  //数字列はstr[1]以降
  } else {
    sign = 0; //正の値
    len = 0;  //数字列はstr[0]以降    
  }
 
  value = 0; // 値をクリア
  tmp = 0;   // 計算過程の値をクリア
  while (str[len]) { //終端でなければ繰り返す
    tmp = 10 * value + str[len++] - '0'; //数字を値に変換
    if (value > tmp) { // もし計算過程の値が前回より小さければ
      err = ERR_VOF;   // オーバーフローを記録
    }
    value = tmp; //計算過程の値を記録
  }

  if (sign) //もし負の値なら
    return -value; //負の値に変換して持ち帰る

  return value; //値を持ち帰る
}

// キーワード検索
//[戻り値]
//  該当なし   : -1
//  見つかった : キーワードコード
int16_t lookup(char* str, uint16_t len) {
  int16_t fd_id;
  int16_t prv_fd_id = -1;
  uint16_t fd_len,prv_len;
  char kwtbl_str[16]; // コマンド比較用
  
  for (uint16_t j = 1; j <= len; j++) {
    fd_id = -1;
    for (uint16_t i = 0; i < SIZE_KWTBL; i++) {
      strcpy_P(kwtbl_str, (char*)pgm_read_word(&(kwtbl[i]))); 
      if (!strncasecmp(kwtbl_str, str, j)) {
        fd_id = i;
        fd_len = j;        
        break;
      }
    }
    if (fd_id >= 0) {
      prv_fd_id = fd_id;
      prv_len = fd_len;
    } else {
      break;
    }
  }
  if (prv_fd_id >= 0) {
    prv_fd_id = -1;
    for (uint16_t i = 0; i < SIZE_KWTBL; i++) {
      strcpy_P(kwtbl_str, (char*)pgm_read_word(&(kwtbl[i]))); 
      if ( (strlen(kwtbl_str) == prv_len) && !strncasecmp(kwtbl_str, str, prv_len) ) {
        prv_fd_id = i;
        break;
      }
    }
  }
  return prv_fd_id;
}

// Convert token to i-code
// Return byte length or 0
unsigned char toktoi() {
  uint8_t i;              // ループカウンタ（一部の処理で中間コードに相当）
  int16_t key;
  uint8_t len = 0;        // 中間コードの並びの長さ
  char* pkw = 0;          // ひとつのキーワードの内部を指すポインタ
  char* ptok;             // ひとつの単語の内部を指すポインタ
  char* s = lbuf;         // 文字列バッファの内部を指すポインタ
  char c;                 // 文字列の括りに使われている文字（「"」または「'」）
  uint16_t value;         // 定数
  uint32_t tmp;           // 変換過程の定数
  uint16_t hex;           // 16進数定数
  uint16_t hcnt;          // 16進数桁数

  uint16_t bin;           // 2進数定数
  uint16_t bcnt;          // 2進数桁数
      
  while (*s) {                 // 文字列1行分の終端まで繰り返す
    while (c_isspace(*s)) s++; // 空白を読み飛ばす

    key = lookup(s, strlen(s));
    if (key >= 0) {    
      // 該当キーワードあり
      if (len >= SIZE_IBUF - 1) {      // もし中間コードが長すぎたら
        err = ERR_IBUFOF;              // エラー番号をセット
        return 0;                      // 0を持ち帰る
      }
      ibuf[len++] = key;               // 中間コードを記録

      s+= strlen_P((char*)pgm_read_word(&(kwtbl[key])));

    }
    
    // 16進数の変換を試みる $XXXX
    if (key == I_DOLLAR) {
      if (isHexadecimalDigit(*s)) {   // もし文字が16進数文字なら
        hex = 0;              // 定数をクリア
        hcnt = 0;             // 桁数
        do { //次の処理をやってみる
          hex = (hex<<4) | hex2value(*s++); // 数字を値に変換
          hcnt++;

        } while (isHexadecimalDigit(*s));   // 16進数文字がある限り繰り返す

        if (hcnt > 4) {      // 桁溢れチェック
          err = ERR_VOF;     // エラー番号オバーフローをセット
          return 0;          // 0を持ち帰る
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;                 // 0を持ち帰る
        }

        len--;    // I_DALLARを置き換えるために格納位置を移動
        ibuf[len++] = I_HEXNUM;  //中間コードを記録
        ibuf[len++] = hex & 255; //定数の下位バイトを記録
        ibuf[len++] = hex >> 8;  //定数の上位バイトを記録
      }      
    }

    // 2進数の変換を試みる $XXXX
    if (key == I_APOST) {
      if ( *s == '0'|| *s == '1' ) {    // もし文字が2進数文字なら
        bin = 0;              // 定数をクリア
        bcnt = 0;             // 桁数
        do { //次の処理をやってみる
          bin = (bin<<1) + (*s++)-'0' ; // 数字を値に変換
          bcnt++;
        } while ( *s == '0'|| *s == '1' ); //16進数文字がある限り繰り返す

        if (bcnt > 16) {      // 桁溢れチェック
          err = ERR_VOF;     // エラー番号オバーフローをセット
          return 0;          // 0を持ち帰る
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;                 // 0を持ち帰る
        }
        len--;    // I_APOSTを置き換えるために格納位置を移動
        ibuf[len++] = I_BINNUM;  // 中間コードを記録
        ibuf[len++] = bin & 255; // 定数の下位バイトを記録
        ibuf[len++] = bin >> 8;  // 定数の上位バイトを記録
      }      
    }
    
    //コメントへの変換を試みる
    if(key == I_REM|| key == I_SQUOT) {       // もし中間コードがI_REMなら
      while (c_isspace(*s)) s++; //空白を読み飛ばす
      ptok = s; //コメントの先頭を指す

      for (i = 0; *ptok++; i++);      // コメントの文字数を得る
      if (len >= SIZE_IBUF - 2 - i) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;             // エラー番号をセット
        return 0;                     // 0を持ち帰る
      }

      ibuf[len++] = i;      // コメントの文字数を記録
      while (i--) {         // コメントの文字数だけ繰り返す
        ibuf[len++] = *s++; // コメントを記録
      }
      break; //文字列の処理を打ち切る（終端の処理へ進む）
    }

   if (key >= 0)                            // もしすでにキーワードで変換に成功していたら以降はスキップ
     continue;

    //10進数定数への変換を試みる
    ptok = s;                            // 単語の先頭を指す
    if (c_isdigit(*ptok)) {              // もし文字が数字なら
      value = 0;                         // 定数をクリア
      tmp = 0;                           // 変換過程の定数をクリア
      do { //次の処理をやってみる
        tmp = 10 * value + *ptok++ - '0'; // 数字を値に変換
        if (tmp > 32768) {                // もし32768より大きければ
          err = ERR_VOF;                  // エラー番号をセット
          return 0;                       // 0を持ち帰る
        }      
        value = tmp; //0を持ち帰る
      } while (c_isdigit(*ptok)); //文字が数字である限り繰り返す

      if (len >= SIZE_IBUF - 3) { //もし中間コードが長すぎたら
        err = ERR_IBUFOF; //エラー番号をセット
        return 0; //0を持ち帰る
      }

      if ( (tmp == 32768) && (len > 0) && (ibuf[len-1] != I_MINUS)) {
        // valueが32768のオーバーフローエラー
        err = ERR_VOF;                  // エラー番号をセット
        return 0;                       // 0を持ち帰る
      }

      s = ptok; //文字列の処理ずみの部分を詰める
      ibuf[len++] = I_NUM;       // 中間コードを記録
      ibuf[len++] = value & 255; // 定数の下位バイトを記録
      ibuf[len++] = value >> 8;  // 定数の上位バイトを記録
    }
    else

    //文字列への変換を試みる
    if (*s == '\"') { //もし文字が「"」なら
      c = *s++; //「"」か「'」を記憶して次の文字へ進む
      ptok = s; //文字列の先頭を指す
      //文字列の文字数を得る
      for (i = 0; (*ptok != c) && c_isprint(*ptok); i++)
        ptok++;
      if (len >= SIZE_IBUF - 1 - i) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;             // エラー番号をセット
        return 0;                     // 0を持ち帰る
      }
      ibuf[len++] = I_STR;  // 中間コードを記録
      ibuf[len++] = i;      // 文字列の文字数を記録
      while (i--) {         // 文字列の文字数だけ繰り返す
        ibuf[len++] = *s++; // 文字列を記録
      }
      if (*s == c) s++; //もし文字が「"」なら次の文字へ進む
    }
    else

    //変数への変換を試みる
    if (c_isalpha(*ptok)) {       // もし文字がアルファベットなら
      if (len >= SIZE_IBUF - 2) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;         // エラー番号をセット
        return 0;                 // 0を持ち帰る
      }
    
      //もし変数が3個並んだら
      if (len >= 4 && ibuf[len - 2] == I_VAR && ibuf[len - 4] == I_VAR) {
        err = ERR_SYNTAX; // エラー番号をセット
        return 0;         // 0を持ち帰る
      }

      ibuf[len++] = I_VAR;                  // 中間コードを記録
      ibuf[len++] = c_toupper(*ptok) - 'A'; // 変数番号を記録
      s++;                                  // 次の文字へ進む
    }
    else

    // どれにも当てはまらなかった場合
    {
      err = ERR_SYNTAX; // エラー番号をセット
      return 0;         // 0を持ち帰る
    }
  } // 文字列1行分の終端まで繰り返すの末尾

  ibuf[len++] = I_EOL; // 文字列1行分の終端を記録
  return len;          // 中間コードの長さを持ち帰る
}

// Return free memory size
short getsize() {
  unsigned char* lp; //ポインタ

  for (lp = listbuf; *lp; lp += *lp); //ポインタをリストの末尾へ移動
  return listbuf + SIZE_LIST - lp - 1; //残りを計算して持ち帰る
}

// Get line numbere by line pointer
short getlineno(unsigned char *lp) {
  if(*lp == 0) //もし末尾だったら
    return -1;
  return *(lp + 1) | *(lp + 2) << 8; //行番号を持ち帰る
}


// ラベルでラインポインタを取得する
// pLabelは [I_STR][長さ][ラベル名] であること
uint8_t* getlpByLabel(uint8_t* pLabel) {
  uint8_t *lp; //ポインタ
  uint8_t len;
  pLabel++;
  len = *pLabel; // 長さ取得
  pLabel++;      // ラベル格納位置
  
  for (lp = listbuf; *lp; lp += *lp)  { //先頭から末尾まで繰り返す
    if ( *(lp+3) == I_STR ) {
       if (len == *(lp+4)) {
           if (strncmp((char*)pLabel, (char*)(lp+5), len) == 0) {
              return lp;
           }
       }
    }  
  }
  return NULL;
}

// Search line by line number
unsigned char* getlp(short lineno) {
  unsigned char *lp; //ポインタ

  for (lp = listbuf; *lp; lp += *lp) //先頭から末尾まで繰り返す
    if (getlineno(lp) >= lineno) //もし指定の行番号以上なら
      break; //繰り返しを打ち切る

  return lp; //ポインタを持ち帰る
}

// 行番号から行インデックスを取得する
uint16_t getlineIndex(uint16_t lineno) {
  uint8_t *lp; //ポインタ
  uint16_t index = 0;  
  uint16_t rc = 32767;
  for (lp = listbuf; *lp; lp += *lp) { // 先頭から末尾まで繰り返す
    if (getlineno(lp) >= lineno) {     // もし指定の行番号以上なら
        rc = index;
      break;                         // 繰り返しを打ち切る
    }
    index++;
  }
  return rc; 
} 

// ELSE中間コードをサーチする
// 引数   : 中間コードプログラムポインタ
// 戻り値 : NULL 見つからない
//          NULL以外 LESEの次のポインタ
//
uint8_t* getELSEptr(uint8_t* p) {
 uint8_t* rc = NULL;
 uint8_t* lp;

 // ブログラム中のGOTOの飛び先行番号を付け直す
  for (lp = p; *lp != I_EOL ; ) {
    switch(*lp) {
    case I_IF:      // IF命令
      goto DONE;
        break;
    case I_ELSE:    // ELSE命令
      rc = lp+1;
      goto DONE;
        break;
    case I_STR:     // 文字列
      lp += lp[1]+1;            
        break;
    case I_NUM:     // 定数
    case I_HEXNUM: 
    case I_BINNUM:
      lp+=3;        // 整数2バイト+中間コード1バイト分移動
      break;
    case I_VAR:     // 変数
      lp+=2;        // 変数名
      break;
    default:        // その他
      lp++;
    }
  }  
DONE:
  return rc;
}

// プログラム行数を取得する
uint16_t countLines(int16_t st=0, int16_t ed=32767) {
  uint8_t *lp; //ポインタ
  uint16_t cnt = 0;  
  int16_t lineno;
  for (lp = listbuf; *lp; lp += *lp)  {
    lineno = getlineno(lp);
    if (lineno < 0)
      break;
    if ( (lineno >= st) && (lineno <= ed)) 
      cnt++;
  }
  return cnt;   
}

// Insert i-code to the list
void inslist() {
  unsigned char *insp; //挿入位置
  unsigned char *p1, *p2; //移動先と移動元
  short len; //移動の長さ

  if (getsize() < *ibuf) { //もし空きが不足していたら
    err = ERR_LBUFOF; //エラー番号をセット
    return; //処理を打ち切る
  }

  insp = getlp(getlineno(ibuf)); //挿入位置を取得

  //同じ行番号の行が存在したらとりあえず削除
  if (getlineno(insp) == getlineno(ibuf)) { //もし行番号が一致したら
    p1 = insp; //p1を挿入位置に設定
    p2 = p1 + *p1; //p2を次の行に設定
    while ((len = *p2) != 0) { //次の行の長さが0でなければ繰り返す
      while (len--) //次の行の長さだけ繰り返す
        *p1++ = *p2++; //前へ詰める
    }
    *p1 = 0; //リストの末尾に0を置く
  }

  //行番号だけが入力された場合はここで終わる
  if (*ibuf == 4) //もし長さが4（行番号のみ）なら
    return; //終了する

  //挿入のためのスペースを空ける
  for (p1 = insp; *p1; p1 += *p1); //p1をリストの末尾へ移動
  len = p1 - insp + 1; //移動する幅を計算
  p2 = p1 + *ibuf; //p2を末尾より1行の長さだけ後ろに設定
  while (len--) //移動する幅だけ繰り返す
    *p2-- = *p1--; //後ろへズラす

  //行を転送する
  len = *ibuf; //中間コードの長さを設定
  p1 = insp; //転送先を設定
  p2 = ibuf; //転送元を設定
  while (len--) //中間コードの長さだけ繰り返す
    *p1++ = *p2++; //転送
}

//Listing 1 line of i-code
void putlist(uint8_t* ip, uint8_t devno=0) {
  unsigned char i; //ループカウンタ
  uint8_t var_code; // 変数コード
  
  while (*ip != I_EOL) { //行末でなければ繰り返す
    //キーワードの処理
    if (*ip < SIZE_KWTBL) { //もしキーワードなら
      c_puts_P((char*)pgm_read_word(&(kwtbl[*ip])),devno); //キーワードテーブルの文字列を表示
      if (*(ip+1) != I_COLON) 
        if ( ((!nospacea(*ip) || spacef(*(ip+1))) && (*ip != I_COLON) && (*ip != I_SQUOT))
        || ((*ip == I_CLOSE)&& (*(ip+1) != I_COLON  && *(ip+1) != I_EOL && !nospaceb(*(ip+1)))) 
        || ( spacebifValue(*ip) && spacebifValue(*(ip+1))) ) //もし例外にあたらなければ
           c_putch(' ',devno); //空白を表示

      if (*ip == I_REM||*ip == I_SQUOT) { //もし中間コードがI_REMなら
        ip++; //ポインタを文字数へ進める
        i = *ip++; //文字数を取得してポインタをコメントへ進める
        while (i--) //文字数だけ繰り返す
          c_putch(*ip++,devno); //ポインタを進めながら文字を表示
        return;
      }
      ip++;//ポインタを次の中間コードへ進める
    }
    else

    //定数の処理
    if (*ip == I_NUM) { //もし定数なら
      ip++; //ポインタを値へ進める
      putnum(*ip | *(ip + 1) << 8, 0,devno); //値を取得して表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //16進定数の処理
    if (*ip == I_HEXNUM) { //もし16進定数なら
      ip++; //ポインタを値へ進める
      c_putch('$',devno); //"$"を表示
      putHexnum(*ip | *(ip + 1) << 8, 2,devno); //値を取得して表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //2進定数の処理
    if (*ip == I_BINNUM) { //もし2進定数なら
      ip++; //ポインタを値へ進める
      c_putch('`',devno); //"`"を表示
      if (*(ip + 1))
          putBinnum(*ip | *(ip + 1) << 8, 16,devno); //値を取得して16桁で表示
      else
          putBinnum(*ip , 8,devno);  //値を取得して8桁で表示
      ip += 2; //ポインタを次の中間コードへ進める
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else 
        
    //変数の処理
    if (*ip == I_VAR) { //もし定数なら
      ip++; //ポインタを変数番号へ進める
      c_putch(*ip++ + 'A',devno); //変数名を取得して表示
      if (!nospaceb(*ip)) //もし例外にあたらなければ
        c_putch(' ',devno); //空白を表示
    }
    else

    //文字列の処理
    if (*ip == I_STR) { //もし文字列なら
      char c; //文字列の括りに使われている文字（「"」または「'」）

      //文字列の括りに使われている文字を調べる
      c = '\"'; //文字列の括りを仮に「"」とする
      ip++; //ポインタを文字数へ進める
      for (i = *ip; i; i--) //文字数だけ繰り返す
        if (*(ip + i) == '\"') { //もし「"」があれば
          c = '\''; //文字列の括りは「'」
          break; //繰り返しを打ち切る
        }

      //文字列を表示する
      c_putch(c,devno); //文字列の括りを表示
      i = *ip++; //文字数を取得してポインタを文字列へ進める
      while (i--) //文字数だけ繰り返す
        c_putch(*ip++,devno); //ポインタを進めながら文字を表示
      c_putch(c,devno); //文字列の括りを表示
      if (*ip == I_VAR || *ip ==I_ELSE) //もし次の中間コードが変数だったら
        c_putch(' ',devno); //空白を表示
    }

    else { //どれにも当てはまらなかった場合
      err = ERR_SYS; //エラー番号をセット
      return; //終了する
    }
  }
}

// Get argument in parenthesis
short getparam() {
  short value; //値

  if (*cip != I_OPEN) {  // もし「(」でなければ
    err = ERR_PAREN;     // エラー番号をセット
    return 0;            // 終了
  }
  cip++;                 // 中間コードポインタを次へ進める

  value = iexp();        // 式を計算
  if (err)               // もしエラーが生じたら
    return 0; //終了

  if (*cip != I_CLOSE) { // もし「)」でなければ
    err = ERR_PAREN;     // エラー番号をセット
    return 0; //終了
  }
  cip++; //中間コードポインタを次へ進める

  return value; //値を持ち帰る
}

#if USE_DMP == 1
// 小数点数値出力 DMP$(数値) or DMP(数値,小数部桁数) or DMP(数値,小数部桁数,整数部桁指定)
void idmp(uint8_t devno=CDEV_SCREEN) {
  int32_t value;     // 値
  int32_t v1,v2;
  int16_t n = 2;    // 小数部桁数
  int16_t dn = 0;   // 整数部桁指定
  int32_t base=1;
  
  if (checkOpen()) return;
  if (getParam(value, false)) return;
  if (*cip == I_COMMA) { 
    cip++; 
    if (getParam(n, 0,4,false)) return;
    if (*cip == I_COMMA) { 
       cip++; 
      if (getParam(dn,-6,6,false)) return;
    }  
  }
  if (checkClose()) return;
  
  for (uint16_t i=0; i<n;i++) {
    base*=10;
  }
  v1 = value / base;
  v2 = value % base;
  if (v1 == 0 && value <0)
    c_putch('-',devno);  
  putnum(v1, dn, devno);
  if (n) {
    c_putch('.',devno);
    putnum(v2<0 ?-v2:v2, -n, devno);
  }
}
#endif

// 文字列参照 WSTR$(変数)
// WSTR(文字列参照変数|文字列参照配列変数|文字列定数,[pos,n])
// ※変数,配列は　[LEN][文字列]への参照とする
// 引数
//  devno: 出力先デバイス番号
// 戻り値
//  なし
//
void iwstr(uint8_t devno=CDEV_SCREEN) {
  int16_t len;  // 文字列長
  int16_t top;  // 文字取り出し位置
  int16_t n;    // 取り出し文字数
  int16_t index;
  uint8_t *ptr;  // 文字列先頭
  
  if (checkOpen()) return;
  if (*cip == I_VAR) {
    // 変数
    cip++;
    ptr = v2realAddr(var[*cip]);
    len = *ptr;
    ptr++;
    cip++;
  } else if (*cip == I_ARRAY) {
    // 配列変数
    cip++; 
    if (getParam(index, 0, SIZE_ARRY-1, false)) return;
    ptr = v2realAddr(arr[index]);
    len = *ptr;
    ptr++;    
  } else if (*cip == I_STR) {
    // 文字列定数
    cip++;
    len = *cip;
    cip++;
    ptr = cip;
    cip+=len;    
  } else {
    err = ERR_SYNTAX;
    return;
  }
  top = 1; // 文字取り出し位置
  n = len; // 取り出し文字数
  if (*cip == I_COMMA) {
    // 引数：文字取り出し位置、取り出し文字数の取得 
    cip++;
    if (getParam(top, 1,len,true)) return;
    if (getParam(n,1,len-top+1,false)) return;
  }
  if (checkClose()) return;

  // 全角を考慮した文字位置の取得
  int16_t i;
  int16_t wtop = 1;
  for (i=0; i < len; i++) {
    if (wtop == top) {
      break;
    }
    if (isZenkaku(ptr[i])) {
      i++;  
    }
    wtop++;
  }
  if (wtop == top) {
    //実際の取り出し位置
    top = i+1;
  } else {
    err = ERR_VALUE;
    return;
  }
  
  // 全角を考慮した取り出し文字列の出力
  int16_t cnt=0;
  for (uint16_t i = top-1 ; i < len; i++) {
    if (cnt == n) {
      break;
    }  
    c_putch(ptr[i], devno);
    if (isZenkaku(ptr[i])) {
      i++; c_putch(ptr[i], devno);
    }
    cnt++;
  }
  return;
}

// 画面クリア
void icls() {
  clear();
  move(0,0);
  //err=0;
}

// Variable assignment handler
void ivar() {
  int16_t value; //値
  int16_t index; //変数番号

  index = *cip++; //変数番号を取得して次へ進む

  if (*cip != I_EQ) { //もし「=」でなければ
    err = ERR_VWOEQ; //エラー番号をセット
    return;
  }
  cip++; //中間コードポインタを次へ進める
  if (*cip == I_STR) {
    cip++;
    value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
    cip += *cip+1;
  } else {
  //値の取得と代入
  value = iexp(); //式の値を取得
  if (err) //もしエラーが生じたら
    return; //終了
  }
  var[index] = value; //変数へ代入
}

// Array assignment handler
void iarray() {
  int16_t value; //値
  int16_t index; //配列の添え字

  index = getparam(); //配列の添え字を取得
  if (err) //もしエラーが生じたら
    return; //終了

  if (index >= SIZE_ARRY || index < 0 ) { //もし添え字が上下限を超えたら
    err = ERR_SOR; //エラー番号をセット
    return; //終了
  }

  if (*cip != I_EQ) { //もし「=」でなければ
    err = ERR_VWOEQ; //エラー番号をセット
    return; //終了
  }

  // 例: @(n)=1,2,3,4,5 の連続設定処理
  do {
    cip++; 
    if (*cip == I_STR) {
      cip++;
      value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
      cip += *cip+1;
    } else {
      value = iexp(); // 式の値を取得
      if (err)        // もしエラーが生じたら
        return;       // 終了
    }
    if (index >= SIZE_ARRY) { // もし添え字が上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      return; 
    }
    arr[index] = value; //配列へ代入
    index++;
  } while(*cip == I_COMMA);
} 

// INPUT handler
void iinput() {
  int16_t value;          // 値
  int16_t index;          // 配列の添え字or変数番号
  uint8_t i;              // 文字数
  uint8_t prompt;         // プロンプト表示フラグ
  int16_t ofvalue;        // オーバーフロー時の設定値
  uint8_t flgofset =0;    // オーバーフロ時の設定値指定あり

  c_show_curs(1);
  prompt = 1;       // まだプロンプトを表示していない

  // プロンプトが指定された場合の処理
  if(*cip == I_STR){   // もし中間コードが文字列なら
    cip++;             // 中間コードポインタを次へ進める
    i = *cip++;        // 文字数を取得
    while (i--)        // 文字数だけ繰り返す
      c_putch(*cip++); // 文字を表示
    prompt = 0;        // プロンプトを表示した

    if (*cip != I_COMMA) {
      err = ERR_SYNTAX;
      goto DONE;
    }
    cip++;
  }

  // 値を入力する処理
  switch (*cip++) {       // 中間コードで分岐
  case I_VAR:             // 変数の場合
    index = *cip;         // 変数番号の取得
    cip++;
   
    // オーバーフロー時の設定値
    if (*cip == I_COMMA) {
      cip++;
      ofvalue = iexp();
      if (err) {
        goto DONE;
      }
      flgofset = 1;
    }
    
    if (prompt) {          // もしまだプロンプトを表示していなければ
      c_putch('A'+index);  // 変数名を表示
      c_putch(':');        //「:」を表示
    }
    
    value = getnum();     // 値を入力
    if (err) {            // もしエラーが生じたら
      if (err == ERR_VOF && flgofset) {
        err = ERR_OK;
        value = ofvalue;
      } else {
        return;            // 終了
      }
    }
    var[index] = value;  // 変数へ代入
    break;               // 打ち切る

  case I_ARRAY: // 配列の場合
    index = getparam();       // 配列の添え字を取得
    if (err)                  // もしエラーが生じたら
      goto DONE;

    if (index >= SIZE_ARRY) { // もし添え字が上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      goto DONE;
    }

    // オーバーフロー時の設定値
    if (*cip == I_COMMA) {
      cip++;
      ofvalue = iexp();
      if (err) {
        goto DONE;
      }
      flgofset = 1;
    }

    if (prompt) { // もしまだプロンプトを表示していなければ
      c_puts_P((const char*)F("@("));     //「@(」を表示
      putnum(index, 0);                   // 添え字を表示
      c_puts_P((const char*)F("):"));     //「):」を表示
    }
    value = getnum(); // 値を入力
    if (err) {           // もしエラーが生じたら
      if (err == ERR_VOF && flgofset) {
        err = ERR_OK;
        value = ofvalue;
      } else {
        goto DONE;
      }
    }
    arr[index] = value; // 配列へ代入
    break;              // 打ち切る

  default: // 以上のいずれにも該当しなかった場合
    err = ERR_SYNTAX; // エラー番号をセット
    goto DONE;
  } // 中間コードで分岐の末尾
DONE:  
  c_show_curs(0);
}

// LET handler
void ilet() {
  if (*cip == I_VAR) {          //変数の場合
    cip++; 
    ivar(); //変数への代入を実行    
  } else if  (*cip == I_ARRAY) { //配列の場合
    cip++; 
    iarray(); //配列への代入を実行    
  } else {
    err = ERR_LETWOV; //エラー番号をセット    
  }
}

// END
void iend() {
  while (*clp)    // 行の終端まで繰り返す
    clp += *clp;  // 行ポインタを次へ進める
}

// IF
void iif() {
  int16_t condition;    // IF文の条件値
  uint8_t* newip;       // ELSE文以降の処理対象ポインタ

  condition = iexp(); // 真偽を取得
  if (err) {          // もしエラーが生じたら
    err = ERR_IFWOC;  // エラー番号をセット
    return;
  }
  if (condition) {    // もし真なら
    return;
  } else { 
    // 偽の場合の処理
    // ELSEがあるかチェックする
    // もしELSEより先にIFが見つかったらELSE無しとする
    // ELSE文が無い場合の処理はREMと同じ
    newip = getELSEptr(cip);
    if (newip != NULL) {
      cip = newip;
      return;
    }
    while (*cip != I_EOL) // I_EOLに達するまで繰り返す
    cip++;                // 中間コードポインタを次へ進める
  }
}

// ラベル
inline void ilabel() {
   cip+= *cip+1;   
}

// GOTO
void igoto() {
  uint8_t* lp;       // 飛び先行ポインタ
  int16_t lineno;    // 行番号

  if (*cip == I_STR) { 
    // ラベル参照による分岐先取得
    lp = getlpByLabel(cip);                   
    if (lp == NULL) {
      err = ERR_ULN;                          // エラー番号をセット
      return;
    }  
  } else {
    // 引数の行番号取得
    lineno = iexp();                          
    if (err)  return;         
    lp = getlp(lineno);                       // 分岐先のポインタを取得
    if (lineno != getlineno(lp)) {            // もし分岐先が存在しなければ
      err = ERR_ULN;                          // エラー番号をセット
      return; 
    }
  }
  clp = lp;        // 行ポインタを分岐先へ更新
  cip = clp + 3;   // 中間コードポインタを先頭の中間コードに更新
}

// GOSUB
void igosub() {
  uint8_t* lp;       // 飛び先行ポインタ
  int16_t lineno;    // 行番号

  if (*cip == I_STR) {
    // ラベル参照による分岐先取得
    lp = getlpByLabel(cip);                   
    if (lp == NULL) {
      err = ERR_ULN;  // エラー番号をセット
      return; 
    }  
  } else {
    // 引数の行番号取得
    lineno = iexp();
    if (err)
      return;  

    lp = getlp(lineno);                       // 分岐先のポインタを取得
    if (lineno != getlineno(lp)) {            // もし分岐先が存在しなければ
      err = ERR_ULN;                          // エラー番号をセット
      return; 
    }
  }
  
  //ポインタを退避
  if (gstki > SIZE_GSTK - 2) {              // もしGOSUBスタックがいっぱいなら
    err = ERR_GSTKOF;                       // エラー番号をセット
      return; 
  }
  gstk[gstki++] = clp;                      // 行ポインタを退避
  gstk[gstki++] = cip;                      // 中間コードポインタを退避

  clp = lp;                                 // 行ポインタを分岐先へ更新
  cip = clp + 3;                            // 中間コードポインタを先頭の中間コードに更新
}

// RETURN
void ireturn() {
  if (gstki < 2) {    // もしGOSUBスタックが空なら
    err = ERR_GSTKUF; // エラー番号をセット
    return; 
  }
  cip = gstk[--gstki]; //行ポインタを復帰
  clp = gstk[--gstki]; //中間コードポインタを復帰
  return;  
}

// FOR
void ifor() {
  int16_t index, vto, vstep; // FOR文の変数番号、終了値、増分
  
  // 変数名を取得して開始値を代入（例I=1）
  if (*cip++ != I_VAR) { // もし変数がなかったら
    err = ERR_FORWOV;    // エラー番号をセット
    return;
  }
  index = *cip; // 変数名を取得
  ivar();       // 代入文を実行
  if (err)      // もしエラーが生じたら
    return;

  // 終了値を取得（例TO 5）
  if (*cip == I_TO) { // もしTOだったら
    cip++;             // 中間コードポインタを次へ進める
    vto = iexp();      // 終了値を取得
  } else {             // TOではなかったら
    err = ERR_FORWOTO; //エラー番号をセット
    return;
  }

  // 増分を取得（例STEP 1）
  if (*cip == I_STEP) { // もしSTEPだったら
    cip++;              // 中間コードポインタを次へ進める
    vstep = iexp();     // 増分を取得
  } else                // STEPではなかったら
    vstep = 1;          // 増分を1に設定

  // もし変数がオーバーフローする見込みなら
  if (((vstep < 0) && (-32767 - vstep > vto)) ||
    ((vstep > 0) && (32767 - vstep < vto))){
    err = ERR_VOF; //エラー番号をセット
    return;
  }

  // 繰り返し条件を退避
  if (lstki > SIZE_LSTK - 5) { // もしFORスタックがいっぱいなら
    err = ERR_LSTKOF;          // エラー番号をセット
    return;
  }
  lstk[lstki++] = clp; // 行ポインタを退避
  lstk[lstki++] = cip; // 中間コードポインタを退避

  // FORスタックに終了値、増分、変数名を退避
  // Special thanks hardyboy
  lstk[lstki++] = (uint8_t*)(uintptr_t)vto;
  lstk[lstki++] = (uint8_t*)(uintptr_t)vstep;
  lstk[lstki++] = (uint8_t*)(uintptr_t)index;  
}

// NEXT
void inext() {
  int16_t index, vto, vstep; // FOR文の変数番号、終了値、増分

  if (lstki < 5) {    // もしFORスタックが空なら
    err = ERR_LSTKUF; // エラー番号をセット
    return;
  }

  // 変数名を復帰
  index = (int16_t)(uintptr_t)lstk[lstki - 1]; // 変数名を復帰
  if (*cip++ != I_VAR) {                     // もしNEXTの後ろに変数がなかったら
    err = ERR_NEXTWOV;                       // エラー番号をセット
    return;
  }
  if (*cip++ != index) { // もし復帰した変数名と一致しなかったら
    err = ERR_NEXTUM;    // エラー番号をセット
    return;
  }

  vstep = (int16_t)(uintptr_t)lstk[lstki - 2]; // 増分を復帰
  var[index] += vstep;                       // 変数の値を最新の開始値に更新
  vto = (short)(uintptr_t)lstk[lstki - 3];   // 終了値を復帰

  // もし変数の値が終了値を超えていたら
  if (((vstep < 0) && (var[index] < vto)) ||
    ((vstep > 0) && (var[index] > vto))) {
    lstki -= 5;  // FORスタックを1ネスト分戻す
    return;
  }

  // 開始値が終了値を超えていなかった場合
  cip = lstk[lstki - 4]; //行ポインタを復帰
  clp = lstk[lstki - 5]; //中間コードポインタを復帰
}

// スキップ
void iskip() {
  while (*cip != I_EOL) // I_EOLに達するまで繰り返す
    cip++;              // 中間コードポインタを次へ進める
}

// LISTコマンド
//  devno : デバイス番号 0:メインスクリーン 1:シリアル 2:グラフィック 3:、メモリー 4:ファイル
void ilist(uint8_t devno=0) {
  int16_t lineno = 0;          // 表示開始行番号
  int16_t endlineno = 32767;   // 表示終了行番号
  int16_t prnlineno;           // 出力対象行番号
  int16_t c;
  
  //表示開始行番号の設定
  if (*cip != I_EOL && *cip != I_COLON) {
    // 引数あり
    if (getParam(lineno,0,32767,false)) return;                // 表示開始行番号
    if (*cip == I_COMMA) {
      cip++;                         // カンマをスキップ
      if (getParam(endlineno,lineno,32767,false)) return;      // 表示終了行番号
     }
   }
 
  //行ポインタを表示開始行番号へ進める
  for ( clp = listbuf; *clp && (getlineno(clp) < lineno); clp += *clp); 
  
  //リストを表示する
  while (*clp) {               // 行ポインタが末尾を指すまで繰り返す

    //強制的な中断の判定
    if (isBreak())
      return;
   
    prnlineno = getlineno(clp);// 行番号取得
    if (prnlineno > endlineno) // 表示終了行番号に達したら抜ける
       break; 
    putnum(prnlineno, 0,devno);// 行番号を表示
    c_putch(' ',devno);        // 空白を入れる
    putlist(clp + 3,devno);    // 行番号より後ろを文字列に変換して表示
    if (err)                   // もしエラーが生じたら
      break;                   // 繰り返しを打ち切る
    newline(devno);            // 改行
    clp += *clp;               // 行ポインタを次の行へ進める
  }
}

// MSG tm,..
// VFDメッセージ表示
void ivmsg(uint8_t flgZ=0) {
#if USE_CMD_VFD == 1
  int16_t dir,tm,lat=1;
  if ( getParam(tm, -1, 1024,true) )  return;
  if (tm == -1) {
    lat = 0;
    tm = 0;
  } 
  // メッセージ部をバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,1);  
  if (err)
    return;

  // メッセージ部の表示
  VFD.dispSentence((const char*)lbuf, tm,flgZ,lat);
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
  
  if (getParam(vadr, 0, 32767, true)) return;       // データアドレス
  if (getParam(len, 0, 32767, true)) return;        // データ長
  if (getParam(tm, -1, 1024,false) )  return;       // 時間

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
  if ( getParam(w, 1, 256,true) )  return;
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

// RENUME command handler
void irenum() {
  uint16_t startLineNo = 10;  // 開始行番号
  uint16_t increase = 10;     // 増分
  uint8_t* ptr;               // プログラム領域参照ポインタ
  uint16_t len;               // 行長さ
  uint16_t i;                 // 中間コード参照位置
  uint16_t newnum;            // 新しい行番号
  uint16_t num;               // 現在の行番号
  uint16_t index;             // 行インデックス
  uint16_t cnt;               // プログラム行数
  
  // 開始行番号、増分引数チェック
  if (*cip == I_NUM) {               // もしRENUMT命令に引数があったら
    startLineNo = getlineno(cip);    // 引数を読み取って開始行番号とする
    cip+=3;
    if (*cip == I_COMMA) {
        cip++;                        // カンマをスキップ
        if (*cip == I_NUM) {          // 増分指定があったら
           increase = getlineno(cip); // 引数を読み取って増分とする
        } else {
           err = ERR_SYNTAX;          // カンマありで引数なしの場合はエラーとする
           return;
       }
    }
  }

  // 引数の有効性チェック
  cnt = countLines()-1;
  if (startLineNo <= 0 || increase <= 0) {
    err = ERR_VALUE;
    return;   
  }
  if (startLineNo + increase * cnt > 32767) {
    err = ERR_VALUE;
    return;       
  }

  // ブログラム中のGOTOの飛び先行番号を付け直す
  for (  clp = listbuf; *clp ; clp += *clp) {
     ptr = clp;
     len = *ptr;
     ptr++;
     i=0;
     // 行内検索
     while( i < len-1 ) {
        switch(ptr[i]) {
        case I_GOTO:  // GOTO命令
        case I_GOSUB: // GOSUB命令
          i++;
          if (ptr[i] == I_NUM) {
            num = getlineno(&ptr[i]);    // 現在の行番号を取得する
            index = getlineIndex(num);   // 行番号の行インデックスを取得する
            if (index == 32767) {
               // 該当する行が見つからないため、変更は行わない
               i+=3;
               continue;
            } else {
               // とび先行番号を付け替える
               newnum = startLineNo + increase*index;
               ptr[i+2] = newnum>>8;
               ptr[i+1] = newnum&0xff;
               i+=3;
               continue;
            }
          } 
          break;
      case I_STR:  // 文字列
        i++;
        i+=ptr[i]; // 文字列長分移動
        break;
      case I_NUM:  // 定数
      case I_HEXNUM:
      case I_BINNUM: 
        i+=3;      // 整数2バイト+中間コード1バイト分移動
        break;
      case I_VAR:  // 変数
        i+=2;      // 変数名
        break;
      default:     // その他
        i++;
        break;
      }
    }
  }
  
  // 各行の行番号の付け替え
  index = 0;
  for (  clp = listbuf; *clp ; clp += *clp ) {
     newnum = startLineNo + increase * index;
     *(clp+1)  = newnum&0xff;
     *(clp+2)  = newnum>>8;
     index++;
  }
}

// 指定行の削除
// DELETE 行番号
// DELETE 開始行番号,終了行番号
void idelete() {
  int16_t sNo;
  int16_t eNo;
  uint8_t  *lp;      // 削除位置ポインタ
  uint8_t *p1, *p2;  // 移動先と移動元ポインタ
  int16_t len;       // 移動の長さ

  if ( getParam(sNo, false) ) return;
  if (*cip == I_COMMA) {
     cip++;
     if ( getParam(eNo, false) ) return;  
  } else {
     eNo = sNo;
  }

  if (eNo < sNo) {
    err = ERR_VALUE;
    return;
  }

  if (eNo == sNo) {
    lp = getlp(eNo); // 削除位置ポインタを取得
    if (getlineno(lp) == sNo) {
      // 削除
      p1 = lp;                              // p1を挿入位置に設定
      p2 = p1 + *p1;                        // p2を次の行に設定
      while ((len = *p2) != 0) {            // 次の行の長さが0でなければ繰り返す
        while (len--)                       // 次の行の長さだけ繰り返す
          *p1++ = *p2++;                    // 前へ詰める
      }
      *p1 = 0; // リストの末尾に0を置く
    }
  } else {
    for (uint16_t i = sNo; i <= eNo;i++) {
      lp = getlp(i); // 削除位置ポインタを取得
      if (getlineno(lp) == i) {               // もし行番号が一致したら
        p1 = lp;                              // p1を挿入位置に設定
        p2 = p1 + *p1;                        // p2を次の行に設定
        while ((len = *p2) != 0) {            // 次の行の長さが0でなければ繰り返す
          while (len--)                       // 次の行の長さだけ繰り返す
            *p1++ = *p2++;                    // 前へ詰める
        }
        *p1 = 0; // リストの末尾に0を置く
      }
    }
  }
}

// 引数ファイル名の取得
uint8_t getFname(uint8_t* fname, uint8_t limit) {
  int16_t len;

  // ファイル名の取得
  if (*cip != I_STR) {
    err = ERR_VALUE;
    return 1;
  }

  cip++;         //中間コードポインタを次へ進める
  len = *cip++;  //文字数を取得
  if (len > limit) {
    // ファイル名が制限長を超えている
    err = ERR_FNAME;
    return 1;
  }
  strncpy((char*)fname,(char*)cip,len);
  fname[len] = 0;
  for (uint8_t i=0; i<len; i++) {
    fname[i] = c_toupper(fname[i]);
//    if (fname[i] >='a' && fname[i] <= 'z')
//       fname[i] = fname[i]-'a' + 'A';
  }
  cip+= len;
  return 0;  
}

// プログラム保存 SAVE 保存領域番号|"ファイル名"
void isave() {
  int16_t  prgno = 0;
  uint16_t topAddr;

  // 引数がファイル名かのチェック
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
  if (*cip == I_STR) {

    // EEPROMへの保存処理
    uint8_t fname[TI2CEPPROM_FNAMESIZ+1];
    uint8_t rc;
   
    // ファイル名の取得
    if (getFname(fname, TI2CEPPROM_FNAMESIZ)) 
      return;

    // プログラムの保存  
    if (rc = rom.save(fname, 0, listbuf, SIZE_LIST)) {
       if (rc == 2)
         err = ERR_NOFSPACE;
       else
         err = ERR_I2CDEV;
    }  
  } else 
 #endif 
  {

    // 内部EEPROMメモリへの保存
    if (*cip == I_EOL) {
      prgno = 0;
    } else if ( getParam(prgno, 0, EEPROM_SAVE_NUM-1, false) ) {
      return;  
    }
    
    topAddr = EEPROM_PAGE_SIZE*prgno;
    eeprom_update_block(listbuf,topAddr,SIZE_LIST);
  }
}

//NEW command handler
void inew(void) {
  //変数と配列の初期化
  memset(var,0,26);
  memset(arr,0,SIZE_ARRY);

  //実行制御用の初期化
  gstki = 0; //GOSUBスタックインデクスを0に初期化
  lstki = 0; //FORスタックインデクスを0に初期化
  *listbuf = 0; //プログラム保存領域の先頭に末尾の印を置く
  clp = listbuf; //行ポインタをプログラム保存領域の先頭に設定
}

// プログラムロード LOAD 保存領域番号|"ファイル名"
void iload(uint8_t flgskip=0) {
  int16_t  prgno = 0;
  uint16_t topAddr;

  // 現在のプログラムの削除
  inew();
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
  // 引数がファイル名かのチェック
  if (*cip == I_STR) {
    // EEPROMへの保存処理
    uint8_t fname[TI2CEPPROM_FNAMESIZ+1];
    uint8_t rc;
    
    // ファイル名の取得
    if (getFname(fname, TI2CEPPROM_FNAMESIZ)) 
      return;
  
    // プログラムのロード
    if (rc = rom.load(fname, 0, listbuf, SIZE_LIST)) {
      if (rc == 2)
         err = ERR_FNAME;  // ファイル名が正しくない,指定したファイルが存在しない
      else 
         err = ERR_I2CDEV; // I2Cデバイスエラー
    }

  } else 
#endif  
  {
    // 内部EEPROMメモリからのロード
    
    if (!flgskip) {
      if (*cip == I_EOL) {
        prgno = 0;
      } else if ( getParam(prgno, 0, EEPROM_SAVE_NUM-1, false) ) {
        return;  
      }
    }  

    // プログラムのロード
    topAddr = EEPROM_PAGE_SIZE*prgno;
    eeprom_read_block(listbuf, topAddr, SIZE_LIST);         
  }
}

// フラッシュメモリ上のプログラム消去 ERASE[プログラム番号[,プログラム番号]
void ierase() {
  int16_t  s_prgno, e_prgno;
  uint32_t* topAddr;
  void iedel();
  
  // ファイル名指定の場合、I2C EPPROMの指定ファイル削除を行う
  if (*cip == I_STR) {
     iedel();
     return;
  } 
  
  if ( getParam(s_prgno, 0, EEPROM_SAVE_NUM-1, false) ) return;
  e_prgno = s_prgno;
  if (*cip == I_COMMA) {
    cip++;
    if ( getParam(e_prgno, 0, EEPROM_SAVE_NUM-1, false) ) return;
  }
  for (uint8_t prgno = s_prgno; prgno <= e_prgno; prgno++) {
    topAddr = (uint32_t*)(EEPROM_PAGE_SIZE*prgno);
    for (uint16_t i=0; i < SIZE_LIST/4; i++) {
      eeprom_update_dword(topAddr,0);
    }      
  }
}

// プログラムファイル一覧表示 FILES
void ifiles() {
  void iefiles();

  // ファイル名指定の場合、I2C EPPROMの一覧表示を行う
  if (*cip == I_STR) {
     iefiles();
     return;
  } 
  
  int16_t StartNo, endNo; // プログラム番号開始、終了
  
  // 引数が数値または、無しの場合、フラッシュメモリのリスト表示
  if (*cip == I_EOL || *cip == I_COLON) {
   StartNo = 0;
   endNo = EEPROM_SAVE_NUM-1;
  } else {
   if ( getParam(StartNo, 0, EEPROM_SAVE_NUM-1, false) ) return;
   // 第2引数チェック
   if (*cip == I_COMMA) {
     cip++;
     if ( getParam(endNo, false) ) return;  
    } else {
     endNo = StartNo;
    }
    if (StartNo > endNo) {
      err = ERR_VALUE;
      return;    
    }
  }     
  for (uint8_t i=StartNo ; i <= endNo; i++) {
    // EEOROMからデータのコピー
    clearlbuf();
    eeprom_read_block(lbuf,(uint8_t*)(EEPROM_PAGE_SIZE*i), SIZE_LINE);
    putnum(i,1);  c_putch(':');
    if( (lbuf[0] == 0x00) && (lbuf[1] == 0x00) ) {  //  プログラム有無のチェック
      c_puts_P((const char*)F("(none)"));        
    } else {
      if (*lbuf) {
        putlist(lbuf+3);         // 行番号より後ろを文字列に変換して表示
      } 
    } 
    newline();
  }
}

// EEPROMのフォーマット
// FORMAT デバイスサイズ[,ドライブ名]
void iformat() {
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
  uint8_t  devname[11];
  uint8_t  rc;
  int16_t  devsize;       // デバイスサイズ
  uint16_t fnum;          // ファイル数
  devname[0] = 0;

  // デバイス容量指定の取得
#if PRGAREASIZE <= 512
  if ( getParam(devsize, 4, 64, false) ) return;  
  if (devsize == 4) {
    fnum = 7;
  } else if (devsize == 8) {
    fnum = 15;
  } else if (devsize == 16) {
    fnum = 31;
  } else if (devsize == 32) {
    fnum = 62;
  } else if (devsize == 64) {
    fnum = 124;
  } else {
    err = ERR_VALUE;
    return;
  }
#elif PRGAREASIZE <= 1024
  if ( getParam(devsize, 4, 64, false) ) return;  
  if (devsize == 4) {
    fnum = 3;
  } else if (devsize == 8) {
    fnum = 7;
  } else if (devsize == 16) {
    fnum = 25;
  } else if (devsize == 32) {
    fnum = 31;
  } else if (devsize == 64) {
    fnum = 63;
  } else {
    err = ERR_VALUE;
    return;
  }
#elif PRGAREASIZE <= 2048
  if ( getParam(devsize, 4, 64, false) ) return;  
  if (devsize == 4) {
    fnum = 1;
  } else if (devsize == 8) {
    fnum = 3;
  } else if (devsize == 16) {
    fnum = 7;
  } else if (devsize == 32) {
    fnum = 15;
  } else if (devsize == 64) {
    fnum = 31;
  } else {
    err = ERR_VALUE;
    return;
  }
#elif PRGAREASIZE <= 4096
  if ( getParam(devsize, 4, 64, false) ) return;  
 if (devsize == 8) {
    fnum = 1;
  } else if (devsize == 16) {
    fnum = 3;
  } else if (devsize == 32) {
    fnum = 7;
  } else if (devsize == 64) {
    fnum = 15;
  } else {
    err = ERR_VALUE;
    return;
  }
#endif
    // ファイル名の取得
   if (*cip == I_COMMA) {
     cip++;
    if (getFname(devname, 10))  return;  
   }
   
  // デバイスのフォーマット
  rc = rom.format(MYSIGN, devname,fnum, SIZE_LIST);
  if (rc) {
    err = ERR_I2CDEV; // I2Cデバイスエラー    
  }
#endif
}

#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
// ワイルドカードでのマッチング(下記のリンク先のソースを利用)
// http://qiita.com/gyu-don/items/5a640c6d2252a860c8cd
//
// [引数]
//  wildcard : 判定条件(ワイルドカード *,?の利用可能)
//  target   : 判定対象文字列
// [戻り値]
// 一致     : 1
// 不一致   : 0
// 
uint8_t wildcard_match(char *wildcard, char *target) {
    char *pw = wildcard, *pt = target;
    for(;;){
        if(*pt == 0) return *pw == 0;
        else if(*pw == 0) return 0;
        else if(*pw == '*'){
            return *(pw+1) == 0 || wildcard_match(pw, pt + 1)
                                || wildcard_match(pw + 1, pt);
        }
        else if(*pw == '?' || (*pw == *pt)){
            pw++;
            pt++;
            continue;
        }
        else return 0;
    }
}
#endif 


// I2C EEPROM内ファイル一覧表示
// FILES "ファイル名"
// ファイル名にはワイルドカード(*?)指定可能
void iefiles() {
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
  uint8_t  fname[TI2CEPPROM_FNAMESIZ+1];    // 引数ファイル名
  uint8_t  tmpfname[TI2CEPPROM_FNAMESIZ+1]; // ファイル管理テーブル内ファイル名
  uint8_t ftable[16];                       // ファイル管理テーブル  
  int16_t  num;                             // ファイル管理テーブル数
  uint8_t flgcmp = 1;                       // ファイル比較フラグ
  uint8_t rc;
  
  // ファイル名の取得
  if (getFname(fname, TI2CEPPROM_FNAMESIZ)) 
    return;
  if (strlen(fname) == 0) {
     flgcmp = 0;
  }
  
  // ファイルテーブル数の取得
  if ( (num = rom.maxFiles()) < 0) {
    err = ERR_I2CDEV;
    return;
  }

  // ドライブ名の表示
  if (rom.getDevName(tmpfname) < 0) {
    err = ERR_I2CDEV;
    return;
  }
  c_puts_P((const char*)F("Drive:")); 
  c_puts(tmpfname);
  newline(); //改行
  
  // ファイル名表示ループ
  uint8_t cnt=0,total=0;
  for (uint8_t i=0; i < num; i++) {
    //強制的な中断の判定
    if (isBreak())
      return;
      
    if( rc = rom.getTable(ftable,i) ) { // 管理テーブルの取得
      err = ERR_I2CDEV;
      return;
    }
    if (!ftable[0])
      continue; // ブランクの場合はスキップ

    memset(tmpfname,0,TI2CEPPROM_FNAMESIZ+1);
    strncpy(tmpfname,ftable,TI2CEPPROM_FNAMESIZ);
    if ( !flgcmp || wildcard_match(fname, tmpfname) ) {
      // 条件に一致      
      c_puts(tmpfname);
      if ((cnt % 4) == 3)
        newline();
      else
        for(uint8_t j=0; j < 18-strlen(tmpfname); j++)
          c_putch(' ');
      cnt++;
    }
    total++;
  }
  newline(); 
  putnum(cnt,1); c_putch('/'); putnum(total,1); c_putch('(');putnum(num,1);c_putch(')');
  c_puts_P((const char*)F(" files")); 
  newline(); //改行
#endif
}

// I2C EPPROM ファイルの削除
void iedel() {
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
    // EEPROMへの保存処理
    uint8_t fname[TI2CEPPROM_FNAMESIZ+1];
    uint8_t rc;
    
    // ファイル名の取得
    if (getFname(fname, TI2CEPPROM_FNAMESIZ)) 
      return;
  
    // プログラムのロード
    if (rc = rom.del(fname)) {
      if (rc == 2)
         err = ERR_FNAME;  // ファイル名が正しくない,指定したファイルが存在しない
      else 
         err = ERR_I2CDEV; // I2Cデバイスエラー
    }  
#endif
}

// I2C EPPROM スレーブアドレス設定
// DRIVE アドレス|"ドライブ"
void idrive() {
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
 int16_t adr;
  uint8_t fname[TI2CEPPROM_FNAMESIZ+1];

  // ファイル名指定の場合、I2C EPPROMの一覧表示を行う
  if (*cip == I_STR) {
    if (getFname(fname, TI2CEPPROM_FNAMESIZ))   return;
    if (strlen(fname) !=1) {
      err = ERR_FNAME;
      return;
    }
    adr = 0x50 + fname[0]-'A';    
  } else {
    if ( getParam(adr, 1,0x7f, false) ) return;
  }  
  rom.setSlaveAddr(adr); 
#endif  
}

// 時間待ち
void iwait() {
  int16_t tm;
  if ( getParam(tm, 0, 32767, false) ) return;
  delay(tm);
}

// カーソル移動 LOCATE x,y
void ilocate() {
  int16_t x,  y;
  if ( getParam(x, true) ) return;
  if ( getParam(y, false) ) return;
  if ( x >= c_getWidth() )   // xの有効範囲チェック
     x = c_getWidth() - 1;
  else if (x < 0)  x = 0;  
  if( y >= c_getHeight() )   // yの有効範囲チェック
     y = c_getHeight() - 1;
  else if(y < 0)   y = 0;

  // カーソル移動
  move(y,x);
}

// 文字色/属性テーブル
const PROGMEM  uint16_t tbl_fcolor[]  =
   { F_BLACK,F_RED,F_GREEN,F_BROWN,F_BLUE,F_MAGENTA,F_CYAN,A_NORMAL,F_YELLOW};
const PROGMEM  uint16_t tbl_bcolor[]  =
   { B_BLACK,B_RED,B_GREEN,B_BROWN,B_BLUE,B_MAGENTA,B_CYAN,B_WHITE,B_YELLOW};
const PROGMEM  uint16_t tbl_attr[]  =
    { A_NORMAL, A_UNDERLINE, A_REVERSE, A_BLINK, A_BOLD };  

// 文字色の指定 COLOLR fc[,bc]
void icolor() {
  int16_t fc,  bc = 0;
  if ( getParam(fc, 0, 8, false) ) return;
  if(*cip == I_COMMA) {
    // 第2引数(背景色）の指定あり
    cip++;
    if ( getParam(bc, 0, 8, false) ) return;  
  }
  attrset(pgm_read_word(&tbl_fcolor[fc])|pgm_read_word(&tbl_bcolor[bc]));
}

// 文字属性の指定 ATTRコマンド
void iattr() {
  int16_t attr;
  if ( getParam(attr, 0, 4, false) ) return;
  attrset(pgm_read_word(&tbl_attr[attr]));
}

// キー入力文字コードの取得 INKEY()関数
int16_t iinkey() {
  int16_t rc = 0;
  
  if (prevPressKey) {
    // 一時バッファに入力済キーがあればそれを使う
    rc = prevPressKey;
    prevPressKey = 0;
  } else if (c_kbhit( )) {
    // キー入力
    rc = c_getch();
  }
  return rc;
}

// GPIO ピン機能設定
void igpio() {
  int16_t pinno;  // ピン番号
  int16_t pmode;  // 入出力モード

  // 入出力ピンの指定
  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) ) return; // ピン番号取得
  if ( getParam(pmode, 0, 2, false) ) return;            // 入出力モードの取得

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

  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) ) return; // ピン番号取得
  if ( getParam(data, false) ) return;                   // データ指定取得
  data = data ? HIGH: LOW;

  if (! IsIO_PIN(pinno)) {
    err = ERR_GPIO;
    return;
  }
  
  // ピンモードの設定
  digitalWrite(pinno, data);
}

// PWMコマンド
// PWM ピン番号, DutyCycle
void ipwm() {
  int16_t pinno;      // ピン番号
  int16_t duty;       // デューティー値 0～255

  if ( getParam(pinno, 0, TT_MAX_PINNUM, true) ) return;    // ピン番号取得
  if ( getParam(duty,  0, 255, false) ) return;             // デューティー値

  // PWMピンとして利用可能かチェック
  if (!IsPWM_PIN(pinno)) {
    err = ERR_GPIO;
    return;    
  }
  analogWrite(pinno, duty);
}

// shiftOutコマンド SHIFTOUT dataPin, clockPin, bitOrder, value
void ishiftOut() {
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
  tone(TonePin ,freq);
  if (tm) {
    delay(tm);
    noTone(TonePin);
  }
}

//　NOTONE
void inotone() {
  noTone(TonePin);
}

// メモリ参照　PEEK(adr[,bnk])
int16_t ipeek() {
  int16_t value =0, vadr;
  uint8_t* radr;

  if (checkOpen()) return 0;
  if ( getParam(vadr, false) )  return 0;
  if (checkClose()) return 0;  
  radr = v2realAddr(vadr);
  if (radr)
    value = *radr;
  else 
    err = ERR_VALUE;
  return value;
}

// POKEコマンド POKE ADR,データ[,データ,..データ]
void ipoke() {
  uint8_t* adr;
  int16_t value;
  int16_t vadr;
  
  // アドレスの指定
  vadr = iexp(); if(err) return ; 
  if (vadr < 0 ) { err = ERR_VALUE; return; }
  if(*cip != I_COMMA) { err = ERR_SYNTAX; return; }
  //if(vadr>=V_FNT_TOP && vadr < V_GRAM_TOP) { err = ERR_RANGE; return; }
  
  // 例: 1,2,3,4,5 の連続設定処理
  do {
    adr = v2realAddr(vadr);
    if (!adr) {
      err = ERR_VALUE;
      break;
    }
    cip++;          // 中間コードポインタを次へ進める
    if (getParam(value,false)) return; 
    *((uint8_t*)adr) = (uint8_t)value;
    vadr++;
  } while(*cip == I_COMMA);
}
// I2CW関数  I2CW(I2Cアドレス, コマンドアドレス, コマンドサイズ, データアドレス, データサイズ)
int16_t ii2cw() {
#if USE_CMD_I2C == 1
  int16_t  i2cAdr, ctop, clen, top, len;
  uint8_t* ptr;
  uint8_t* cptr;
  int16_t  rc;

  if (checkOpen()) return 0;
  if (getParam(i2cAdr, 0, 0x7f, true)) return 0;
  if (getParam(ctop, 0, 32767, true)) return 0;
  if (getParam(clen, 0, 32767, true)) return 0;
  if (getParam(top, 0, 32767, true)) return 0;
  if (getParam(len, 0, 32767,false)) return 0;
  if (checkClose()) return 0;

  ptr  = v2realAddr(top);
  cptr = v2realAddr(ctop);
  if (ptr == 0 || cptr == 0 || v2realAddr(top+len) == 0 || v2realAddr(ctop+clen) == 0) 
     { err = ERR_VALUE; return 0; }

  // I2Cデータ送信
  Wire.beginTransmission(i2cAdr);
  if (clen) {
    for (uint16_t i = 0; i < clen; i++)
      Wire.write(*cptr++);
  }
  if (len) {
    for (uint16_t i = 0; i < len; i++)
      Wire.write(*ptr++);
  }
  rc =  Wire.endTransmission();
  return rc;
#else
  return 1;
#endif
}

// I2CR関数  I2CR(I2Cアドレス, 送信データアドレス, 送信データサイズ,受信データアドレス,受信データサイズ)
int16_t ii2cr() {
#if USE_CMD_I2C == 1
  int16_t  i2cAdr, sdtop, sdlen,rdtop,rdlen;
  uint8_t* sdptr;
  uint8_t* rdptr;
  int16_t tm = 0;
  int16_t  rc;
  int16_t  n;

  if (checkOpen()) return 0;
  if (getParam(i2cAdr, 0, 0x7f, true)) return 0;
  if (getParam(sdtop, 0, 32767, true)) return 0;
  if (getParam(sdlen, 0, 32767, true)) return 0;
  if (getParam(rdtop, 0, 32767, true)) return 0;
  if (getParam(rdlen, 0, 32767,false)) return 0;
  if (checkClose()) return 0;

  sdptr = v2realAddr(sdtop);
  rdptr = v2realAddr(rdtop);
  if (sdptr == 0 || rdptr == 0 || v2realAddr(sdtop+sdlen) == 0 || v2realAddr(rdtop+rdlen) == 0) 
     { err = ERR_VALUE; return 0; }
 
  // I2Cデータ送受信
  Wire.beginTransmission(i2cAdr);
  
  // 送信
  if (sdlen) {
    Wire.write(sdptr, sdlen);
  }
  rc = Wire.endTransmission();
  if (rdlen) {
    if (rc!=0)
      return rc;
  if (tm) 
    delay(tm); 
    n = Wire.requestFrom(i2cAdr, rdlen);
    while (Wire.available()) {
      *(rdptr++) = Wire.read();
    }
  }  
  return rc;
#else
  return 1;
#endif
}

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
  
// SHIFTIN関数 SHIFTIN(データピン, クロックピン, オーダ[,ロジック])
int16_t ishiftIn() {
  int16_t rc;
  int16_t dataPin, clockPin;
  int16_t bitOrder;
  int16_t lgc = HIGH;

  if (checkOpen()) return 0;
  if (getParam(dataPin, 0,TT_MAX_PINNUM, true)) return 0;
  if (getParam(clockPin,0,TT_MAX_PINNUM, true)) return 0;
  if (getParam(bitOrder,0,1, false)) return 0;
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(lgc,LOW, HIGH, false)) return 0;
  }
  if (checkClose()) return 0;
  rc = _shiftIn((uint8_t)dataPin, (uint8_t)clockPin, (uint8_t)bitOrder, lgc);
  return rc;
}

// PULSEIN関数 PULSEIN(ピン, 検出モード, タイムアウト時間[,単位指定])
int16_t ipulseIn() {
  int32_t rc=0;
  int16_t dataPin;       // ピン
  int16_t mode;          // 検出モード
  int16_t tmout;         // タイムアウト時間(0:ミリ秒 1:マイクロ秒)
  int16_t scale =1;      // 戻り値単位指定 (1～32767)

  // コマンドライン引数の取得
  if (checkOpen()) return 0;                              // '('のチェック
  if (getParam(dataPin, 0,TT_MAX_PINNUM, true)) return 0; // ピン
  if (getParam(mode, LOW, HIGH, true)) return 0;          // 検出モード
  if (getParam(tmout,0,32767, false)) return 0;           // タイムアウト時間(ミリ秒)
  if (*cip == I_COMMA) {
    cip++;
    if (getParam(scale,1, 32767, false)) return 0;        // 戻り値単位指定 (1～32767)
  }
  if (checkClose()) return 0;                             // ')'のチェック
  
  rc = pulseIn(dataPin, mode, ((uint32_t)tmout)*1000)/scale;  // パルス幅計測
  if (rc > 32767) rc = -1; // オーバーフロー時は-1を返す
  
  return rc;
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
  Wire.requestFrom(0x68, num);
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
  uint16_t rcv[4],d;
  if (readRTC(rcv,3,4))
    return;
  rcv[0]+=2000;
  setValueTo(rcv, 4);
}

// GETDATEコマンド  SETDATE 時格納変数,分格納変数, 秒格納変数
void igetTime() {
  uint16_t rcv[3],d;
  if (readRTC(rcv,0,3))
    return;
  setValueTo(rcv, 3);
}

// DATEコマンド
KW(w1,"Sun");KW(w2,"Mon");KW(w3,"Tue");KW(w4,"Wed");KW(w5,"Thr");KW(w6,"Fri");KW(w7,"Sat");
const char*  const weekstr[] PROGMEM = {w1,w2,w3,w4,w5,w6,w7};
void idate(uint8_t devno=0) {
  uint16_t rcv[7],d;

  if (readRTC(rcv,0,7))
    return;
       
   putnum((int16_t)rcv[0]+2000, -4 ,devno);
   c_putch('/',devno);
   putnum((int16_t)rcv[1], -2,devno);
   c_putch('/',devno);
   putnum((int16_t)rcv[2], -2,devno);
   c_puts_P((const char*)F(" ["),devno);
   c_puts_P((const char*)pgm_read_word(&weekstr[rcv[3]-1]),devno);
   c_puts("] ",devno);
   putnum((int16_t)rcv[4], -2,devno);
   c_putch(':',devno);
   putnum((int16_t)rcv[5], -2,devno);
   c_putch(':',devno);
   putnum((int16_t)rcv[6], -2,devno);
   newline(devno);
}

// DATE$()
void idatestr(uint8_t devno=CDEV_SCREEN) {
  if (checkOpen()) return;
  if (checkClose()) return;
  idate(devno);    
}

#endif

// システム情報の表示
void iinfo() {
#if  USE_SYSINFO == 1
char top = 't';
  uint32_t adr = (uint32_t)&top;
  uint8_t* tmp = (uint8_t*)malloc(1);
  uint32_t hadr = (uint32_t)tmp;
  free(tmp);

  // スタック領域先頭アドレスの表示
  c_puts_P((const char*)F("Stack Top:"));
  putHexnum((int16_t)(adr>>16),4);putHexnum((int16_t)(adr&0xffff),4);
  newline();
  
  // ヒープ領域先頭アドレスの表示
  c_puts_P((const char*)F("Heap Top :"));
  putHexnum((int16_t)(hadr>>16),4);putHexnum((int16_t)(hadr&0xffff),4);
  newline();

  // SRAM未使用領域の表示
  c_puts_P((const char*)F("SRAM Free:"));
  putnum((int16_t)(adr-hadr),0);
  newline(); 

  // コマンドエントリー数
  c_puts_P((const char*)F("Command table:"));
  putnum((int16_t)(I_EOL+1),0);
  newline();
#endif  
}

// 中間コードの実行
// 戻り値      : 次のプログラム実行位置(行の先頭)

// メモリへの文字出力
inline void mem_putch(uint8_t c) {
  if (lbuf_pos < SIZE_LINE) {
   lbuf[lbuf_pos] = c;
   lbuf_pos++;
  }
}

// メモリ書き込みポインタのクリア
inline void clearlbuf() {
  lbuf_pos=0;
  memset(lbuf,0,SIZE_LINE);
}

// 指定した行の前の行番号を取得する
int16_t getPrevLineNo(int16_t lineno) {
  uint8_t* lp, *prv_lp = NULL;
  int16_t rc = -1;
  for ( lp = listbuf; *lp && (getlineno(lp) < lineno); lp += *lp) {
    prv_lp = lp;
  }
  if (prv_lp)
    rc = getlineno(prv_lp);
  return rc;
}

// 指定した行の次の行番号を取得する
int16_t getNextLineNo(int16_t lineno) {
  uint8_t* lp;
  int16_t rc = -1;
  
  lp = getlp(lineno); 
  if (lineno == getlineno(lp)) { 
    // 次の行に移動
    lp+=*lp;
    rc = getlineno(lp);
  }
  return rc;
}

// 指定した行のプログラムテキストを取得する
char* getLineStr(int16_t lineno) {
    uint8_t* lp = getlp(lineno);
    if (lineno != getlineno(lp)) 
      return NULL;
    
    // 行バッファへの指定行テキストの出力
    clearlbuf(); // バッファのクリア
    putnum(lineno, 0,3); // 行番号を表示
    c_putch(' ',3);      // 空白を入れる
    putlist(lp+3,3);     // 行番号より後ろを文字列に変換して表示
    c_putch(0,3);        // \0を入れる
    return lbuf;
}

// PRINT handler
void iprint(uint8_t devno=0,uint8_t nonewln=0) {
  int16_t value;     //値
  int16_t len;       //桁数
  uint8_t i;         //文字数
  
  len = 0; //桁数を初期化
  while (*cip != I_COLON && *cip != I_EOL) { //文末まで繰り返す
    switch (*cip++) { //中間コードで分岐
    case I_STR:   //文字列
      i = *cip++; //文字数を取得
      while (i--) //文字数だけ繰り返す
        c_putch(*cip++, devno); //文字を表示
      break; 

    case I_SHARP: //「#
      len = iexp(); //桁数を取得
      if (err) {
        return;
      }
      break; 

    case I_CHR:    ichr(devno); break; // CHR$()関数
    case I_HEX:    ihex(devno); break; // HEX$()関数
    case I_BIN:    ibin(devno); break; // BIN$()関数     
#if USE_DMP == 1
    case I_DMP:    idmp(devno); break; // DMP$()関数
#endif
    case I_STRREF: iwstr(devno); break; // STR$()関数
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
    case I_DATESTR:idatestr(devno); break; // DATE$()関数
#endif
    case I_ELSE:        // ELSE文がある場合は打ち切る
       newline(devno);
       return;
       break;

    default: //以上のいずれにも該当しなかった場合（式とみなす）
      cip--;
      value = iexp();   // 値を取得
      if (err) {
        newline();
        return;
      }
      putnum(value, len,devno); // 値を表示
      break;
    } //中間コードで分岐の末尾
    
    if (err)  {
        newline(devno);
        return;
    }
    if (nonewln && *cip == I_COMMA) { // 文字列引数流用時はここで終了
        return;
    }
  
    if (*cip == I_ELSE) {
        newline(devno); 
        return;
    } else 
   
    if (*cip == I_COMMA || *cip == I_SEMI) { // もし',' ';'があったら
      cip++;
      if (*cip == I_COLON || *cip == I_EOL || *cip == I_ELSE) //もし文末なら
        return; 
    } else {    //',' ';'がなければ
      if (*cip != I_COLON && *cip != I_EOL) { //もし文末でなければ
        err = ERR_SYNTAX;
        newline(devno); 
        return;
      }
    }
  }
  if (!nonewln) {
    newline(devno);
  }
}

// *** サウンド(Tone/PLAY) *************************
#if USE_CMD_PLAY == 1
  uint16_t mml_Tempo   = 120; // テンポ(50～512)  
  #define   mml_len   4       // 長さ(1,2,4,8,16,32)
  #define   mml_oct   4       // 音の高さ(1～8)
  
  // note定義
  const PROGMEM  uint16_t mml_scale[] = {
    4186,  // C
    4435,  // C#
    4699,  // D
    4978,  // D#
    5274,  // E
    5588,  // F
    5920,  // F#
    6272,  // G
    6643, // G#
    7040, // A
    7459, // A#
    7902, // B
  };
  
  // mml_scaleテーブルのインデックス
  #define MML_C_BASE 0
  #define MML_CS_BASE 1
  #define MML_D_BASE 2
  #define MML_DS_BASE 3
  #define MML_E_BASE 4
  #define MML_F_BASE 5
  #define MML_FS_BASE 6
  #define MML_G_BASE 7
  #define MML_GS_BASE 8
  #define MML_A_BASE 9
  #define MML_AS_BASE 10
  #define MML_B_BASE 11

  const PROGMEM  uint8_t mml_scaleBase[] = {
    MML_A_BASE,MML_B_BASE,MML_C_BASE,MML_D_BASE,MML_E_BASE,MML_F_BASE,MML_G_BASE,
  };


// TEMPO テンポ
void itempo() {
  int16_t tempo;  
  if ( getParam(tempo, 32, 500, false) ) return; // テンポの取得
  mml_Tempo = tempo;
}

// PLAY 文字列
void iplay() {
  uint8_t* ptr = lbuf;
  uint16_t freq;              // 周波数
  uint16_t len = mml_len ;    // 共通長さ
  uint8_t  oct = mml_oct ;    // 共通高さ

  uint16_t local_len = mml_len ;    // 個別長さ
  uint8_t  local_oct = mml_oct ;    // 個別高さ
  
  uint16_t tempo = mml_Tempo; // テンポ
  int8_t  scale = 0;          // 音階
  uint32_t duration;          // 再生時間(msec)
  uint8_t flgExtlen = 0;
  
  // 引数のMMLをバッファに格納する
  clearlbuf();
  iprint(CDEV_MEMORY,1);
  if (err)
    return;

  // MMLの評価
  while(*ptr) {
    flgExtlen = 0;
    local_len = len;
    local_oct = oct;
    
    //強制的な中断の判定
    if (isBreak())
      return;

    // 英字を大文字に統一
    if (*ptr >= 'a' && *ptr <= 'z')
       *ptr = 'A' + *ptr - 'a';

    // 空白はスキップ    
    if (*ptr == ' '|| *ptr == '&') {
      ptr++;
      continue;
    }
    // 音階記号
    if (*ptr >= 'A' && *ptr <= 'G') {
      scale = pgm_read_byte(&mml_scaleBase[*ptr-'A']); // 音階コードの取得        
      ptr++;

      // 半音上げ下げ
      if (*ptr == '#' || *ptr == '+') {
        // 半音上げる
        if (scale < MML_B_BASE) {
          scale++;
        } else {
          if (local_oct < 8) {
            scale = MML_B_BASE;
            local_oct++;
          }
        }
        ptr++;
      } else if (*ptr == '-') {
        // 半音下げる
        if (scale > MML_C_BASE) {
          scale--;
        } else {
          if (local_oct > 1) {
            scale = MML_B_BASE;
            local_oct--;
          }
        }                
        ptr++;      
      } 

      // 長さの指定
      uint16_t tmpLen =0;
      char* tmpPtr = ptr;
      while(c_isdigit(*ptr)) {
         tmpLen*= 10;
         tmpLen+= *ptr - '0';
         ptr++;
      }
      if (tmpPtr != ptr) {
        // 長さ引数ありの場合、長さを評価
        if ( (tmpLen==1)||(tmpLen==2)||(tmpLen==4)||(tmpLen==8)||(tmpLen==16)||(tmpLen==32)||(tmpLen==64) ) {
          local_len = tmpLen;
        } else {
          err = ERR_MML; // 長さ指定エラー
          return;
        }
      }    

      // 半音伸ばし
      if (*ptr == '.') {
        ptr++;
        flgExtlen = 1;
      } 
    
      // 音階の再生
      duration = 240000/tempo/local_len;  // 再生時間(msec)
      if (flgExtlen)
        duration += duration>>1;
        
      freq = pgm_read_word(&mml_scale[scale])>>(8-local_oct); // 再生周波数(Hz);  
      tone(TonePin ,freq);  // 音の再生
      delay(duration);
      noTone(TonePin);
    } else if (*ptr == 'L') {  // 長さの指定     
      ptr++;
      uint16_t tmpLen =0;
      char* tmpPtr = ptr;
      while(c_isdigit(*ptr)) {
         tmpLen*= 10;
         tmpLen+= *ptr - '0';
         ptr++;
      }
      if (tmpPtr != ptr) {
        // 長さ引数ありの場合、長さを評価
        if ( (tmpLen==1)||(tmpLen==2)||(tmpLen==4)||(tmpLen==8)||(tmpLen==16)||(tmpLen==32)||(tmpLen==64) ) {
          len = tmpLen;
        } else {
          err = ERR_MML; // 長さ指定エラー
          return;
        }
      }   
    } else if (*ptr == 'O') { // オクターブの指定
      ptr++;
      uint16_t tmpOct =0;
      while(c_isdigit(*ptr)) {
         tmpOct*= 10;
         tmpOct+= *ptr - '0';
         ptr++;
      }
      if (tmpOct < 1 || tmpOct > 8) {
        err = ERR_MML; 
        return;       
      }
      oct = tmpOct;
    } else if (*ptr == 'R') { // 休符
      ptr++;      
      // 長さの指定
      uint16_t tmpLen =0;
      char* tmpPtr = ptr;
      while(c_isdigit(*ptr)) {
         tmpLen*= 10;
         tmpLen+= *ptr - '0';
         ptr++;
      }
      if (tmpPtr != ptr) {
        // 長さ引数ありの場合、長さを評価
        if ( (tmpLen==1)||(tmpLen==2)||(tmpLen==4)||(tmpLen==8)||(tmpLen==16)||(tmpLen==32)||(tmpLen==64) ) {
          local_len = tmpLen;
        } else {
          err = ERR_MML; // 長さ指定エラー
          return;
        }
      }       
      if (*ptr == '.') {
        ptr++;
        flgExtlen = 1;
      } 

      // 休符の再生
      duration = 240000/tempo/local_len;    // 再生時間(msec)
      if (flgExtlen)
        duration += duration>>1;
      delay(duration);
    } else if (*ptr == '<') { // 1オクターブ上げる
      if (oct < 8) {
        oct++;
      }
      ptr++;
    } else if (*ptr == '>') { // 1オクターブ下げる
      if (oct > 1) {
        oct--;
      }
      ptr++;
    } else if (*ptr == 'T') { // テンポの指定
      ptr++;      
      // 長さの指定
      uint32_t tmpTempo =0;
      char* tmpPtr = ptr;
      while(c_isdigit(*ptr)) {
         tmpTempo*= 10;
         tmpTempo+= *ptr - '0';
         ptr++;
      }
      if (tmpPtr == ptr) {
        err = ERR_MML; 
        return;        
      }
      if (tmpTempo < 32 || tmpTempo > 255) {
        err = ERR_MML; 
        return;                
      }
      tempo = tmpTempo;
    } else {
      err = ERR_MML; 
      return;              
    }
  }
}
#endif

// ラインエディタ
void c_gets() {
  uint8_t x,y;
  uint8_t c;               // 文字
  uint8_t len = 0;         // 文字数
  uint8_t pos = 0;         // 文字位置
  int16_t line_index = -1; // リスト参照
  int16_t tempindex;
  uint8_t *text;
  int16_t value, tmp;

  getyx(y,x);
  //strcpy(tbuf,lbuf);
  //memset(lbuf,0,SIZE_LINE);
  clearlbuf();
  while ((c = c_getch()) != KEY_CR) { //改行でなければ繰り返す
    if (c == KEY_TAB) {  // [TAB]キー
      if  (len == 0) {
        if (errorLine > 0) {
          // 空白の場合は、直前のエラー発生行の内容を表示する
          text = tlimR(getLineStr(errorLine));
        } else {
          text = tlimR(lbuf);          
        }
        if (text) {
          // 指定した行が存在する場合は、その内容を表示する
          //strcpy(lbuf,text);
          pos = 0;
          len = strlen(lbuf);
          line_redrawLine(y,pos,len);
        } else {
          c_beep();
        }   
      } else if (len) {
        // 先頭に数値を入力している場合は、行の内容を表示する
        text = lbuf;
        value = 0; //定数をクリア
        tmp   = 0; //変換過程の定数をクリア
        if (c_isdigit(*text)) { //もし文字が数字なら
           do { //次の処理をやってみる
              tmp = 10 * value + *text++ - '0'; //数字を値に変換
              if (value > tmp) { // もし前回の値より小さければ
                value = -1;
                break;
               }
               value = tmp;
            } while (c_isdigit(*text)); //文字が数字である限り繰り返す
         }
         if (value >0) {
            text = tlimR(getLineStr(value));
            if (text) {
              // 指定した行が存在する場合は、その内容を表示する
              //strcpy(lbuf,text);
              pos = 0;
              len = strlen(lbuf);
              line_redrawLine(y,pos,len);
              line_index = value;
            } else {
              c_beep();
            }
         }
      }
    } else if ( (c == KEY_BACKSPACE) && (len > 0) && (pos > 0) ) { // [BS]キー
      if (pos > 1 && isZenkaku(lbuf[pos-2])) {
        // 全角文字の場合、2文字削除
        pos-=2;
        len-=2;
        line_delCharAt(lbuf, pos);
        line_delCharAt(lbuf, pos);
      } else {      
        len--; pos--;
        line_delCharAt(lbuf, pos);
      }
      line_redrawLine(y,pos,len);
    } else if ( (c == KEY_DC) && (len > 0) && (pos < len) ) {     // [Delete]キー
      if (isZenkaku(lbuf[pos])) {
        // 全角文字の場合、2文字削除
        line_delCharAt(lbuf, pos);
        len--;
      }
      line_delCharAt(lbuf, pos);
      len--;
      line_redrawLine(y,pos,len);      
    } else if (c == KEY_LEFT && pos > 0) {                // ［←］: カーソルを前の文字に移動
      line_movePrevChar(lbuf,y,pos);
    } else if (c == KEY_RIGHT && pos < len) {             // ［→］: カーソルを次の文字に移動
      line_moveNextChar(lbuf,y,pos);
    } else if (  (c == KEY_PPAGE || c == KEY_UP )   || // ［PageUP］: 前の行のリストを表示
                 (c == KEY_NPAGE || c == KEY_DOWN ) ){ // ［PageDown］: 次の行のリストを表示 
      if (line_index == -1) {
        line_index = getlineno(listbuf);
        tempindex = line_index;
      } else {
        if (c == KEY_PPAGE || c == KEY_UP)
          tempindex = getPrevLineNo(line_index);
        else 
          tempindex = getNextLineNo(line_index);
      }
      if (tempindex > 0) {
        line_index = tempindex;
        text = tlimR(getLineStr(line_index));
        //strcpy((char *)lbuf, (char *)text);
        pos = 0;
        len = strlen(lbuf);
        line_redrawLine(y,pos,len);
      } else {
        pos = 0; len = 0;
        memset(lbuf,0,SIZE_LINE);
        line_redrawLine(y,pos,len);
      }
    } else if (c == KEY_HOME ) { // [HOME]キー
       pos = 0;
       move(y,1);
    } else if (c == KEY_END ) {  // [END]キー
       pos = len;
       move(y,pos+1);
    } else if (c == KEY_F1 ) {   // [F1]キー(画面クリア)
       icls();
       pos = 0; len = 0;
       getyx(y,x);
       memset(lbuf,0,SIZE_LINE);
       c_putch('>');
    } else if (c == KEY_F2 ) {   // [F2]キー(ラインクリア)
       pos = 0; len = 0;
       memset(lbuf,0,SIZE_LINE);
       line_redrawLine(y,pos,len);
    } else if (c>=32 && (len < (SIZE_LINE - 1))) { //表示可能な文字が入力された場合の処理（バッファのサイズを超えないこと）      
      line_insCharAt(lbuf,pos,c);
      insch(c);
      pos++;len++;
    }
  }
  newline(); //改行
  lbuf[len] = 0; //終端を置く

  if (len > 0) { //もしバッファが空でなければ
    while (c_isspace(lbuf[--len])); //末尾の空白を戻る
    lbuf[++len] = 0; //終端を置く
  }
}

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

// add or subtract calculation
short iplus() {
  short imul();
  short value, tmp; //値と演算値

  value = imul(); //値を取得
  if (err) //もしエラーが生じたら
    return -1; //終了

  for (;;) //無限に繰り返す
  switch(*cip){ //中間コードで分岐
  case I_PLUS: //足し算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = imul(); //演算値を取得
    value += tmp; //足し算を実行
    break; //ここで打ち切る

  case I_MINUS: //引き算の場合
    cip++; //中間コードポインタを次へ進める
    tmp = imul(); //演算値を取得
    value -= tmp; //引き算を実行
    break; //ここで打ち切る

  default: //以上のいずれにも該当しなかった場合
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

// The parser
short iexp() {
  short value, tmp; //値と演算値

  value = iplus(); //値を取得
  if (err) //もしエラーが生じたら
    return -1; //終了

  // conditional expression 
  for (;;) //無限に繰り返す
  switch(*cip++){ //中間コードで分岐

  case I_EQ: //「=」の場合
    tmp = iplus(); //演算値を取得
    value = (value == tmp); //真偽を判定
    break; //ここで打ち切る
  case I_NEQ:   //「!=」の場合
  case I_NEQ2:  //「<>」の場合
  //case I_SHARP: //「#」の場合
    tmp = iplus(); //演算値を取得
    value = (value != tmp); //真偽を判定
    break; //ここで打ち切る
  case I_LT: //「<」の場合
    tmp = iplus(); //演算値を取得
    value = (value < tmp); //真偽を判定
    break; //ここで打ち切る
  case I_LTE: //「<=」の場合
    tmp = iplus(); //演算値を取得
    value = (value <= tmp); //真偽を判定
    break; //ここで打ち切る
  case I_GT: //「>」の場合
    tmp = iplus(); //演算値を取得
    value = (value > tmp); //真偽を判定
    break; //ここで打ち切る
  case I_GTE: //「>=」の場合
    tmp = iplus(); //演算値を取得
    value = (value >= tmp); //真偽を判定
    break; //ここで打ち切る
 case I_LAND: // AND (論理積)
    tmp = iplus(); //演算値を取得
    value = (value && tmp); //真偽を判定
    break;
 case I_LOR: // OR (論理和)
    tmp = iplus(); //演算値を取得
    value = (value || tmp); //真偽を判定
    break; 
  default: //以上のいずれにも該当しなかった場合
    cip--;    
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

// Get value
int16_t ivalue() {
  int16_t value; //値

  switch (*cip++) { //中間コードで分岐

  //定数の取得
  case I_NUM:    // 定数の場合
  case I_HEXNUM: // 16進定数
  case I_BINNUM: // 2進数定数  
     value = *cip | *(cip + 1) << 8; //定数を取得
    cip += 2;   // 中間コードポインタを定数の次へ進める
    break;      // ここで打ち切る

  //+付きの値の取得
  case I_PLUS:        //「+」の場合
    value = ivalue(); // 値を取得
    break;            // ここで打ち切る

  //負の値の取得
  case I_MINUS:           // 「-」の場合
    value = 0 - ivalue(); // 値を取得して負の値に変換
    break;                // ここで打ち切る

  case I_LNOT:            // 「!」
    value = !ivalue();    // 値を取得してNOT演算
    break; 

  case I_BITREV:          // 「~」 ビット反転
    value = ~((uint16_t)ivalue()); //値を取得してNOT演算
    break;

  //変数の値の取得
  case I_VAR:            // 変数の場合
    value = var[*cip++]; // 変数番号から変数の値を取得して次を指し示す
    break;               // ここで打ち切る

  //括弧の値の取得
  case I_OPEN:           //「(」の場合
    cip--;
    value = getparam();  // 括弧の値を取得
    break;               // ここで打ち切る

  //配列の値の取得
  case I_ARRAY: //配列の場合
    value = getparam();       // 括弧の値を取得
    if (err)                  // もしエラーが生じたら
      break;                  // ここで打ち切る
    if (value >= SIZE_ARRY) { // もし添え字の上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      break;                  // ここで打ち切る
    }
    value = arr[value];       // 配列の値を取得
    break;                    // ここで打ち切る

  //関数の値の取得
  case I_RND: //関数RNDの場合
    value = getparam();      // 括弧の値を取得
    if (err)                 // もしエラーが生じたら
      break;                 // ここで打ち切る
    value = random(value);   // 乱数を取得
    break;                   // ここで打ち切る

  case I_ABS:                // 関数ABSの場合
    value = getparam();      // 括弧の値を取得
    if (value == -32768)
      err = ERR_VOF;
    if (err)
      break;
    if (value < 0) 
      value *= -1;           // 正負を反転
    break;

  case I_SIZE:              // 関数FREEの場合
    if (checkOpen()||checkClose()) break;
    value = getsize();      // プログラム保存領域の空きを取得
    break;

  case I_INKEY:             // 関数INKEY
   if (checkOpen()||checkClose()) break;
    value = iinkey();       // キー入力値の取得
    break;

  case I_BYTE:    value = iwlen();    break; // 関数BYTE(文字列)   
  case I_LEN:     value = iwlen(1);   break; // 関数LEN(文字列)
  case I_ASC:     value = iasc();     break; // 関数ASC(文字列)
  case I_PEEK:    value = ipeek();    break; // PEEK()関数
  case I_I2CW:    value = ii2cw();    break; // I2CW()関数
  case I_I2CR:    value = ii2cr();    break; // I2CR()関数
  case I_SHIFTIN: value = ishiftIn(); break; // SHIFTIN()関数
  case I_PULSEIN: value = ipulseIn(); break; // PLUSEIN()関数
  case I_MAP:     value = imap();     break; // 関数MAP(V,L1,H1,L2,H2)
#if USE_IR == 1
  case I_IR:      value = iir();     break;  // IR(ピン番号)
#endif
#if USE_GRADE == 1
  case I_GRADE:   value = igrade();   break; // 関数GRADE(値,配列番号,配列データ数)
#endif
  case I_TICK: // 関数TICK()
    if ((*cip == I_OPEN) && (*(cip + 1) == I_CLOSE)) {
      // 引数無し
      value = 0;
      cip+=2;
    } else {
      value = getparam(); // 括弧の値を取得
      if (err)
        break;
    }
    if(value == 0) {
        value = (millis()) & 0x7FFF;            // 0～32767msec(0～32767)
    } else if (value == 1) {
        value = (millis()/1000) & 0x7FFF;       // 0～32767sec(0～32767)
    } else {
      value = 0;                                // 引数が正しくない
      err = ERR_VALUE;
    }
    break;
      
  case I_DIN: // DIN(ピン番号)
    if (checkOpen()) break;
    if (getParam(value,0,TT_MAX_PINNUM, false)) break;
    if (checkClose()) break;
    if ( !IsIO_PIN(value) ) {
      err = ERR_GPIO;
      break;
    }
    value = digitalRead(value);  // 入力値取得
    break;

  case I_ANA: // ANA(ピン番号)
    if (checkOpen()) break;
    if (getParam(value,0,TT_MAX_PINNUM, false)) break;
    if (checkClose()) break;
    value = analogRead(value);    // 入力値取得
    break;  
  
  case I_OUTPUT:   value = OUTPUT;         break; 
  case I_INPUT_PU: value = INPUT_PULLUP;   break;
  case I_INPUT_FL: value = INPUT;          break;

  // 定数
  case I_HIGH:  value = CONST_HIGH; break;
  case I_LOW:   value = CONST_LOW;  break;
  case I_ON:    value = CONST_ON;   break;
  case I_OFF:   value = CONST_OFF;  break;
  case I_LSB:   value = CONST_LSB;  break;
  case I_MSB:   value = CONST_MSB;  break;
  
  // 画面サイズ定数の参照
  case I_CW: value = c_getWidth()   ; break;
  case I_CH: value = c_getHeight()  ; break;
  
  // キーボードコード
  case I_KUP:    value = 0x1c   ; break;
  case I_KDOWN:  value = 0x1d   ; break;
  case I_KRIGHT: value = 0x1e   ; break;
  case I_KLEFT:  value = 0x1f   ; break;
  case I_KSPACE: value = 0x20   ; break;
  case I_KENTER: value = 0x0d   ; break;

  // メモリ領域
//  case I_VRAM:  value = V_VRAM_TOP;  break;
  case I_MVAR:  value = V_VAR_TOP;   break;
  case I_MARRAY:value = V_ARRAY_TOP; break; 
  case I_MPRG:  value = V_PRG_TOP;   break;
  case I_MEM:   value = V_MEM_TOP;   break; 

  default: //以上のいずれにも該当しなかった場合
    cip--;
#ifdef ARDUINO_AVR_MEGA2560
    if (*cip >= I_A0 && *cip <=I_A15) {
#else
    if (*cip >= I_A0 && *cip <=I_A7) {
#endif
       value = 14 +  (*cip-I_A0);
       cip++;
    } else {   
      err = ERR_SYNTAX; //エラー番号をセット
    }
  }
  return value; //取得した値を持ち帰る
}

// multiply or divide calculation
short imul() {
  short value, tmp; //値と演算値

  value = ivalue(); //値を取得
  if (err) //もしエラーが生じたら
    return -1; //終了

  for (;;) //無限に繰り返す
  switch(*cip++){ //中間コードで分岐

  case I_MUL: //掛け算の場合
    tmp = ivalue(); //演算値を取得
    value *= tmp; //掛け算を実行
    break; //ここで打ち切る

  case I_DIV: //割り算の場合
    tmp = ivalue(); //演算値を取得
    if (tmp == 0) { //もし演算値が0なら
      err = ERR_DIVBY0; //エラー番号をセット
      return -1; //終了
    }
    value /= tmp; //割り算を実行
    break; //ここで打ち切る

  case I_DIVR: //剰余の場合
    tmp = ivalue(); //演算値を取得
    if (tmp == 0) { //もし演算値が0なら
      err = ERR_DIVBY0; //エラー番号をセット
      return -1; //終了
    }
    value %= tmp; //割り算を実行
    break; 

  case I_LSHIFT: // シフト演算 "<<" の場合
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)<<tmp;
    break;

  case I_RSHIFT: // シフト演算 ">>" の場合
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)>>tmp;
    break; 

   case I_AND:  // 算術積(ビット演算)
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)&((uint16_t)tmp);
    break; //ここで打ち切る

   case I_OR:   //算術和(ビット演算)
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)|((uint16_t)tmp);
    break; 

   case I_XOR: //非排他OR(ビット演算)
    tmp = ivalue(); //演算値を取得
    value =((uint16_t)value)^((uint16_t)tmp);
  
  default: //以上のいずれにも該当しなかった場合
    cip--;
    return value; //値を持ち帰る
  } //中間コードで分岐の末尾
}

unsigned char* iexe() {
  uint8_t c;    // 入力キー
  err = 0;

  while (*cip != I_EOL) { //行末まで繰り返す

  //強制的な中断の判定
  if (isBreak())
    break;

    //中間コードを実行
    switch (*cip++) { //中間コードで分岐
    case I_STR:      ilabel();          break;  // 文字列の場合(ラベル)
    case I_GOTO:     igoto();           break;  // GOTOの場合
    case I_GOSUB:    igosub();          break;  // GOSUBの場合
    case I_RETURN:   ireturn();         break;  // RETURNの場合
    case I_FOR:      ifor();            break;  // FORの場合
    case I_NEXT:     inext();           break;  // NEXTの場合
    case I_IF:       iif();             break;  // IFの場合
    case I_ELSE:                                // 単独のELSEの場合
    case I_SQUOT:                               // 'の場合
    case I_REM:      iskip();           break;  // REMの場合
    case I_END:      iend();            break;  // ENDの場合
    case I_CLS:      icls();            break;  // CLS
    case I_WAIT:     iwait();           break;  // WAIT
    case I_LOCATE:   ilocate();         break;  // LOCATE
    case I_COLOR:    icolor();          break;  // COLOR
    case I_ATTR:     iattr();           break;  // ATTR
    case I_VAR:      ivar();            break;  // 変数（LETを省略した代入文）
    case I_ARRAY:    iarray();          break;  // 配列（LETを省略した代入文）
    case I_LET:      ilet();            break;  // LET
    case I_QUEST:                               // ？
    case I_PRINT:    iprint();          break;  // PRINT
    case I_INPUT:    iinput();          break;  // INPUT
    case I_POKE:     ipoke();           break;  // POKEコマンド
    case I_GPIO:     igpio();           break;  // GPIO
    case I_DOUT:     idwrite();         break;  // OUT    
    case I_POUT:     ipwm();            break;  // PWM 
    case I_SHIFTOUT: ishiftOut();       break;  // ShiftOut

#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
    case I_CPRINT:   icprint();         break;  // CPRINT
    case I_CCLS:     iccls();           break;  // CCLS
    case I_CCURS:    iccurs();          break;  // CCURS
    case I_CLOCATE:  iclocate();        break;  // CLOCATE
    case I_CCONS:    iccons();          break;  // CCONS
    case I_CDISP:    icdisp();          break;  // CDISP
#endif
    
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1
    case I_SETDATE:  isetDate();        break;  // SETDATEコマンド
    case I_GETDATE:  igetDate();        break;  // GETDATEコマンド
    case I_GETTIME:  igetTime();        break;  // GETDATEコマンド
    case I_DATE:     idate();           break;  // DATEコマンド
#endif
#if USE_CMD_VFD == 1
    case I_VMSG:     ivmsg();           break;  // VMSG
    case I_VPUT:     ivput();           break;  // VPUT
    case I_VSCROLL:  ivscroll();        break;  // VSCROLL
    case I_VCLS:     ivcls();           break;  // VCLS
    case I_VBRIGHT:  ivbright();        break;  // VBRIGHT
    case I_VDISPLAY: ivdisplay();       break;  // VDISPLAY文を実行
#endif
    case I_TONE:     itone();           break;  // TONE
    case I_NOTONE:   inotone();         break;  // NOTONE
#if USE_CMD_PLAY == 1
    case I_PLAY:      iplay();          break;  // PLAY
    case I_TEMPO:     itempo();         break;  // TEMPO    
#endif
    case I_SYSINFO:   iinfo();          break;  // SYSINFO        

    // システムコマンド
    case I_RUN:    // 中間コードがRUNの場合
    case I_LIST:   // LIST
    case I_RENUM:  // RENUM
    case I_DELETE: // DELETE
    case I_NEW:    // NEW
    case I_LOAD:   // LOAD
    case I_SAVE:   // SAVE
    case I_ERASE:  // ERASE
    case I_FILES:  // FILES
    case I_FORMAT: // FORMAT
    case I_DRIVE:  // DRIVE

      err = ERR_COM; // エラー番号をセット
      return NULL;   // 終了
    
    case I_COLON:  // 中間コードが「:」の場合
      break; 
      
    default:       // 以上のいずれにも該当しない場合
     cip--;
     err = ERR_SYNTAX; //エラー番号をセット
     
      break; //打ち切る
    } //中間コードで分岐の末尾

    if (err) //もしエラーが生じたら
      return NULL; //終了
  } //行末まで繰り返すの末尾
  return clp + *clp; //次に実行するべき行のポインタを持ち帰る
}

// RUN command handler
void irun() {
  unsigned char* lp; //行ポインタの一時的な記憶場所

  //変数と配列の初期化
  memset(var,0,52);
  memset(arr,0,SIZE_ARRY*2);

  gstki = 0;         // GOSUBスタックインデクスを0に初期化
  lstki = 0;         // FORスタックインデクスを0に初期化
  clp = listbuf;     // 行ポインタをプログラム保存領域の先頭に設定
  c_show_curs(0); 

  while (*clp) {     // 行ポインタが末尾を指すまで繰り返す
    cip = clp + 3;   // 中間コードポインタを行番号の後ろに設定
    lp = iexe();     // 中間コードを実行して次の行の位置を得る
    if (err)         // もしエラーを生じたら      
      break;
    clp = lp;        // 行ポインタを次の行の位置へ移動
  } // 行ポインタが末尾を指すまで繰り返すの末尾
  c_show_curs(1); 
}

// エラーメッセージ出力
// 引数: dlfCmd プログラム実行時 false、コマンド実行時 true
void error(uint8_t flgCmd = false) {
  c_show_curs(0);
  if (err) { //もし「OK」ではなかったら
    //もしプログラムの実行中なら（cipがリストの中にあり、clpが末尾ではない場合）
    if (cip >= listbuf && cip < listbuf + SIZE_LIST && *clp && !flgCmd) {
      // エラーメッセージを表示      
      c_puts_P((const char*)pgm_read_word(&errmsg[err]));
      c_puts_P((const char*)F(" in "));
      errorLine = getlineno(clp);
      putnum(errorLine, 0); // 行番号を調べて表示
      newline();

      // リストの該当行を表示
      putnum(getlineno(clp), 0);
      c_putch(' ');
      putlist(clp + 3);          
      newline();
    } else {
      errorLine = -1;
      c_puts_P((const char*)pgm_read_word(&errmsg[err]));
      newline();
    }
  }
  c_puts_P((const char*)pgm_read_word(&errmsg[0])); //「OK」を表示
  newline();                                        // 改行
  err = 0;                                          // エラー番号をクリア
  c_show_curs(1); 
}

//Command precessor
uint8_t icom() {
  uint8_t rc = 1;
  cip = ibuf;       // 中間コードポインタを中間コードバッファの先頭に設定
  switch (*cip++) { // 中間コードポインタが指し示す中間コードによって分岐
  case I_RUN:   irun();     break;  // RUN命令
  case I_LIST:  ilist();    break;  // LIST
  case I_RENUM: irenum();   break;  // RENUMの場合  
  case I_DELETE:idelete();  break;  // DELETE
  case I_NEW:   inew();     break;  // NEW  
  case I_LOAD:  iload();    break;  // LOAD命令を実行
  case I_SAVE:  isave();    break;  // SAVE
  case I_ERASE: ierase();   break;  // ERASE
  case I_FILES: ifiles();   break;  // FILES
  case I_FORMAT:iformat();  break;  // FORMAT
  case I_DRIVE: idrive();   break;  // DRIVE
  case I_CLS:   icls();  
  case I_REM:
  case I_SQUOT:
  case I_OK:    rc = 0;     break; // I_OKの場合
  default: //どれにも該当しない場合
    cip--;
    c_show_curs(0);  
    iexe(); //中間コードを実行
    c_show_curs(1);  
    break; //打ち切る
  }
  return rc;
}

/*
  TOYOSHIKI Tiny BASIC
  The BASIC entry point
*/

void basic() {
  unsigned char len; //中間コードの長さ

#if MYDEBUG == 1
  SSerial.begin(9600);
  SSerial.println(F("[DEBUG] Start."));
#endif 

  // mcursesの設定
  setFunction_putchar(Arduino_putchar);  // 依存関数
  setFunction_getchar(Arduino_getchar);  // 依存関数
  initscr();                             // 依存関数
  setscrreg(0,MCURSES_LINES);

#if USE_CMD_VFD == 1 
  VFD.begin();       // VFDドライバ
#endif
#if USE_CMD_I2C == 1
  Wire.begin();                      // I2C利用開始  
#endif  

#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
  // OLEDキャラクタディスプレイ
  OLEDinit();
#endif

  inew(); // 実行環境を初期化
  icls(); // 画面クリア
  //起動メッセージ
  c_puts_P((const char*)F("TOYOSHIKI TINY BASIC")); //「TOYOSHIKI TINY BASIC」を表示
  newline(); //改行
  c_puts_P((const char*)F(STR_EDITION)); //版を区別する文字列を表示
  c_putch(' ');
  c_puts_P((const char*)F(STR_VARSION)); //版を区別する文字列を表示
  newline(); //改行
  putnum(getsize(),3); c_puts_P((const char*)F("byte free")); //プログラム領域を表示
  newline(); //改行  
  error(); //「OK」またはエラーメッセージを表示してエラー番号をクリア
  
  // リセット時に指定PINがHIGHの場合、プログラム自動起動
  if (digitalRead(AutoPin)) {
    // ロードに成功したら、プログラムを実行する
    //c_show_curs(0);        // カーソル非表示
    iload(1);               // プログラムのロード
    irun();                 // RUN命令を実行
    newline();              // 改行
    error();                // エラーメッセージ出力
    err = 0; 
  }

  //端末から1行を入力して実行
  c_show_curs(1);
  for (;;) { //無限ループ
    c_putch('>'); //プロンプトを表示
    c_gets();
    if(!strlen(lbuf))        continue;

    // 1行の文字列を中間コードの並びに変換
    len = toktoi(); // 文字列を中間コードに変換して長さを取得
    if (err) {      // もしエラーが発生したら
      clp = NULL;
      error();      // エラーメッセージを表示してエラー番号をクリア
      continue;     // 繰り返しの先頭へ戻ってやり直し
    }

    // 中間コードの並びがプログラムと判断される場合
    if (*ibuf == I_NUM) { // もし中間コードバッファの先頭が行番号なら
      *ibuf = len;        // 中間コードバッファの先頭を長さに書き換える
      inslist();          // 中間コードの1行をリストへ挿入
      if (err)            // もしエラーが発生したら
        error();          // エラーメッセージを表示してエラー番号をクリア
      continue;           // 繰り返しの先頭へ戻ってやり直し
    }

    // 中間コードの並びが命令と判断される場合
    if (icom())           // 実行する
        error(false);     // エラーメッセージを表示してエラー番号をクリア

  } //無限ループの末尾
}

