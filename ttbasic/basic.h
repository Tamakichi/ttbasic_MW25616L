//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// ヘッダファイル（定数、変数・関数宣言） 2019/06/08 by たま吉さん 
// 修正 2019/06/11 GETFONTコマンドの追加（美咲フォント対応）
// 修正 2019/07/07 NeoPixel制御コマンド追加
// 修正 2019/07/13 TIMERイベント機能の追加(ON TIMER)
// 修正 2019/07/27 エラーメッセージ表示（エラーコードずれ）不具合対応
// 修正 2019/08/07 外部割込みイベント機能の追加(ON PIN)
// 修正 2019/08/08 LEDコマンド、定数の追加
// 修正 2019/08/12 SLEEP機能の追加(SLEEPコマンド)
// 修正 2019/10/08 NeoPixelのエラーメッセージの追加
//

#ifndef __basic_h__
#define __basic_h__

#include "Arduino.h"
#include "ttconfig.h"

#define MYSIGN      "TBU7"    // EPPROM識別用シグニチャ

//***  TOYOSHIKI TinyBASIC 利用ワークメモリサイズ等定義 ***
#define LINELEN   64          // 1行の文字数
#define SIZE_LINE 64          // コマンドラインテキスト有効長さ
#define SIZE_IBUF 64          // 行当たりの中間コード有効サイズ
#define SIZE_LIST PRGAREASIZE // BASICプログラム領域サイズ
#define SIZE_ARRY ARRYSIZE    // 配列利用可能数 @(0)～@(定義数-1)
#define SIZE_GSTK 6           // GOSUB stack size(2/nest)
#define SIZE_LSTK 15          // FOR stack size(5/nest)

// キャラクタ入出力デバイス選択定義 
#define CDEV_SCREEN   0  // メインスクリーン
#define CDEV_VFD      1  // VFDディスプレイ
#define CDEV_CLCD     2  // キャラクターLCD/OLCDディスプレイ
#define CDEV_MEMORY   3  // メモリー

// キーワード定義マクロ(k:キーワード用変数名 、s:キーワード文字列)
#define KW(k,s) const char k[] PROGMEM=s  

// **** GPIOピンに関する定義 ***********************
// ピン数の定義
#ifdef ARDUINO_AVR_MEGA2560
 #define TT_MAX_PINNUM 69
#else
 #define TT_MAX_PINNUM 21
#endif 

//*** 中間コード定義 *******************************
// ※並び位置をキーワードテーブルと一致させることに注意
// ※キーワードテーブルは basic.cppに定義
enum {
  I_GOTO, I_GOSUB, I_RETURN, I_END, 
  I_FOR, I_TO, I_STEP, I_NEXT,
  I_IF, I_ELSE, I_REM,
  I_INPUT, I_PRINT, I_QUEST, I_LET,
  I_COMMA, I_SEMI,I_COLON, I_SQUOT,
  I_MINUS, I_PLUS, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,
  
// 2因子演算子: "*","/","%","<<",">>","&","|","^"
  I_MUL, I_DIV, I_DIVR, I_LSHIFT, I_RSHIFT, I_AND, I_OR, I_XOR,

// 条件判定演算子
  I_EQ, I_NEQ, I_NEQ2,I_LT,I_LTE, I_GT, I_GTE, I_LAND, I_LOR,

  I_SHARP, 
  I_LNOT, I_BITREV, 
  I_ARRAY, I_RND, I_ABS, I_SIZE,

// システムコマンド
  I_RUN, I_LIST, I_RENUM, I_DELETE, I_NEW, 
  I_LOAD, I_SAVE, I_ERASE, I_FILES,
  I_FORMAT, I_DRIVE,
  I_CLS,
#if USE_CMD_VFD == 1 || USE_ALL_KEYWORD == 1
  I_VMSG, I_VCLS, I_VSCROLL, I_VBRIGHT, I_VDISPLAY, I_VPUT,
#endif
  I_WAIT,
  I_CHR,  I_HEX, I_BIN, I_STRREF,
  I_BYTE, I_LEN, I_ASC,
  I_COLOR, I_ATTR, I_LOCATE, I_INKEY,
  I_GPIO, I_DOUT, I_POUT,
  I_DIN, I_ANA,
  I_TONE, I_NOTONE,
#if USE_CMD_PLAY == 1 || USE_ALL_KEYWORD == 1
  I_PLAY, I_TEMPO,
#endif
  I_SYSINFO,

// 仮想アドレス
  I_MVAR, I_MARRAY, I_MPRG, I_MEM, I_MEM2, 
  
