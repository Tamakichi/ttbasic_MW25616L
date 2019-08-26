/*
 (C)2012 Tetsuya Suzuki
 GNU General Public License
 */

//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// 2018/03/01 by たま吉さん 
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
//  修正 2018/03/25 Arduino Mega2560のA0～A7のピン番号割り付け不具合の対応
//  修正 2018/03/27 "INPUT_PU"のスペルミス対応
//  修正 2018/08/31 BIN$()の不具合修正
//  修正 2019/05/26 MML文演奏部をライブラリに置き換えた
//  修正 2019/06/06 プログラムソースを機能別に分割、冗長部の見直し
//  修正 2019/06/07 NEXT文で変数の指定を省略可能に変更
//  修正 2019/06/11 GETFONTコマンドの追加（美咲フォント対応）
//  修正 2019/06/30 行挿入不具合対応 inslist()のlenの型をuint8_tからuint16_tに変更
//  修正 2019/07/02 imul(),iexp(),ivalue()の見直し(プログラムサイズのダイエット)
//  修正 2019/07/02 定数MEM2(ユーザーワーク領域2)の追加
//  修正 2019/07/07 仮想メモリアドレス定数取得の見直し（テーブル化）
//  修正 2019/07/07 igoto()、igosub()をiGotoGosub()に統合
//  修正 2019/07/07 RENUMの引数エラーチェックの強化
//  修正 2019/07/07 NeoPixel制御コマンド追加
//  修正 2019/07/13 TIMERイベント機能の追加(ON TIMER)
//  修正 2019/07/14 キーワードの表示変更（命令文は大文字+小文字)
//  修正 2019/08/07 外部割込みイベント機能の追加(ON PIN)
//  修正 2019/08/08 LEDコマンド、定数の追加
//  修正 2019/08/12 SLEEP機能の追加(SLEEPコマンド)
//  修正 2019/08/19 PINキーワードをEXTに変更(ピン設定と混乱するため)
//  修正 2019/08/21 OUT、INの事前GPIO設定を不要に変更
//  修正 2019/08/21 キーボード変更(POUT=>PWM、INPUT_FL=>FLOAT、INPUT_PU=>PULLUP
//

#include <Arduino.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

//*** 環境設定ファイル ****************************
#include "ttconfig.h"          
#include "basic.h"

//*** 機器依存識別 ********************************
#define STR_EDITION_ \
"TOYOSHIKI TINY BASIC\n" \
"ARDUINO Extended version 0.07\n"

//*** 仮想メモリ定義 ******************************
#define V_VRAM_TOP  0x0000    // VRAM領域先頭
#define V_VAR_TOP   0x1900    // 変数領域先頭
#define V_ARRAY_TOP 0x1AA0    // 配列領域先頭getParam
#define V_PRG_TOP   0x1BA0    // プログラム領域先頭
#define V_MEM_TOP   0x2BA0    // ユーザー作業領域先頭
#define V_MEM2_TOP  0x2CA0    // ユーザー作業領域2先頭

//*** 定数 ***************************************
#define CONST_HIGH   1   // HIGH
#define CONST_LOW    0   // LOW
#define CONST_ON     1   // ON
#define CONST_OFF    0   // OFF
#define CONST_LSB    0   // LSB
#define CONST_MSB    1   // MSB

//*** 制御文字コード *******************************
#define CHAR_CR         '\r'
#define CHAR_BACKSPACE  '\b'
#define CHAR_ESCAPE     0x1B 
#define CHAR_DEL        0x02
#define CHAR_CTRL_C     3

//*** BASIC言語 キーワード定義 *********************
// ※中間コードは basic.hに定義
// キーワード定義(AVR SRAM消費軽減対策:フラシュメモリに配置）

KW(k000,"GoTo"); KW(k001,"GoSub"); KW(k002,"Return"); KW(k069,"End");
KW(k003,"For"); KW(k004,"To"); KW(k005,"Step"); KW(k006,"Next");
KW(k007,"If"); KW(k068,"Else"); KW(k008,"Rem");
KW(k010,"Input"); KW(k011,"Print"); KW(k042,"?"); KW(k012,"Let");
KW(k013,","); KW(k014,";"); KW(k036,":"); KW(k070,"\'"); 

// 2項演算子
KW(k015,"-"); KW(k016,"+");
// 2因子演算子
KW(k017,"*");KW(k018,"/");KW(k058,"%");KW(k059,"<<");KW(k060,">>");KW(k062,"&");KW(k061,"|");KW(k065,"^");
// 条件判定演算子
KW(k024,"=");KW(k066,"!=");KW(k067,"<>");KW(k026,"<");KW(k025,"<=");KW(k023,">"); KW(k021,">=");
KW(k063,"And");KW(k064,"Or");

KW(k019,"("); KW(k020,")");KW(k035,"$"); KW(k114,"`");
KW(k022,"#"); 
KW(k056,"!");KW(k057,"~");

KW(k027,"@"); KW(k028,"Rnd"); KW(k029,"Abs"); KW(k030,"Free");

// システムコマンド
KW(k032,"Run"); KW(k031,"List"); KW(k090,"Renum");KW(k142,"Delete");  KW(k033,"New"); 
KW(k044,"Load"); KW(k043,"Save"); KW(k046,"Erase"); KW(k045,"Files"); 
KW(k134,"Format"); KW(k141,"Drive");

KW(k034,"Cls"); 
 
#if USE_CMD_VFD == 1 || USE_ALL_KEYWORD == 1
KW(k037,"Vmsg");KW(k038,"Vcls"); KW(k039,"Vscroll"); KW(k040,"Vbright");
KW(k041,"Vdisplay"); KW(k106,"Vput"); 
#endif
KW(k047,"Wait");
KW(k048,"Chr$"); KW(k050,"Hex$"); KW(k051,"Bin$"); KW(k072,"Str$");
KW(k074,"Byte"); KW(k075,"Len"); KW(k076,"Asc");
KW(k052,"Color"); KW(k053,"Attr"); KW(k054,"Locate"); KW(k055,"Inkey");
KW(k078,"Gpio"); KW(k079,"Out"); KW(k080,"PWM"); KW(k086,"In"); KW(k087,"Ana");

KW(k091,"Tone"); KW(k092,"NoTone");
#if USE_CMD_PLAY == 1 || USE_ALL_KEYWORD == 1
KW(k093,"Play"); KW(k107,"Tempo");
#endif
KW(k094,"SysInfo");
KW(k100,"Peek"); KW(k101,"Poke"); KW(k102,"I2cw"); KW(k103,"I2cr"); KW(k104,"Tick"); 
KW(k108,"Map"); KW(k109,"Grade"); KW(k110,"ShiftOut"); KW(k111,"PulseIn");
KW(k112,"Dmp$");KW(k113,"ShiftIn");
// 仮想アドレス
KW(k097,"Var");KW(k098,"Array");KW(k099,"Prg");KW(k095,"Mem");KW(k172,"Mem2");
// 定数
KW(k081,"Output"); KW(k082,"PullUp"); KW(k083,"Float");                // GPIOモード
KW(k084,"Off"); KW(k085,"On"); KW(k088,"Low"); KW(k089,"High");        // ビット状態
KW(k121,"LSB"); KW(k122,"MSB");                                        // ビット方向
KW(k115,"Up"); KW(k116,"Down"); KW(k117,"Right");KW(k118,"Left");      // キーボードコード
KW(k119,"Space");KW(k120,"Enter");                                     // キーボードコード
KW(k123,"CW"); KW(k124,"CH");                                          // 画面サイズ
KW(k177,"Change");KW(k178,"Falling");KW(k179,"Rising");                // ピン変化
KW(k180,"LED");                                                        // LED(=13) or LEDコマンド
// RTC関連コマンド(5)
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
KW(k125,"Date"); KW(k126,"GetDate"); KW(k127,"GetTime"); KW(k128,"SetDate");
KW(k129,"Date$");  
#endif

// キャラクタディスプレイ
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
KW(k135,"Cprint");KW(k136,"Ccls");KW(k137,"Ccurs");KW(k138,"Clocate");
KW(k139,"Ccons");KW(k140,"Cdisp");
#endif
// アナログ入力ピン
#if USE_ANADEF == 1 || USE_ALL_KEYWORD == 1
KW(k143,"A0");KW(k144,"A1");KW(k145,"A2");KW(k146,"A3");KW(k147,"A4");
KW(k148,"A5");KW(k149,"A6");KW(k150,"A7");
  #ifdef ARDUINO_AVR_MEGA2560
    KW(k151,"A8");KW(k152,"A9");KW(k153,"A10");KW(k154,"A11");KW(k155,"A12");
    KW(k156,"A13");KW(k157,"A14");KW(k158,"A15");
  #endif
#endif
// 赤外線リモコン入力
#if USE_IR == 1 || USE_ALL_KEYWORD == 1
KW(k159,"IR");
#endif
// 美咲フォントの利用
#if USE_MISAKIFONT != 0 || USE_ALL_KEYWORD == 1
KW(k160,"GetFont");
#endif
// NeoPixelの利用
#if USE_NEOPIXEL == 1 || USE_ALL_KEYWORD == 1
KW(k161,"Ninit"); KW(k162,"Nbright"); KW(k163,"Ncls"); KW(k164,"Nset");
KW(k165,"Npset"); KW(k166,"Nmsg"); KW(k167,"Nupdate");KW(k168,"Nshift");
KW(k169,"RGB");KW(k170,"Nline"); KW(k171,"Nscroll"); KW(k173,"Npoint");
#endif
// タイマー・外部割込みイベントの利用
#if USE_EVENT == 1 || USE_ALL_KEYWORD == 1
KW(k175,"Timer"); KW(k176,"Ext"); KW(k181,"Sleep");
#endif

KW(k071,"OK");

//*** キーワードテーブル ***************************
// ※並び位置が中間コードと一致させることに注意
// ※中間コードは basic.hに定義
const char*  const kwtbl[] PROGMEM = {
  k000,k001,k002,k069,                               // "GOTO","GOSUB,"RETURN","END"
  k003,k004,k005,k006,                               // "FOR","TO","STEP","NEXT"
  k007,k068,k008,                                    // "IF","ELSE","REM"
  k010,k011,k042,k012,                               // "INPUT","PRINT","?","LET"
  k013,k014,k036,k070,                               // ",",";",":","\'"
  k015,k016,k019,k020,k035,k114,                     // "-","+","(",")","$","`",
  k017,k018,k058,k059,k060,k062,k061,k065,           // 2因子演算子:   "*","/","%","<<",">>","&","|","^"
  k024,k066,k067,k026,k025,k023,k021,k063,k064,      // 条件判定演算子: "=","!=","<>","<","<=",">",">=","AND","OR"
  k022,                                              // "#",
  k056,k057,                                         // "!","~",                                          
  k027,k028,k029,k030,                               // "@","RND","ABS","FREE"

  // システムコマンド 
  k032,k031,k090,k142,k033,                          // "RUN","LIST","RENUM","DELETE","NEW",
  k044,k043,k046,k045,                               // "LOAD","SAVE","ERASE","FILES",
  k134,k141,                                         // "FORMAT","DRIVE"

  k034,                                               // "CLS"  
#if USE_CMD_VFD == 1 || USE_ALL_KEYWORD == 1
  k037,k038,k039,k040,k041,k106,                     // "VMSG","VCLS","VSCROLL","VBRIGHT","VDISPLAY","VPUT"
#endif
  k047,                                              // "WAIT"
  k048,k050,k051,k072,                               // "CHR$","HEX$","BIN$","STR$","WSTR$"
  k074,k075,k076,                                    // "BYTE","LEN","ASC","WASC"
  k052,k053,k054,k055,                               // "COLOR","ATTR","LOCATE","INKEY"
  k078,k079,k080,                                    // "GPIO","OUT","POUT"
  k086,k087,                                         // "IN","ANA"
  k091,k092,                                         // "TONE","NOTONE"
#if USE_CMD_PLAY == 1 || USE_ALL_KEYWORD == 1
  k093, k107,                                        // "PLAY","TEMPO"
#endif
  k094,                                              // "SYSINFO"
  // 仮想アドレス
  k097,k098,k099,k095,k172,                          // "VAR","ARRAY","PRG","MEM","MEM2",
  
  k100,k101,k102,k103,k104,                          // "PEEK","POKE","I2CW","I2CR","TICK"
  k108,k109,k110,k111,k112,k113,                     // "MAP","GRADE","SHIFTOUT","PULSEIN","DMP$","SHIFTIN"

// 定数  
  k081,k082,k083,                                    // "OUTPUT","INPUT_PU","INPUT_FL"
  k084,k085,k088,k089,k121,k122,                     // "OFF","ON","LOW","HIGH","LSB","MSB",
  k115,k116,k117,k118,k119,k120,                     // "KUP","KDOWN","KRIGHT","KLEFT","KSPACE","KENTER"
  k123,k124,k177,k178,k179,                          // "CW","CH","Change","Falling","Rising"
  k180,                                              // "LED",

#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  k125,k126,k127,k128, k129,                         // "DATE","GETDATE","GETTIME","SETDATE","DATE$"
#endif
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  k135,k136,k137,k138,k139,k140,                     // "CPRINT","CCLS","CCURS","CLOCATE","CCONS","CDISP";  
#endif
#if USE_ANADEF == 1 || USE_ALL_KEYWORD == 1
  k143,k144,k145,k146,k147,k148,k149,k150,           // "A0","A1","A2","A3","A4","A5","A6","A7",
  #ifdef ARDUINO_AVR_MEGA2560
    k151,k152,k153,k154,k155,k156,k157,k158,         // "A8","A9","A10","A11","A12","A13","A14","A15",
  #endif
#endif
#if USE_IR == 1 || USE_ALL_KEYWORD == 1
  k159,                                              // "IR"
#endif
// 美咲フォントの利用
#if USE_MISAKIFONT != 0 || USE_ALL_KEYWORD == 1
  k160,
#endif
// NeoPixelの利用
#if USE_NEOPIXEL == 1 || USE_ALL_KEYWORD == 1
  k161,k162,k163,k164,k165,k166,k167,k168,k169,k170,k171,k173,
#endif
// イベントの利用
#if USE_EVENT == 1 || USE_ALL_KEYWORD == 1
  k175,k176,k181,
  
#endif
  k071,                                              // "OK"
};

//*** キーワード数定義 *****************************
#define SIZE_KWTBL (sizeof(kwtbl) / sizeof(kwtbl[0]))

//*** LIST出力整形用定義テーブル ********************
// 後ろに空白を入れない中間コード
const PROGMEM unsigned char i_nsa[] = {
  I_RETURN, I_COMMA, I_SEMI, I_COLON,I_END,
  I_MINUS, I_PLUS, I_MUL, I_DIV,  I_DIVR, I_OPEN, I_CLOSE, I_DOLLAR, I_APOST,
  I_LSHIFT, I_RSHIFT, I_OR, I_AND, I_NEQ, I_NEQ2, I_XOR,
  I_GTE, I_SHARP, I_GT, I_EQ, I_LTE, I_LT, I_LNOT, I_BITREV, I_DIVR,
  I_ARRAY, I_RND, I_ABS, I_SIZE,I_CLS, I_QUEST,
  I_CHR, I_HEX, I_BIN,I_STRREF,
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  I_DATESTR,
#endif
#if USE_CMD_VFD == 1 || USE_ALL_KEYWORD == 1
  I_VCLS,
#endif
  I_INKEY,
  I_LEN, I_BYTE, I_ASC,
  I_OUTPUT, I_INPUT_PU,I_INPUT_FL,
  I_OFF, I_ON, I_DIN, I_ANA,
  I_SYSINFO,
  I_MEM, I_MVAR, I_MARRAY,I_MPRG, I_MEM2, 
  I_PEEK, I_I2CW, I_I2CR, I_TICK,
  I_MAP, I_GRADE, I_SHIFTIN, I_PULSEIN, I_DMP,
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_LSB, I_MSB,I_CW, I_CH,  
#if USE_ANADEF == 1 || USE_ALL_KEYWORD == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif  
#endif
#if USE_IR == 1 || USE_ALL_KEYWORD == 1
  I_IR,
#endif
#if USE_MISAKIFONT != 0 || USE_ALL_KEYWORD == 1
I_GETFONT,
#endif
#if USE_NEOPIXEL == 1 || USE_ALL_KEYWORD == 1
 I_RGB, I_NPOINT,
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
#if USE_CMD_VFD == 1 || USE_ALL_KEYWORD == 1
  I_VMSG, I_VPUT,
#endif
  I_GPIO, I_DOUT,I_RENUM,
  I_TONE, I_NOTONE,I_SYSINFO,I_POKE,
#if USE_CMD_PLAY == 1 || USE_ALL_KEYWORD == 1
  I_PLAY, I_TEMPO,
#endif
#if USE_RTC_DS3231 == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  I_DATE, I_GETDATE, I_GETTIME, I_SETDATE,   // RTC関連コマンド(4)  
#endif 
  I_FORMAT,I_DRIVE,
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1 || USE_ALL_KEYWORD == 1
  I_CPRINT, I_CCLS, I_CCURS, I_CLOCATE, I_CCONS, I_CDISP,  
#endif
#if USE_NEOPIXEL == 1 || USE_ALL_KEYWORD == 1
  I_NINIT, I_NBRIGHT, I_NCLS, I_NSET, I_NPSET, I_NMSG, I_NUPDATE, I_NSHIFT, I_NLINE,I_NSCROLL,
#endif
#if USE_EVENT == 1 || USE_ALL_KEYWORD == 1
  I_TIMER, I_PIN,
#endif
};

// 後ろが変数、数値、定数の場合、後ろに空白を空ける中間コード
const PROGMEM uint8_t i_sb_if_value[] = {
  I_NUM, I_STR, I_HEXNUM, I_VAR, I_BINNUM,
  I_OFF, I_ON, I_MEM, I_MVAR, I_MARRAY,I_MPRG, I_MEM2, 
  I_KUP, I_KDOWN, I_KRIGHT, I_KLEFT, I_KSPACE, I_KENTER,  // キーボードコード
  I_LSB, I_MSB,I_CW, I_CH,  
#if USE_ANADEF == 1 || USE_ALL_KEYWORD == 1
  I_A0,I_A1,I_A2,I_A3,I_A4,I_A5,I_A6,I_A7,
  #ifdef ARDUINO_AVR_MEGA2560
    I_A8,I_A9,I_A10,I_A11,I_A12,I_A13,I_A14,I_A15,
  #endif 
#endif
};

// 定数テーブル
const PROGMEM uint8_t constValue[] = {
  OUTPUT,INPUT_PULLUP,INPUT,
  CONST_OFF,CONST_ON,CONST_LOW,CONST_HIGH,CONST_LSB,CONST_MSB,
  0x1c,0x1d,0x1e,0x1f,0x20,0x0d,
  c_getWidth(),c_getHeight(),
  1,2,3,
  13,
};