  I_PEEK, I_POKE, I_I2CW, I_I2CR, I_TICK,
  I_MAP, I_GRADE, I_SHIFTOUT, I_PULSEIN, I_DMP, I_SHIFTIN,

// 定数    
  I_OUTPUT, I_INPUT_PU, I_INPUT_FL,
  I_OFF, I_ON,  I_LOW, I_HIGH,I_LSB, I_MSB,
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_CW, I_CH,  I_CHANGE, I_FALLING, I_RISING,
  I_LED,

#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  I_DATE, I_GETDATE, I_GETTIME, I_SETDATE, I_DATESTR,  // RTC関連コマンド(5)  
#endif 
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  I_CPRINT, I_CCLS, I_CCURS, I_CLOCATE, I_CCONS, I_CDISP,
#endif
#if USE_ANADEF == 1 || USE_ALL_KEYWORD == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif  
#endif
#if USE_IR == 1 || USE_ALL_KEYWORD == 1
  I_IR,
#endif
// 美咲フォントの利用
#if USE_MISAKIFONT != 0 || USE_ALL_KEYWORD == 1
  I_GETFONT,
#endif
// NeoPixelの利用
#if USE_NEOPIXEL || USE_ALL_KEYWORD == 1
  I_NINIT, I_NBRIGHT, I_NCLS, I_NSET, I_NPSET, I_NMSG, I_NUPDATE, I_NSHIFT, 
  I_RGB, I_NLINE,I_NSCROLL, I_NPOINT,
#endif
// タイマー・外部割込みイベントの利用
#if USE_EVENT == 1 || USE_ALL_KEYWORD == 1
  I_TIMER, I_PIN, I_SLEEP,
#endif
  I_OK, 
  I_NUM, I_VAR, I_STR, I_HEXNUM, I_BINNUM,
  I_EOL
};

//*** エラーコード定義 ****************************
// ※並び位置はエラーメッセージ定義と一致させることに注意
// ※エラーメッセージ定義はbasic.cppで定義
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
#if USE_CMD_PLAY == 1 || USE_ALL_KEYWORD == 1
  ERR_PLAY_MML,
#endif
  ERR_I2CDEV,
  ERR_FNAME,
  ERR_NOFSPACE,
#if USE_EVENT == 1 || USE_ALL_KEYWORD == 1
  ERR_NOEDEF,
#endif
#if USE_NEOPIXEL == 1 || USE_ALL_KEYWORD == 1
  ERR_NINIT,
#endif
};

// GOTO/GOSUBモード
#define MODE_GOTO    0
#define MODE_GOSUB   1
#define MODE_ONGOTO  2
#define MODE_ONGOSUB 3

//*** インタプリタ用グローバル変数外部参照宣言 ******
extern uint8_t err;                  // エラーコード
extern int16_t errorLine;            // 直前のエラー発生行番号
extern int16_t arr[SIZE_ARRY];       // 配列変数領域
extern int16_t var[26];              // 変数領域（A-Z×2バイト)
extern uint8_t lbuf[SIZE_LINE];      // コマンドラインバッファ
extern uint8_t listbuf[SIZE_LIST];   // プログラム領域
extern uint8_t* cip;                 // インタプリタ中間コード参照位置
extern uint8_t* clp;                 // カレント行先頭ポインタ
extern uint8_t prevPressKey;         // 直前入力キーの値(INKEY()、[ESC]中断キー競合防止用)

extern uint8_t gstki;               // GOSUB スタック インデックス
extern uint8_t lstki;               // FOR 市タック インデックスtoktoi()

//*** 関数のプロトタイプ宣言 **********************

// 出力制御他版との互換用
uint8_t c_getch();
#define c_kbhit() (Serial.available()?c_getch():0)
#define c_getHeight()  24
#define c_getWidth()   80
#define c_IsCurs()     flgCurs

void clearlbuf() ;
void clearibuf() ;
void iprint(uint8_t devno=0,uint8_t nonewln=0);

uint8_t getParam(int16_t& prm, int16_t v_min,int16_t v_max,uint8_t flgCmma);
uint8_t getParam(uint16_t& prm, uint8_t flgCmma);
uint8_t getParam(int16_t& prm, uint8_t flgCmma);
uint8_t getParam(int32_t& prm, uint8_t flgCmma);