// 仮想アドレステーブル
PROGMEM static const uint16_t vatable [] = {
  V_VAR_TOP, V_ARRAY_TOP,  V_PRG_TOP, V_MEM_TOP, V_MEM2_TOP,  
};

//*** LIST出力整形例外チェック関数 ******************
char sstyle(uint8_t code,const uint8_t *table, uint8_t count) {
  while(count--) //中間コードの数だけ繰り返す
    if (code == pgm_read_byte(table + count)) //もし該当の中間コードがあったら
      return 1; //1を持ち帰る
  return 0; //（なければ）0を持ち帰る
}

//*** LIST出力整形例外チェック マクロ関数 ************
#define nospacea(c) sstyle(c, i_nsa, sizeof(i_nsa))
#define nospaceb(c) sstyle(c, i_nsb, sizeof(i_nsb))
// 必ず前に空白を入れる中間コードか？
#define spacef(c) sstyle(c, i_sf, sizeof(i_sf))
// 後ろが変数、数値、定数の場合、後ろに空白を空ける中間コードか
#define spacebifValue(c) sstyle(c, i_sb_if_value, sizeof(i_sb_if_value))  

//*** エラーメッセージ定義 **************************
// メッセージ(フラッシュメモリ配置)

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
#if USE_EVENT == 1
KW(e29,"No event");
#endif

// エラーメッセージテーブル
const char*  const errmsg[] PROGMEM = {
  e00,e01,e02,e03,e04,e05,e06,e07,e08,e09,
  e10,e11,e12,e13,e14,e15,e16,e17,e18,e19,  
  e20,e21,e22,e23,e24,
#if USE_CMD_PLAY == 1
  e25,
#endif
  e26,e27,e28,
#if USE_EVENT == 1
  e29,
#endif
};

//*** エラー発生情報保持変数 ************************
uint8_t err;             // エラーコード
int16_t errorLine = -1;  // 直前のエラー発生行番号

//*** インタプリタ用グローバル変数 *******************
uint8_t lbuf[SIZE_LINE];     // コマンドラインバッファ
int16_t lbuf_pos = 0;        // コマンドライン内参照位置
uint8_t ibuf[SIZE_IBUF];     // 中間コード変換バッファ
int16_t var[26];             // 変数領域（A-Z×2バイト)
int16_t arr[SIZE_ARRY];      // 配列変数領域
uint8_t listbuf[SIZE_LIST];  // プログラム領域
uint8_t* clp;                // カレント行先頭ポインタ
uint8_t* cip;                // インタプリタ中間コード参照位置
uint8_t* gstk[SIZE_GSTK];    // GOSUB スタック
uint8_t gstki;               // GOSUB スタック インデックス
uint8_t* lstk[SIZE_LSTK];    // FOR スタック
uint8_t lstki;               // FOR 市タック インデックスtoktoi()

uint8_t prevPressKey = 0;    // 直前入力キーの値(INKEY()、[ESC]中断キー競合防止用)


//*** 関数の定義 ***********************************

//*************
//* 文字処理 ***
//*************

// 英小文字⇒大文字変換
char c_toupper(char c) { return(c <= 'z' && c >= 'a' ? c - 32 : c); }
// 表示可能文字チェック
char c_isprint(char c) { return (c); }
// 空白文字(スペース、制御文字)
char c_isspace(char c) { return(c == ' ' || (c <= 13 && c >= 9)); }
// 数値チェック
char c_isdigit(char c) { return(c <= '9' && c >= '0'); }
// 英字チェック
char c_isalpha(char c) { return ((c <= 'z' && c >= 'a') || (c <= 'Z' && c >= 'A')); }
// 全角判定
uint8_t isZenkaku(uint8_t c){
  return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc))) ? 1:0;
}

// 1桁16進数文字を整数に変換
uint16_t hex2value(char c) {
  if (c <= '9' && c >= '0')
    c -= '0';
  else if (c <= 'f' && c >= 'a')
    c = c + 10 - 'a';
  else if (c <= 'F' && c >= 'A')
    c = c + 10 - 'A' ;
  return c;
}

//***************
//* 文字入出力 ***
//***************

// 指定デバイスへの文字の出力
//  c     : 出力文字
//  devno : デバイス番号
void c_putch(uint8_t c, uint8_t devno) {
  if (devno == CDEV_SCREEN ) {
     c_addch(c);       // 標準出力 
  } else if (devno == CDEV_MEMORY) {
     mem_putch(c);     // メモリー出力
  } 
#if USE_SO1602AWWB == 1
  else if (devno == CDEV_CLCD) {
    OLEDputch(c);      // OLEDキャラクタディスプレイ出力
  }
#endif
} 

//  改行
//  devno : デバイス番号
void newline(uint8_t devno) {
 if ( devno == CDEV_SCREEN ) {
    c_newLine();     // 標準出力 
 } else if (devno == CDEV_MEMORY ) {
    mem_putch('\n'); // メモリー出力
  }
}

// 文字列出力
//  s     : 出力文字列
//  devno : デバイス番号
void c_puts(const char *s, uint8_t devno) {
  while (*s) c_putch(*s++, devno); //終端でなければ出力して繰り返す
}

// PROGMEM参照バージョン
//  s     : 出力文字列（フラッシュメモリ上）
//  devno : デバイス番号
void c_puts_P(const char *s, uint8_t devno) {
  char c;
  while( c = pgm_read_byte(s++) ) {
    c_putch(c,devno); //終端でなければ出力して繰り返す
  }
}

// 数値の入力
// INPUT文から呼び出される
int16_t getnum() {
  int16_t value = 0 , tmp = 0; // 値と計算過程の値
  char c;             // 文字
  uint8_t len =  0;   // 文字数
  uint8_t sign = 0;   // 負号
  uint8_t str[7];     // 入力文字バッファ

  // 文字入力ループ
  for(;;) {
    c = c_getch();
    if (c == CHAR_CR && len) {
        // 1文字以上入力で[ENTER]確定
        break;
    } else if (c == CHAR_CTRL_C || c == CHAR_ESCAPE) {
        // [CTRL+C] or [ESC]で入力中止
        err = ERR_CTR_C;
        break;
    } else if (((c == CHAR_BACKSPACE) || (c == CHAR_DEL)) && (len > 0)) {
        //［BS］or [DEL] かつ 1文字以上入力あり
        len--; // 文字数を1減らす
        c_putch(CHAR_BACKSPACE); c_putch(' '); c_putch(CHAR_BACKSPACE); // 文字を消す
    } else if ((len == 0 && (c == '+' || c == '-')) || (len < 6 && c_isdigit(c))) {
        // 行頭の符号および数字が入力された場合の処理（符号込みで6桁を超えないこと）        
        str[len++] = c; // バッファへ入れて文字数を1増やす
        c_putch(c); //表示
    } else {      
      c_beep();
    }
  }
  newline();    // 改行
  str[len] = 0; // 終端を置く
  if (str[0] == '-') {
    sign = 1;   // 負の値
    len = 1;    // 数字列はstr[1]以降    
  } else if (str[0] == '+') {
    len = 1;    // 数字列はstr[1]以降
  } else {
    len = 0;    // 数字列はstr[0]以降    
  }
 
  while (str[len]) {  // 終端でなければ繰り返す
    tmp = 10 * value + str[len++] - '0'; //数字を値に変換
    if (value > tmp) { // もし計算過程の値が前回より小さければ
      err = ERR_VOF;   // オーバーフローを記録
    }
    value = tmp;       // 計算過程の値を記録
  }

  if (sign)        // もし負の値なら
    return -value; // 負の値に変換して持ち帰る
  return value;    // 値を持ち帰る
}

// 実行時 強制的な中断の判定
uint8_t isBreak() {
  uint8_t c = c_kbhit();
  if (c) {
      // [ESC],［CTRL_C］キーなら中断
      if (c == CHAR_CTRL_C || c == CHAR_ESCAPE ) { 
        err = ERR_CTR_C;  // エラー番号をセット
        prevPressKey = 0;
      } else {
        prevPressKey = c;
      }
   }
   return err;
}

// 2進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し)
//  devno : デバイス番号
// 機能
// 'BBBBBBBBBBBBBBBB' B:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putBinnum(int16_t value, uint8_t d, uint8_t devno) {
  uint16_t  bin = (uint16_t)value; // 符号なし16進数として参照利用する
  uint16_t  dig = 0;               // 先頭が1から始まる桁数

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

// 10進数の出力
// 引数
//  value : 出力対象数値
//  d     : 桁指定(0で指定無し),負の値字0埋め
//  devno : デバイス番号
// 機能
// 'SNNNNN' S:符号 N:数値 or 空白
//  dで桁指定時は空白補完する
//
void putnum(int16_t value, int16_t d, uint8_t devno) {
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
  do {
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
//  devno : デバイス番号
// 機能
// 'XXXX' X:数値
//  dで桁指定時は0補完する
//  符号は考慮しない
// 
void putHexnum(int16_t value, uint8_t d, uint8_t devno) {
  uint16_t  hex = (uint16_t)value; // 符号なし16進数として参照利用する
  uint8_t   h;
  uint8_t   dig;
  
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

// 指定位置プログラムリスト出力
// 中間コードをキーワード変換し、１行分を出力する
//
//  ip    : 中間コート格納ポインタ
//  devno : デバイス番号
void putlist(uint8_t* ip, uint8_t devno) {
  uint8_t i; //ループカウンタ

  // 行末まで繰り返し処理
  while (*ip != I_EOL) { 
  
    // 中間コード・キーワードの処理
    if (*ip < SIZE_KWTBL) {
      // もしキーワードなら、キーワードテーブルの文字列を出力
      c_puts_P((char*)pgm_read_word(&(kwtbl[*ip])),devno);
      // 次の中間コードが':'でない場合、ルールをチェックし、キーワードの後ろに空白を出力
      if (*(ip+1) != I_COLON) 
        if ( ((!nospacea(*ip) || spacef(*(ip+1))) && (*ip != I_COLON) && (*ip != I_SQUOT))
        || ((*ip == I_CLOSE)&& (*(ip+1) != I_COLON  && *(ip+1) != I_EOL && !nospaceb(*(ip+1)))) 
        || ( spacebifValue(*ip) && spacebifValue(*(ip+1))) ) // もし例外にあたらなければ
           c_putch(' ',devno); // 空白を出力

      // キーワードがコメント("REM" or "'")の場合、以降の文字列をそのまま出力する
      if (*ip == I_REM||*ip == I_SQUOT) {
        ip++;                   // ポインタを文字数へ進める
        i = *ip++;              // 文字数を取得してポインタをコメントへ進める
        while (i--)             // 文字数だけ繰り返す
          c_putch(*ip++,devno); // ポインタを進めながら文字を出力
        return;
      }
      ip++;

    // 数値定数の処理
    } else if (*ip == I_NUM) {      
      // 数値を出力
      ip++;  putnum(*ip | *(ip + 1) << 8, 0,devno); 

      ip += 2;               // ポインタを次の中間コードへ進める
      if (!nospaceb(*ip))    // もし例外にあたらなければ
        c_putch(' ',devno);  // 空白を出力

    // 16進数定数の処理
    } else if (*ip == I_HEXNUM) {
      // 16進数定数を出力($+16進数定数)
      ip++; c_putch('$',devno); putHexnum(*ip | *(ip + 1) << 8, 2,devno);

      ip += 2;              // ポインタを次の中間コードへ進める
      if (!nospaceb(*ip))   // もし例外にあたらなければ
        c_putch(' ',devno); // 空白を出力

    // 2進定数の処理（値に応じて8桁 or 16桁出力）
    } else if (*ip == I_BINNUM) {
      ip++; c_putch('`',devno); //"`"を出力
      if (*(ip + 1))            // 2バイト目が1以上
          putBinnum(*ip | *(ip + 1) << 8, 16,devno); // 16桁で出力
      else
          putBinnum(*ip , 8,devno);                  // 8桁で出力

      ip += 2;                 // ポインタを次の中間コードへ進める
      if (!nospaceb(*ip))      // もし例外にあたらなければ
        c_putch(' ',devno);    // 空白を出力

    // 変数の処理  
    } else if (*ip == I_VAR) {
      ip++; //ポインタを変数番号へ進める
      c_putch(*ip++ + 'A',devno); // 変数名を取得して出力
      if (!nospaceb(*ip))         // もし例外にあたらなければ
        c_putch(' ',devno);       // 空白を出力

    // 文字列の処理    
    } else if (*ip == I_STR) {
      char c; // 文字列の括りに使われている文字（「"」または「'」）
      // 文字列の括りに使われている文字を調べる
      c = '\"';                  // 文字列の括りを仮に「"」とする
      ip++;                      // ポインタを文字数へ進める
      for (i = *ip; i; i--)      // 文字数だけ繰り返す
        if (*(ip + i) == '\"') { // もし「"」があれば
          c = '\'';              // 文字列の括りは「'」
          break;                 // 繰り返しを打ち切る
        }

      // 文字列を出力する
      c_putch(c,devno);          // 文字列の括りを出力
      i = *ip++;                 // 文字数を取得してポインタを文字列へ進める
      while (i--)                // 文字数だけ繰り返す
        c_putch(*ip++,devno);    // ポインタを進めながら文字を表示
      c_putch(c,devno);          // 文字列の括りを表示

      // もし次の中間コードが変数、ELSEだったら空白を出力
      if (*ip == I_VAR || *ip ==I_ELSE) 
        c_putch(' ',devno);

    //どれにも当てはまらなかった場合
    } else { 
      err = ERR_SYS; // エラー番号をセット
      return;        // 終了する
    }
  }
}

// エラーメッセージ出力
// 引数: dlfCmd プログラム実行時 false、コマンド実行時 true
void error(uint8_t flgCmd = false) {
  
  c_show_curs(0); // メッセージ表示中はカーソル非表示

  //「OK」でない場合、エラーメッセージを出力する
  if (err) { 
  
    // もしプログラムの実行中のエラーなら発生行の内容を付加して出力する
    //（cip がプログラム領域の中にあり、clpが末尾ではない場合）
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

    // ダイレクトにコマンドを実行しば場合、エラーメッセージのみ出力
    } else {
      errorLine = -1;
      c_puts_P((const char*)pgm_read_word(&errmsg[err]));
      newline();
    }
  }
  
  //「OK」を表示
  c_puts_P((const char*)pgm_read_word(&errmsg[0])); 
  newline();
  c_show_curs(1); // カーソル再表示
  
  // エラー番号をクリア
  err = 0;                                          
}

// メモリへの文字出力
// ※コマンドラインバッファをワーク用に利用
void mem_putch(uint8_t c) {
  if (lbuf_pos < SIZE_LINE) {
   lbuf[lbuf_pos] = c;
   lbuf_pos++;
  }
}

// メモリ書き込みポインタのクリア
// ※コマンドラインバッファをワーク用に利用
void clearlbuf() {
  lbuf_pos=0;
  memset(lbuf,0,SIZE_LINE);
}

void clearibuf() {
  memset(ibuf,0,SIZE_IBUF);
}

//*****************************
// テキスト・中間コード変換用関数 *
//*****************************

//*** コマンド・関数引数評価 ****************

// コマンド引数評価(符号付き整数の取得, 範囲チェックあり)
// 戻り値 評価した引数の値（エラー時はerrにエラーコードをセットする）
uint8_t getParam(int16_t& prm, int16_t v_min, int16_t v_max, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err &&  (prm < v_min || prm > v_max)) 
    err = ERR_VALUE;
  else if (flgCmma && *cip++ != I_COMMA) {
    err = ERR_SYNTAX;
 }
  return err;
}

// コマンド引数取得(符号付き整数の取得, 引数チェックなし)
// 戻り値 評価した引数の値（エラー時はerrにエラーコードをセットする）
uint8_t getParam(int16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(符号無し整数の取得, 引数チェックなし)
// 戻り値 評価した引数の値（エラー時はerrにエラーコードをセットする）
uint8_t getParam(uint16_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// コマンド引数取得(符号付き32ビット整数の取得, 引数チェックなし)
// 戻り値 評価した引数の値（エラー時はerrにエラーコードをセットする）
// ※この関数は、符号付き引数の範囲が-32768 ～ 32767を超える場合に利用
uint8_t getParam(int32_t& prm, uint8_t flgCmma) {
  prm = iexp(); 
  if (!err && flgCmma && *cip++ != I_COMMA) {
   err = ERR_SYNTAX;
  }
  return err;
}

// 単一引数の評価（配列の添え字評価等に利用
// 例： "(" 値 ")" の処理
int16_t getparam() {
  int16_t value; // 値
  if ( checkOpen() || getParam(value, false) || checkClose() )
    return 0;
  return value;
}

// '('チェック関数
uint8_t checkOpen() {
  if (*cip != I_OPEN) err = ERR_PAREN;
  else cip++;
  return err;
}

// ')'チェック関数
uint8_t checkClose() {
  if (*cip != I_CLOSE) err = ERR_PAREN;
  else cip++;
  return err;
}

// コマンド・関数の引数の仮想アドレスを実アドレスに変換
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
    radr = vadr - V_MEM_TOP + (uint8_t*)lbuf;    
  } else if ((vadr >= V_MEM2_TOP) && (vadr < V_MEM2_TOP+SIZE_IBUF)) { // ユーザーワーク領域2
    radr = vadr - V_MEM2_TOP + (uint8_t*)ibuf;    
  }
  return radr;
}

// キーワード検索
//[戻り値]
//  該当なし   : -1
//  見つかった : キーワードコード
//
int16_t lookup(char* str, uint8_t len) {
  int16_t fd_id;
  int16_t prv_fd_id = -1;
  uint8_t fd_len,prv_len;
  char kwtbl_str[16]; // コマンド比較用
  
  for (uint8_t j = 1; j <= len; j++) {
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

// トークンを中間コードに変換
// 戻り値 中間コードの並びの長さ or 0
uint8_t toktoi() {
  uint8_t i;              // ループカウンタ（一部の処理で中間コードに相当）
  int16_t key;            // 中間コード
  uint8_t len = 0;        // 中間コード領域のサイズ
  char* ptok;             // ひとつの単語の内部を指すポインタ
  char c;                 // 文字列の括りに使われている文字（「"」または「'」）
  uint16_t value;         // 定数
  uint32_t tmp;           // 変換過程の定数
  uint8_t  cnt;           // 桁数
  uint8_t  spcnt = 0;     // 先頭スペースカウント
  char* s = (char*)lbuf;       // 文字列バッファの内部を指すポインタ    
  while (*s) {                 // 文字列1行分の終端まで繰り返す
    while (c_isspace(*s))s++;  // 空白を読み飛ばす
    key = lookup(s, strlen(s));// キーワードを切り出し、中間コードを取得
    if (key >= 0) {    
      // 有効なキーワードあり
      if (len >= SIZE_IBUF - 1) {      // もし中間コード領域の容量チェック
        err = ERR_IBUFOF;              // 容量オーバー、エラー番号をセット
        return 0;
      }
      // 中間コードを格納
      ibuf[len++] = key;
      // キーワード長分、取り出し位置移動
      s+= strlen_P((char*)pgm_read_word(&(kwtbl[key])));
    }
    
    if (key == I_DOLLAR) {
      // 中間コードが16進数：$XXXX
      if (isHexadecimalDigit(*s)) {         // もし文字が16進数文字なら
        value = 0;                          // 定数をクリア
        cnt = 0;                            // 桁数
        do { //次の処理をやってみる
          value = (value<<4) | hex2value(*s++); // 数字を値に変換
          cnt++;
        } while (isHexadecimalDigit(*s));   // 16進数文字がある限り繰り返す

        if (cnt > 4) {      // 桁溢れチェック
          err = ERR_VOF;     // エラー番号オバーフローをセット
          return 0;          // 0を持ち帰る
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;                 // 0を持ち帰る
        }

        len--;    // I_DALLARを置き換えるために格納位置を移動
        ibuf[len++] = I_HEXNUM;  //中間コードを記録
        ibuf[len++] = value & 255; //定数の下位バイトを記録
        ibuf[len++] = value >> 8;  //定数の上位バイトを記録
      }      
    }

    // 2進数の変換を試みる $XXXX
    if (key == I_APOST) {
      if ( *s == '0'|| *s == '1' ) {    // もし文字が2進数文字なら
        value = 0;                      // 定数をクリア
        cnt = 0;                        // 桁数
        do { //次の処理をやってみる
          value = (value<<1) + (*s++)-'0' ; // 数字を値に変換
          cnt++;
        } while ( *s == '0'|| *s == '1' ); //16進数文字がある限り繰り返す

        if (cnt > 16) {      // 桁溢れチェック
          err = ERR_VOF;      // エラー番号オバーフローをセット
          return 0;
        }
  
        if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
          err = ERR_IBUFOF;         // エラー番号をセット
          return 0;
        }
        len--;    // I_APOSTを置き換えるために格納位置を移動
        ibuf[len++] = I_BINNUM;  // 中間コードを記録
        ibuf[len++] = value & 255; // 定数の下位バイトを記録
        ibuf[len++] = value >> 8;  // 定数の上位バイトを記録
      }      
    }
    
    // コメントへの変換を試みる
    if(key == I_REM || key == I_SQUOT) {       // もし中間コードがI_REMなら
      while (c_isspace(*s)) s++; //空白を読み飛ばす
      ptok = s; //コメントの先頭を指す

      for (i = 0; *ptok++; i++);      // コメントの文字数を得る
      if (len >= SIZE_IBUF - 2 - i) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;             // エラー番号をセット
        return 0;
      }

      ibuf[len++] = i;      // コメントの文字数を記録
      while (i--) {         // コメントの文字数だけ繰り返す
        ibuf[len++] = *s++; // コメントを記録
      }
      break; //文字列の処理を打ち切る（終端の処理へ進む）
    }

   if (key >= 0)  // もしすでにキーワードで変換に成功していたら以降はスキップ
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
          return 0;
        }      
        value = tmp; //0を持ち帰る
      } while (c_isdigit(*ptok)); //文字が数字である限り繰り返す

      if ( (tmp == 32768) && (len > 0) && (ibuf[len-1] != I_MINUS)) {
        err = ERR_VOF;                  // valueが32768のオーバーフローエラー
        return 0;
      }

      if (len >= SIZE_IBUF - 3) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;         // バッファオーバー
        return 0;
      }

      s = ptok; // 文字列の処理ずみの部分を詰める
      ibuf[len++] = I_NUM;       // 中間コードを記録
      ibuf[len++] = value & 255; // 定数の下位バイトを記録
      ibuf[len++] = value >> 8;  // 定数の上位バイトを記録

    //文字列への変換を試みる
    } else if (*s == '\"') { //もし文字が「"」なら
      c = *s++; //「"」か「'」を記憶して次の文字へ進む
      ptok = s; //文字列の先頭を指す
      //文字列の文字数を得る
      for (i = 0; (*ptok != c) && c_isprint(*ptok); i++)
        ptok++;
      if (len >= SIZE_IBUF - 1 - i) { // もし中間コードが長すぎたら
        err = ERR_IBUFOF;             // エラー番号をセット
        return 0;
      }
      ibuf[len++] = I_STR;  // 中間コードを記録
      ibuf[len++] = i;      // 文字列の文字数を記録
      while (i--) {         // 文字列の文字数だけ繰り返す
        ibuf[len++] = *s++; // 文字列を記録
      }
      if (*s == c) s++; //もし文字が「"」なら次の文字へ進む

    //変数への変換を試みる    
    } else if (c_isalpha(*ptok)) { // もし文字がアルファベットなら
      if (len >= SIZE_IBUF - 2) {  // もし中間コードが長すぎたら
        err = ERR_IBUFOF;          // バッファオーバー
        return 0;
      }
    
      //もし変数が3個並んだら
      if (len >= 4 && ibuf[len - 2] == I_VAR && ibuf[len - 4] == I_VAR) {
        err = ERR_SYNTAX;  // エラー番号をセット
        return 0;
      }

      ibuf[len++] = I_VAR;                  // 中間コードを記録
      ibuf[len++] = c_toupper(*ptok) - 'A'; // 変数番号を記録
      s++;                                  // 次の文字へ進む

    // どれにも当てはまらなかった場合    
    } else {
      err = ERR_SYNTAX; // エラー番号をセット
      return 0;
    }
  } // 文字列1行分の終端まで繰り返すの末尾

  ibuf[len++] = I_EOL; // 文字列1行分の終端を記録
  return len;          // 中間コードの長さを持ち帰る
}