int16_t getparam();
uint8_t checkOpen();
uint8_t checkClose();
void iskip();
int16_t iexp(void);
void mem_putch(uint8_t c);
void putHexnum(int16_t value, uint8_t d, uint8_t devno=0);
void putBinnum(int16_t value, uint8_t d, uint8_t devno=0);
void putnum(int16_t value, int16_t d, uint8_t devno=0);
void c_putch(uint8_t c, uint8_t devno = CDEV_SCREEN) ;
uint8_t* v2realAddr(uint16_t vadr);
uint8_t isZenkaku(uint8_t c);
char c_toupper(char c);
void c_puts_P(const char *s,uint8_t devno=0);
void c_puts(const char *s, uint8_t devno=0);
void newline(uint8_t devno=CDEV_SCREEN);
uint8_t isBreak();
char* getLineStr(int16_t lineno);
char* tlimR(char* str);
char c_isspace(char c);
char c_isdigit(char c);
void putlist(uint8_t* ip, uint8_t devno=0);
void inew(void);
void putHexnum(int16_t value, uint8_t d, uint8_t devno);
uint8_t* getJumplp();
void iGotoGosub(uint8_t mode, uint16_t evtlp = 0);
void irun();
void initProgram();

// コンソール画面関連
void init_console();
uint8_t* getlp(short lineno);
int16_t getlineno(uint8_t *lp);
int16_t getPrevLineNo(int16_t lineno) ;
int16_t getNextLineNo(int16_t lineno);
void icls();
void ilocate();
void icolor() ;
void iattr() ;
void c_show_curs(uint8_t mode);
void c_newLine() ;
void c_beep();
void c_addch(uint8_t c);
int16_t getnum();
uint8_t isBreak();

int16_t getPrevLineNo(int16_t lineno);
int16_t getNextLineNo(int16_t lineno);
void line_delCharAt(uint8_t* str, uint8_t pos);
void line_insCharAt(uint8_t* str, uint8_t pos,uint8_t c);
void line_movePrevChar(uint8_t* str, uint8_t ln, uint8_t& pos);
void line_moveNextChar(uint8_t* str, uint8_t ln, uint8_t& pos);
void line_redrawLine(uint8_t ln,uint8_t pos,uint8_t len);
void c_gets();

// 追加コマンド(cmd_sub.cpp)
void ihexbin(uint8_t devno,uint8_t mode);
//void ihex(uint8_t devno=CDEV_SCREEN);
//void ibin(uint8_t devno=CDEV_SCREEN);
void ichr(uint8_t devno=CDEV_SCREEN);
int16_t imap();
int16_t igrade();
int16_t iasc();
int16_t iwlen(uint8_t flgZen=0);
void idmp(uint8_t devno=CDEV_SCREEN);
void iwstr(uint8_t devno=CDEV_SCREEN);
int16_t ipeek();
void ipoke();

// ファイルシステム
#define MODE_LOAD 0
#define MODE_SAVE 1
void iLoadSave(uint8_t mode,uint8_t flgskip=0);
uint8_t getFname(uint8_t* fname, uint8_t limit);
void ierase();
void ifiles();

// MW25616L VFDディスプレイ(オプション) 機能
void VFD_init();
void ivmsg(uint8_t flgZ=0);
void ivput();
void ivcls();
void ivscroll();
void ivbright();
void ivdisplay();

// I2C EPPROMファイルシステム（オプション）機能
int16_t ii2cw();
int16_t ii2cr();
void iformat();
void iefiles();
void iedel();
void idrive();

// GPIO
void igpio();
void iout();
int16_t iIN();
int16_t iana();
void ipwm() ;
int16_t ishiftIn();
void ishiftOut();
int16_t ipulseIn();
int16_t iir();
void i2c_init();
int16_t ii2crw(uint8_t mode);
void iled();

// I2C RTC DS3231
void isetDate();
void igetDate();
void igetTime() ;
void idate(uint8_t devno=0);
void idatestr(uint8_t devno=CDEV_SCREEN);

// OLEDキャラクタディスプレイ
uint8_t OLEDinit();
uint8_t OLEDputch(uint8_t c);
void iclocate() ;
void iccls();
void iccons() ;
void icdisp();
void iccurs();
void icprint();

// 赤外線リモコン
uint32_t Read_IR(uint8_t pinNo,uint16_t tm) ;

// サウンド出力
void itone();
void inotone();
void itempo();
void iplay();
void mml_init();

// 美咲フォントデータ取得
int16_t igetfont();

// NeoPixel制御
void ininit();
void inupdate();
void inbright();
void incls();
int16_t iRGB();
void inset();
void inpset();
void inshift();
void inmsg();
void inLine();
void inscroll();
int16_t inpoint();

// タイマーイベント
void iOnPinTimer();
void iTimer();
void initTimerEvent();
void clerTimerEvent();
void doTimerEvent();

void clerExtEvent();
void doExtEvent();
void iPin();
void isleep();
#endif