// プログラム領域空きチェック
int16_t getsize() {
  uint8_t* lp; //ポインタ
  for (lp = listbuf; *lp; lp += *lp); //ポインタをリストの末尾へ移動
  return listbuf + SIZE_LIST - lp - 1; //残りを計算して持ち帰る
}

// 中間コード格納行の行番号取得(1行分リスト内行番号取得)
int16_t getlineno(uint8_t *lp) {
  return  (*lp == 0) ? -1: *(lp + 1) | *(lp + 2) << 8; //行番号を持ち帰る
}

// 指定行番号のリストポインタを取得
uint8_t* getlp(short lineno) {
  uint8_t *lp; // ポインタ
  for (lp = listbuf; *lp && getlineno(lp) < lineno; lp += *lp); // 先頭から末尾まで繰り返す
  return lp; // ポインタを持ち帰る
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

// 行番号から行インデックスを取得する
uint16_t getlineIndex(uint16_t lineno) {
  uint8_t *lp; // ポインタ
  uint16_t index = 0;  
  uint16_t rc = 32767;
  for (lp = listbuf; *lp; lp += *lp) {           // 先頭から末尾まで繰り返す
    if ((uint16_t)getlineno(lp) >= lineno) {     // もし指定の行番号以上なら
      rc = index;
      break;                                     // 繰り返しを打ち切る
    }
    index++;
  }
  return rc; 
}

// プログラム行数を取得する
uint16_t countLines(int16_t st=0, int16_t ed=32767) {
  uint8_t *lp; //ポインタ
  uint16_t cnt = 0;  
  int16_t  lineno;
  for (lp = listbuf; *lp; lp += *lp)  {
    lineno = getlineno(lp);
    if (lineno < 0)
      break;
    if ( (lineno >= st) && (lineno <= ed)) 
      cnt++;
  }
  return cnt;   
}

// ラベルでリストポインタを取得する
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

// 指定行の削除
// 引数
//  no :行番号
// 戻り値
// 1:正常 0:削除対象無し
bool dellist(int16_t no) {
  uint8_t *lp;      // 削除対象位置
  uint8_t *p1, *p2; // 移動先と移動元
  uint8_t  len;     // 移動の長さ

  lp = getlp(no);   // 削除位置ポインタを取得
  if (getlineno(lp) == no) {
    p1 = lp;                              // p1を挿入位置に設定
    p2 = p1 + *p1;                        // p2を次の行に設定
    while ((len = *p2) != 0) {            // 次の行の長さが0でなければ繰り返す
      while (len--)                       // 次の行の長さだけ繰り返す
        *p1++ = *p2++;                    // 前へ詰める
    }
    *p1 = 0; // リストの末尾に0を置く
    return false;
  }
  return true;
}

// プログラムリスト（中間コード領域）に行の挿入
// ibuf:挿入するプログラム
void inslist() {
  uint8_t *insp;    // listbuf領域内挿入位置
  uint8_t *p1, *p2; // 移動先と移動元
  uint16_t len;     // 移動の長さ

  // 領域容量チャック
  if (getsize() < *ibuf) {
    err = ERR_LBUFOF;   // エラー番号をセット
    return;
  }

  // 挿入位置を取得
  insp = getlp(getlineno(ibuf)); 
  
  // 同じ行番号の行が存在したらとりあえず削除
  dellist(getlineno(ibuf));

  // 行番号だけが入力された場合はここで終わる
  if (*ibuf == 4) // もし長さが4（行番号のみ）なら
    return;       // 終了する

  // 挿入のためのスペースを空ける
  for (p1 = insp; *p1; p1 += *p1); // p1をリストの末尾へ移動
  len = p1 - insp + 1;             // 移動する幅を計算
  p2 = p1 + *ibuf;                 // p2を末尾より1行の長さだけ後ろに設定
  while (len--)                    // 移動する幅だけ繰り返す
    *p2-- = *p1--;                 // 後ろへズラす

  // 行を転送する
  len = *ibuf; // 中間コードの長さを設定
  p1 = insp;   // 転送先を設定
  p2 = ibuf;   // 転送元を設定
  while (len--) // 中間コードの長さだけ繰り返す
    *p1++ = *p2++; // 転送
}

// 変数代入式の評価
// 例：A=値|式
void ivar() {
  int16_t value;  // 値
  int16_t index;  // 変数番号

  index = *cip++; // 変数番号を取得して次へ進む

  // 「=」のチェック
  if (*cip != I_EQ) {
    err = ERR_VWOEQ;  // エラー番号をセット
    return;
  } 
  cip++;

  if (*cip == I_STR) {
    // 代入対象が文字列定数の場合、文字列の仮想アドレスを代入
    cip++;
    value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
    cip += *cip+1;
  } else {
    // そうでない場合、式の評価(整数値)と代入
    value = iexp(); // 式の値を取得
    if (err)        // もしエラーが生じたら
      return;
  }
  var[index] = value; //変数へ代入
}

// 配列への代入式の評価
void iarray() {
  int16_t value; // 値
  int16_t index; // 配列の添え字

  index = getparam(); // 配列の添え字を取得
  if (err)
    return;

  // 添え字範囲チェック
  if (index >= SIZE_ARRY || index < 0 ) {
    err = ERR_SOR;   // エラー番号をセット
    return;
  }

  // 「=」のチェック
  if (*cip != I_EQ) {
    err = ERR_VWOEQ; // エラー番号をセット
    return;
  }

  // 連続代入の処理 例: @(n)=1,2,3,4,5 
  do {
    cip++; 
    if (*cip == I_STR) {
      // 代入対象が文字列定数の場合、文字列の仮想アドレスを代入   
      cip++;
      value = (int16_t)((uint32_t)cip - (uint32_t)listbuf + V_PRG_TOP);
      cip += *cip+1;
    } else {
      // そうでない場合、式の評価(整数値)と代入    
      value = iexp(); // 式の値を取得
      if (err)
        return;
    }
    
    if (index >= SIZE_ARRY) { // もし添え字が上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      return; 
    }
    
    arr[index] = value;       // 配列へ代入
    index++;
  } while(*cip == I_COMMA);
} 

// ELSE中間コードをサーチする
// 引数   : 中間コードプログラムポインタ
// 戻り値 : NULL 見つからない
//          NULL以外 LESEの次のポインタ
//
uint8_t* getELSEptr(uint8_t* p) {
  uint8_t* rc = NULL;
  uint8_t* lp;

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

// INPUT handler
void iinput() {
  int16_t value;          // 値
  int16_t index;          // 配列の添え字or変数番号
  uint8_t i;              // 文字数
  uint8_t prompt;         // プロンプト表示フラグ
  int16_t ofvalue;        // オーバーフロー時の設定値
  uint8_t flgofset =0;    // オーバーフロ時の設定値指定あり

  c_show_curs(1);
  prompt = 1;          // まだプロンプトを表示していない

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
  if (*cip == I_VAR) {
    // 変数の場合
    cip++; 
    ivar();  // 変数への代入を実行    
  } else if  (*cip == I_ARRAY) {
   // 配列の場合
    cip++; 
    iarray(); // 配列への代入を実行    
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
  int16_t  condition;   // IF文の条件値
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
    iskip(); // ELSE以降をスキップ
  }
}

// ラベル
// ラベルをスキップ
inline void ilabel() {
   cip+= *cip+1;   
}

// GOTO/GOSUB ジャンプ先リストポインタ取得
uint8_t* getJumplp() {  
  int16_t lineno;    // 行番号
  uint8_t* lp;       // 飛び先行ポインタ  
  if (*cip == I_STR) { 
    // 引数がラベル
    lp = getlpByLabel(cip);
    if (lp == NULL) {
      // 飛び先ラベルが見つからない
      err = ERR_ULN;
      return 0;
    }  
  } else {
    // 引数の行番
    lineno = iexp();                          
    if (err)  return 0;
    lp = getlp(lineno);
    if (lineno != getlineno(lp)) {
      // 飛び先行がない
      err = ERR_ULN;
      return 0; 
    }
  }
  return lp;
}

// GOTO/GOSUB
// GOTO 行番号|ラベル、GOSUB 行番号|ラベル
// mode 0:MODE_GOTO 1:MODE_GOSUB 2:MODE_ONGOTO 3:MODE_ONGOSUB
void iGotoGosub(uint8_t mode, uint16_t evtlp) {
  uint8_t* lp;

  if (mode == MODE_ONGOTO || mode == MODE_ONGOSUB)
    lp = evtlp;
  else 
    lp = getJumplp();     // 飛び先行ポインタ

  if (err)
    return;
    
  if (mode == MODE_GOSUB || mode == MODE_ONGOSUB) {
    //ポインタを退避
    if (gstki > SIZE_GSTK - 2) { // もしGOSUBスタックがいっぱいなら
      err = ERR_GSTKOF;          // エラー番号をセット
      return; 
    }
    gstk[gstki++] = clp;         // 行ポインタを退避
    gstk[gstki++] = cip;         // 中間コードポインタを退避
  }
  clp = lp;                      // 行ポインタを分岐先へ更新
  cip = clp + 3;                 // 中間コードポインタを先頭の中間コードに更新
}

// RETURN
void ireturn() {
  if (gstki < 2) {     // もしGOSUBスタックが空なら
    err = ERR_GSTKUF;  // エラー番号をセット
    return; 
  }
  cip = gstk[--gstki]; // 行ポインタを復帰
  clp = gstk[--gstki]; // 中間コードポインタを復帰
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
  if (*cip++ != I_VAR) {                       // もしNEXTの後ろに変数がなかったら
    cip--;
  } else {
    if (*cip++ != index) { // もし復帰した変数名と一致しなかったら
      err = ERR_NEXTUM;    // エラー番号をセット
      return;
    }
  }
  vstep = (int16_t)(uintptr_t)lstk[lstki - 2]; // 増分を復帰
  var[index] += vstep;                         // 変数の値を最新の開始値に更新
  vto = (short)(uintptr_t)lstk[lstki - 3];     // 終了値を復帰

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
  
  //表示開始行番号の設定
  if (*cip != I_EOL && *cip != I_COLON) {
    // 引数あり
    if (getParam(lineno,0,32767,false)) return;                // 表示開始行番号
    if (*cip == I_COMMA) {
      cip++;                                                   // カンマをスキップ
      if (getParam(endlineno,lineno,32767,false)) return;      // 表示終了行番号
     }
   }
 
  clp = getlp(lineno);         // 行ポインタを表示開始行番号へ進める

  //リストを表示する
  while (*clp) {               // 行ポインタが末尾を指すまで繰り返す
    if (isBreak())
      return;  //強制的な中断の判定
   
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

// RENUME [開始番号[,増分]]
// ※引数には、符号無し10進数定数のみ指定可能
void irenum() {
  uint16_t startLineNo = 10;  // 開始行番号
  uint16_t increase = 10;     // 増分
  uint8_t* ptr;               // プログラム領域参照ポインタ
  uint8_t  len;               // 行長さ
  uint8_t  i;                 // 中間コード参照位置
  uint16_t newnum;            // 新しい行番号
  uint16_t num;               // 現在の行番号
  uint16_t index;             // 行インデックス
  uint16_t cnt;               // プログラム行数
  
  // 開始行番号、増分引数チェック
  if (*cip != I_EOL && *cip != I_COLON) {
    // もしRENUMT命令に引数があったら
    if (*cip == I_NUM) {
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
    } else {
      err = ERR_SYNTAX;                 // カンマありで引数なしの場合はエラーとする
      return;    
    }
  }

  // 引数の有効性チェック
  cnt = countLines()-1;
  if ( (startLineNo <= 0 || increase <= 0) ||
       (startLineNo + increase * cnt > 32767) ) {
    err = ERR_VALUE;
    return;   
  }

  // ブログラム中のGOTOの飛び先行番号を付け直す
  for (clp = listbuf; *clp ; clp += *clp) {
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
  for ( clp = listbuf; *clp ; clp += *clp ) {
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
  int16_t sNo;       // 開始行番号
  int16_t eNo;       // 終了行番号
  uint8_t *lp;       // 削除位置ポインタ
  int16_t n;         // 削除対象行

  if ( getParam(sNo, false) ) return;
  if (*cip == I_COMMA) {
     cip++;
     if ( getParam(eNo, sNo,32767,false) ) return;  
  } else {
     eNo = sNo;
  }
  // 行ポインタを表示開始行番号へ進める
  lp = getlp(sNo);
  for (;;) {
     n = getlineno(lp);
     if (n < 0 || n > eNo)
        break;
     dellist(n);
  }
}

//NEW command handler
void inew(void) {
  // 変数と配列の初期化
  memset(var,0,26);
  memset(arr,0,SIZE_ARRY);

  // 実行制御用の初期化
  gstki = 0;     // GOSUBスタックインデクスを0に初期化
  lstki = 0;     // FORスタックインデクスを0に初期化
  *listbuf = 0;  // プログラム保存領域の先頭に末尾の印を置く
  clp = listbuf; // 行ポインタをプログラム保存領域の先頭に設定
}

// 時間待ち
void iwait() {
  int16_t tm;
  if ( getParam(tm, 0, 32767, false) ) return;
  delay(tm);
}

// キー入力文字コードの取得
// INKEY()関数
int16_t iinkey() {
  int16_t rc = 0;
   if (checkOpen()||checkClose()) return 0;  
  if (prevPressKey) {
    // 一時バッファに入力済キーがあればそれを使う
    rc = prevPressKey;
    prevPressKey = 0;
  } else if (c_kbhit()) {
    // キー入力
    rc = c_getch();
  }
  return rc;
}

// システム情報の表示
void iinfo() {
#if  USE_SYSINFO == 1
char top = 't';
  uint16_t adr = (uint32_t)&top;
  uint8_t* tmp = (uint8_t*)malloc(1);
  uint16_t hadr = (uint16_t)tmp;
  free(tmp);

  // スタック領域先頭アドレスの表示
  c_puts_P((const char*)F("Stack Top:"));
  putHexnum(adr,4);
  
  // ヒープ領域先頭アドレスの表示
  c_puts_P((const char*)F("\nHeap Top :"));
  putHexnum(hadr,4);

  // SRAM未使用領域の表示
  c_puts_P((const char*)F("\nSRAM Free:"));
  putnum((int16_t)(adr-hadr),0);

  // コマンドエントリー数
  c_puts_P((const char*)F("\nCommand table:"));
  putnum((int16_t)(I_EOL+1),0);
/*
  // タイマーイベント
  putnum((int16_t)(te_period),0);
  c_puts_P((const char*)F("\nTAction:"));
  putnum((int16_t)(te_action),0);
  c_puts_P((const char*)F("\nTActive:"));
  putnum((int16_t)(te_flgActive),0);
  c_puts_P((const char*)F("\nTEvent:"));
  putnum((int16_t)(te_flgEvent),0);
*/
  newline();
#endif  
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
    return (char *)lbuf;
}

// PRINT文の処理
void iprint(uint8_t devno, uint8_t nonewln) {
  int16_t value;     // 値
  int16_t len = 0;   // 桁数
  uint8_t i;         // 文字数

  // 文末まで繰り返す
  while (*cip != I_COLON && *cip != I_EOL && *cip != I_ELSE) {
    switch (*cip++) { // 中間コードで分岐
    case I_STR:       // 文字列
      i = *cip++;     // 文字数を取得
      while (i--)     // 文字数だけ繰り返す
        c_putch(*cip++, devno); //文字を表示
      break; 
    case I_SHARP:   len = iexp();    break; // 「#」 桁数を取得
    case I_CHR:     ichr(devno);     break; // CHR$()関数
    case I_HEX:     ihexbin(devno,0);break; // HEX$()関数
    case I_BIN:     ihexbin(devno,1);break; // BIN$()関数     
#if USE_DMP == 1
    case I_DMP:     idmp(devno);     break; // DMP$()関数
#endif
    case I_STRREF:  iwstr(devno);    break; // STR$()関数
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1 && USE_RTC_DS3231 == 1
    case I_DATESTR: idatestr(devno); break; // DATE$()関数
#endif
    default: // 以上のいずれにも該当しなかった場合（式とみなす）
      cip--;
      value = iexp();             // 値を取得
      if (!err) 
         putnum(value, len,devno); // 値を表示
       break;
    } // 中間コードで分岐の末尾
    
    if (err)
       break;
    if (nonewln && *cip == I_COMMA)  // 文字列引数流用時はここで終了
       return;
    if (*cip == I_COMMA || *cip == I_SEMI) { // もし',' ';'があったら
       cip++;
       if (*cip == I_COLON || *cip == I_EOL || *cip == I_ELSE) //もし文末なら
          return; 
    } else if (*cip != I_COLON && *cip != I_EOL && *cip != I_ELSE) {
      //もし文末でなければ
      err = ERR_SYNTAX;
      break;
    }
  }
  if (!nonewln) {
    newline(devno);
  }
}

// add or subtract calculation
int16_t iplus() {
  int16_t imul();
  int16_t value, tmp; // 値と演算値

  value = imul(); // 値を取得
  if (err)
    return -1;

  for (;;) // 無限に繰り返す
  switch(*cip){   // 中間コードで分岐
  case I_PLUS:    // 足し算の場合
    cip++;        // 中間コードポインタを次へ進める
    tmp = imul(); // 演算値を取得
    value += tmp; // 足し算を実行
    break;

  case I_MINUS:   // 引き算の場合
    cip++;        // 中間コードポインタを次へ進める
    tmp = imul(); // 演算値を取得
    value -= tmp; // 引き算を実行
    break;

  default: // 以上のいずれにも該当しなかった場合
    return value; 
  }
}

// The parser
int16_t iexp() {
  int16_t value, tmp; // 値と演算値
  value = iplus();    // 値を取得
  if (err)
    return -1; 

  // conditional expression 
  for (;;) { // 無限に繰り返す
    uint8_t code = *cip;
    if (code >= I_EQ && code <= I_LOR) {
      cip++;
      tmp = iplus(); // 演算値を取得
      if (code == I_EQ) {        //「=」の場合
        value = (value == tmp);
      } else if (code == I_NEQ || code == I_NEQ2) { //「!=」「<>」の場合
        value = (value != tmp);
      } else if (code == I_LT) {  //「<」の場合
        value = (value < tmp);
      } else if (code == I_LTE) { //「<=」の場合
        value = (value <= tmp);
      } else if (code == I_GT)  { //「>」の場合
        value = (value > tmp);
      } else if (code == I_GTE) { //「>=」の場合
        value = (value >= tmp);
      } else if (code == I_LAND){ // AND (論理積)
        value = (value && tmp);
      } else if (code == I_LOR){  // OR (論理和)
        value = (value || tmp);    
      }
    } else {
      break;
    }
  }
  return value;    
}

// Get value
int16_t ivalue() {
  int16_t value; // 値

  switch (*cip++) {    // 中間コードで分岐
  case I_NUM:          // 定数の場合
  case I_HEXNUM:       // 16進定数
  case I_BINNUM:       // 2進数定数  
    value = *cip | *(cip + 1) << 8; // 定数を取得
    cip += 2;    // 中間コードポインタを定数の次へ進める
    break;

  case I_PLUS:  value = ivalue();   break;  //「+」の場合   
  case I_MINUS: value = - ivalue(); break;  //「-」の場合   
  case I_LNOT:  value = !ivalue();  break;  //「!」NOT演算
  case I_BITREV:             // 「~」 ビット反転
    value = ~((uint16_t)ivalue()); //値を取得してNOT演算
    break;

   case I_VAR:               // 変数の場合
    value = var[*cip++];     // 変数番号から変数の値を取得して次を指し示す
    break;

  case I_OPEN:               //「(」の場合
    cip--;
    value = getparam();      // 括弧の値を取得
    break;

  case I_ARRAY:               // 配列の場合
    value = getparam();       // 括弧の値を取得
    if (err)                  // もしエラーが生じたら
      break;
    if (value >= SIZE_ARRY) { // もし添え字の上限を超えたら
      err = ERR_SOR;          // エラー番号をセット
      break;
    }
    value = arr[value];       // 配列の値を取得
    break;

  // 関数の値の取得
  case I_RND:                 // 関数RNDの場合
    value = getparam();       // 括弧の値を取得
    if (err)                  // もしエラーが生じたら
      break;
    value = random(value);    // 乱数を取得
    break;

  case I_ABS:                 // 関数ABSの場合
    value = getparam();       // 括弧の値を取得
    if (value == -32768)
      err = ERR_VOF;
    if (err)
      break;
    if (value < 0) 
      value *= -1;            // 正負を反転
    break;

  case I_SIZE:                // 関数FREEの場合
    if (checkOpen()||checkClose()) break;
    value = getsize();        // プログラム保存領域の空きを取得
    break;

  case I_INKEY:   value = iinkey();   break; // 関数INKEY    
  case I_BYTE:    value = iwlen();    break; // 関数BYTE(文字列)   
  case I_LEN:     value = iwlen(1);   break; // 関数LEN(文字列)
  case I_ASC:     value = iasc();     break; // 関数ASC(文字列)
  case I_PEEK:    value = ipeek();    break; // PEEK()関数
  case I_I2CW:    value = ii2crw(1);  break; // I2CW()関数
  case I_I2CR:    value = ii2crw(0);  break; // I2CR()関数
  case I_SHIFTIN: value = ishiftIn(); break; // SHIFTIN()関数
  case I_PULSEIN: value = ipulseIn(); break; // PLUSEIN()関数
  case I_MAP:     value = imap();     break; // 関数MAP(V,L1,H1,L2,H2)
#if USE_IR == 1
  case I_IR:      value = iir();      break; // IR(ピン番号)
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
      
  case I_DIN: value = iIN();  break;  // DIN(ピン番号)
  case I_ANA: value = iana();    break;  // ANA(ピン番号)
#if USE_MISAKIFONT != 0
  case I_GETFONT:value = igetfont(); break; // 美咲フォントの取得
#endif
#if USE_NEOPIXEL == 1
  case I_RGB:    value = iRGB();    break;  // RGBコード変換
  case I_NPOINT: value = inpoint(); break;  // NPOINT(X,Y)
#endif

  default: //以上のいずれにも該当しなかった場合
    cip--;
    // 仮想アドレス
    if (*cip >= I_MVAR && *cip <= I_MEM2) {
       value = pgm_read_word(vatable + *cip - I_MVAR);
       cip++;
    } else
    
    // 定数
    if (*cip >= I_OUTPUT && *cip <= I_LED) {
       value = pgm_read_byte(constValue + *cip-I_OUTPUT);
       cip++;
    } else
#if USE_ANADEF == 1
 #ifdef ARDUINO_AVR_MEGA2560
    if (*cip >= I_A0 && *cip <=I_A15) {
       value = 54 +  (*cip-I_A0);
 #else
    if (*cip >= I_A0 && *cip <=I_A7) {
       value = 14 +  (*cip-I_A0);
 #endif
       cip++;
    } else
#endif
    {
      err = ERR_SYNTAX; //エラー番号をセット
    }
  }
  return value; //取得した値を持ち帰る
}

// 2因子演算 (* / % << >>  & | ^)
int16_t imul() {
  int16_t value, tmp; // 値と演算値
  value = ivalue();   // 値を取得
  if (err)         
    return -1;
       
  for (;;) { //無限に繰り返す    
    uint8_t code = *cip;
    if(code >= I_MUL && code <= I_XOR) {
      cip++;  
      tmp = ivalue(); // 演算値を取得 
      if (code == I_MUL) {           // 掛け算 
        value *= tmp;     
      } else if (code == I_DIV) {    // 割り算
        if (tmp == 0) {
          err = ERR_DIVBY0;
        } else {
          value /= tmp;   // 割り算を実行
        }
      } else if (code == I_DIVR) {   // 商余
        if (tmp == 0) {
          err = ERR_DIVBY0;
        } else {
          value %= tmp;
        }
      } else if (code == I_LSHIFT) { // << ビット左シフト
        value =((uint16_t)value)<<tmp;       
      } else if (code == I_RSHIFT) { // >> ビット右シフト
        value =((uint16_t)value)>>tmp;  
      } else if (code == I_AND) {    // & ビットAND
        value =((uint16_t)value)&((uint16_t)tmp);        
      } else if (code == I_OR) {     // | ビットOR
        value =((uint16_t)value)|((uint16_t)tmp);        
      } else if (code == I_XOR) {    // ^ ビットXOR
        value =((uint16_t)value)^((uint16_t)tmp);        
      }
    } else {
      return value; // 値を持ち帰る      
    }
    if (err)
      return -1;
  }
}

// 中間コードの実行
// 戻り値      : 次のプログラム実行位置(行の先頭)
uint8_t* iexe() {
  err = 0;
  while (*cip != I_EOL) { //行末まで繰り返す

  //強制的な中断の判定
  if (isBreak())
    break;

    //中間コードを実行
    switch (*cip++) { //中間コードで分岐
    case I_STR:      ilabel();          break;  // 文字列の場合(ラベル)
    case I_GOTO:     iGotoGosub(MODE_GOTO);  break;  // GOTOの場合
    case I_GOSUB:    iGotoGosub(MODE_GOSUB); break;  // GOSUBの場合
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
    case I_DOUT:     iout();            break;  // OUT
    case I_LED:      iled();            break;  // LED
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
#if USE_NEOPIXEL == 1
    case I_NINIT:     ininit();         break;  // NINIT
    case I_NUPDATE:   inupdate();       break;  // NUPDATE
    case I_NBRIGHT:   inbright();       break;  // NBRIGHT
    case I_NCLS:      incls();          break;  // NCLS
    case I_NSET:      inset();          break;  // NSET
    case I_NPSET:     inpset();         break;  // NPSET        
    case I_NSHIFT:    inshift();        break;  // NSHIFT
#if USE_MISAKIFONT != 0
    case I_NMSG:      inmsg();          break;  // NMSG
#endif
    case I_NLINE:     inLine();         break;  // NLINE
    case I_NSCROLL:   inscroll();       break;  // NSCROLL
#endif
#if USE_EVENT == 1
    case I_ON:        iOnPinTimer();    break;  // ON TIMER| ON PIN
    case I_TIMER:     iTimer();         break;  // TIMER
    case I_PIN:       iPin();           break;  // PIN
#if USE_SLEEP == 1
    case I_SLEEP:     isleep();         break;  // SLEEP
#endif
#endif
    case I_SYSINFO:   iinfo();          break;  // SYSINFO           

    case I_COLON:     break; // 中間コードが「:」の場合   
      
    default:                 // 以上のいずれにも該当しない場合
     cip--;
     if (*cip >= I_RUN && *cip <= I_DRIVE) {
        err = ERR_COM; // エラー番号をセット
     } else {    
        err = ERR_SYNTAX; //エラー番号をセット
     }
     break;
    } //中間コードで分岐の末尾

    if (err)
      return NULL;
#if USE_EVENT == 1
    doTimerEvent();
    if (err)
      return NULL;
    doExtEvent();
    if (err)
      return NULL;
#endif
  } //行末まで繰り返すの末尾
  return clp + *clp; //次に実行するべき行のポインタを持ち帰る
}

// RUNコマンド
void irun() {
  uint8_t* lp; // 行ポインタの一時的な記憶場所

  // 変数と配列の初期化
  memset(var,0,52);
  memset(arr,0,SIZE_ARRY*2);

  gstki = 0;         // GOSUBスタックインデクスを0に初期化
  lstki = 0;         // FORスタックインデクスを0に初期化
  clp = listbuf;     // 行ポインタをプログラム保存領域の先頭に設定

 c_show_curs(0);    // カーソル消去
  while (*clp) {     // 行ポインタが末尾を指すまで繰り返す
    cip = clp + 3;   // 中間コードポインタを行番号の後ろに設定
    lp = iexe();     // 中間コードを実行して次の行の位置を得る
    if (err)         // もしエラーを生じたら      
      break;
    clp = lp;        // 行ポインタを次の行の位置へ移動
  }
  c_show_curs(1);    // カーソル表示
#if USE_EVENT == 1
  clerTimerEvent();
  clerExtEvent();
#endif
}

//Command precessor
uint8_t icom() {
  uint8_t rc = 1;
  cip = ibuf;       // 中間コードポインタを中間コードバッファの先頭に設定
  //c_show_curs(0);  
  switch (*cip++) { // 中間コードポインタが指し示す中間コードによって分岐
  case I_RUN:   irun();     break;  // RUN命令
  case I_LIST:  ilist();    break;  // LIST
  case I_RENUM: irenum();   break;  // RENUMの場合  
  case I_DELETE:idelete();  break;  // DELETE
  case I_NEW:   inew();     break;  // NEW  
  case I_LOAD:  iLoadSave(MODE_LOAD); break;  // LOAD
  case I_SAVE:  iLoadSave(MODE_SAVE); break;  // SAVE
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
  //c_show_curs(1);      
  return rc;
}

/*
  TOYOSHIKI Tiny BASIC
  The BASIC entry point
*/

void basic() {
  uint8_t len;     // 中間コードの長さ

  init_console();  // シリアルコンソールの初期設定

#if USE_CMD_VFD == 1 
  VFD_init();  // VFD利用開始
#endif
#if USE_CMD_I2C == 1
  i2c_init();  // I2C利用開始  
#endif  
#if USE_CMD_PLAY == 1
  mml_init();  // MML初期化
#endif
#if USE_SO1602AWWB == 1 && USE_CMD_I2C == 1
  // OLEDキャラクタディスプレイ
  OLEDinit();
#endif
#if USE_EVENT == 1
  initTimerEvent();
  clerExtEvent();
#endif

  inew(); // 実行環境を初期化
  icls(); // 画面クリア
  //起動メッセージ
  c_puts_P((const char*)F(STR_EDITION_));
  putnum(getsize(),3); c_puts_P((const char*)F("byte free\n")); //プログラム領域を表示
  error(); //「OK」またはエラーメッセージを表示してエラー番号をクリア
  
  // リセット時に指定PINがHIGHの場合、プログラム自動起動
  if (digitalRead(AutoPin)) {
    // ロードに成功したら、プログラムを実行する
    iLoadSave(MODE_LOAD,true);   // プログラムのロード
    irun();                      // RUN命令を実行
    newline();                   // 改行
    error();                     // エラーメッセージ出力
    err = 0; 
  }

  // 端末から1行を入力して実行
  c_show_curs(1);
  for (;;) { //無限ループ
    c_putch('>'); // プロンプトを表示
    c_gets();     // ラインエディタ（lbufに格納）
    if(!strlen((char*)lbuf))  continue;

    // 1行の文字列を中間コードの並びに変換
    len = toktoi(); // 文字列を中間コードに変換して長さを取得（ibufに買機能）
    if (err) {      // もしエラーが発生したら
      clp = NULL;
      error();      // エラーメッセージを表示してエラー番号をクリア
      continue;     // 繰り返しの先頭へ戻ってやり直し
    }

    // 中間コードの並びがプログラムと判断される場合
    if (*ibuf == I_NUM) { // 先頭が行番号の場合、
      *ibuf = len;        // 中間コードバッファの先頭を長さに書き換える
      inslist();          // 中間コードの1行をリストへ挿入
      if (err)            // 挿入エラー
        error();          // エラーメッセージを表示してエラー番号をクリア
      continue;           // 繰り返しの先頭へ戻ってやり直し
    } else {
    // 中間コードの並びが命令と判断される場合
      if (icom())           // 実行する
        error(false);     // エラーメッセージを表示してエラー番号をクリア
    }
  } //無限ループの末尾
}
