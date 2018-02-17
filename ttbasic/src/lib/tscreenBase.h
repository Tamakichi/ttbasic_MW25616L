// 
// スクリーン制御基本クラス ヘッダーファイル
// 作成日 2017/06/27 by たま吉さん
// 修正日 2017/09/15 IsCurs()  カーソル表示有無の取得の追加
// 修正日 2017/10/15 定義競合のためKEY_F1、KEY_F(n)をKEY_Fn1、KEY_Fn(n)変更
// 修正日 2018/01/30 制御キーのキーコード変更、全角（シフトJIS)対応
// 修正日 2018/02/02 editLine()の追加
// 修正日 2018/02/10, 基底クラスtSerialDevの廃止対応

#ifndef __tscreenBase_h__
#define __tscreenBase_h__

#define DEPEND_TTBASIC           1     // 豊四季TinyBASIC依存部利用の有無 0:利用しない 1:利用する
#define USE_EDITLINE             0     // 行編集関数editLineの利用  0:利用しない 1:利用する     

#include <Arduino.h>

// 編集キーの定義
#define SC_KEY_TAB       '\t'   // [TAB] key
#define SC_KEY_CR        '\r'   // [Enter] key
#define SC_KEY_BACKSPACE '\b'   // [Backspace] key

#define SC_KEY_ESCAPE    0x1B   // [ESC] key
#define SC_KEY_UP        0x1c   // [↑] key
#define SC_KEY_DOWN      0x1d   // [↓] key
#define SC_KEY_RIGHT     0x1e   // [→] key
#define SC_KEY_LEFT      0x1f   // [←] key
#define SC_KEY_HOME      0x01   // [Home] key
#define SC_KEY_DC        0x02   // [Delete] key
#define SC_KEY_IC        0x05   // [Insert] key
#define SC_KEY_NPAGE     0x10   // [PageDown] key
#define SC_KEY_PPAGE     0x11   // [PageUp] key
#define SC_KEY_END       0x0f   // [End] key
#define SC_KEY_BTAB      0x18   // [Back tab] key
#define SC_KEY_F1        0x0c   // Function key F1
#define SC_KEY_F2        0x04
#define SC_KEY_F3        0x0e
#define SC_KEY_F4        0x03
#define SC_KEY_F5        0x12
#define SC_KEY_F6        0x13
#define SC_KEY_F7        0x14
#define SC_KEY_F8        0x15
#define SC_KEY_F9        0x16
#define SC_KEY_F10       0x17
#define SC_KEY_F11       0x18
#define SC_KEY_F12       0x19  

// コントロールキーコードの定義
#define SC_KEY_CTRL_L 0x0c  // 画面を消去
#define SC_KEY_CTRL_R 0x12  // 画面を再表示
#define SC_KEY_CTRL_X   24  // 1文字削除(DEL)
#define SC_KEY_CTRL_C    3  // break
#define SC_KEY_CTRL_D    4  // 行削除
#define SC_KEY_CTRL_N 0x0e  // 行挿入

#define VPEEK(X,Y)      (screen[width*(Y)+(X)])
#define VPOKE(X,Y,C)    (screen[width*(Y)+(X)]=C)

class tscreenBase  {
  protected:
    uint8_t* screen;            // スクリーン用バッファ
    uint16_t width;             // スクリーン横サイズ
    uint16_t height;            // スクリーン縦サイズ
    uint16_t maxllen;           // 1行最大長さ
    uint16_t pos_x;             // カーソル横位置
    uint16_t pos_y;             // カーソル縦位置
    uint8_t*  text;             // 行確定文字列
    uint8_t flgIns;             // 編集モード
    uint8_t dev;                // 文字入力デバイス
    uint8_t flgCur;             // カーソル表示設定
    uint8_t flgExtMem;          // 外部確保メモリ利用フラグ
	
  public:
    virtual void INIT_DEV() =0;                              // デバイスの初期化
	  virtual void END_DEV() {};                               // デバイスの終了
    virtual void MOVE(uint8_t y, uint8_t x) =0;              // キャラクタカーソル移動
    virtual void WRITE(uint8_t x, uint8_t y, uint8_t c) =0;  // 文字の表示
    virtual void WRITE(uint8_t c) =0;                        // 文字の表示
    virtual void CLEAR() =0;                                 // 画面全消去
    virtual void CLEAR_LINE(uint8_t l) =0;                   // 行の消去
    virtual void SCROLL_UP()  =0;                            // スクロールアップ
    virtual void SCROLL_DOWN() =0;                           // スクロールダウン
    virtual void INSLINE(uint8_t l) =0;                      // 指定行に1行挿入(下スクロール)
    
  public:

    tscreenBase() {};
    virtual ~tscreenBase() {};
    virtual void beep() =0;                              // BEEP音の発生
    virtual void show_curs(uint8_t flg);                 // カーソルの表示/非表示
    virtual void draw_cls_curs();                        // カーソルの消去
    inline  uint8_t IsCurs() { return flgCur; };         // カーソル表示有無の取得
    virtual void putch(uint8_t c);                       // 文字の出力
    virtual void putwch(uint16_t c);                     // 文字の出力（シフトJIS対応)
    virtual uint8_t get_ch();                            // 文字の取得
    virtual uint16_t get_wch();                          // 文字の取得（シフトJIS対応)
    virtual uint8_t isKeyIn();                           // キー入力チェック
	  virtual void setColor(uint16_t fc, uint16_t bc) =0;  // 文字色指定
  	virtual void setAttr(uint16_t attr) =0;              // 文字属性
	  virtual void set_allowCtrl(uint8_t flg) {};          // シリアルからの入力制御許可設定
    virtual uint8_t IS_PRINT(uint8_t ch) {
      //return (((ch) >= 32 && (ch) < 0x7F) || ((ch) >= 0xA0)); 
     return ch;
    };
    void init(uint16_t w=0,uint16_t h=0,uint16_t ln=128, uint8_t* extmem=NULL); // スクリーンの初期設定
	  virtual void end();                               // スクリーン利用終了
    void clerLine(uint16_t l);                        // 1行分クリア
    void cls();                                       // スクリーンのクリア
    void refresh();                                   // スクリーンリフレッシュ表示
    virtual void refresh_line(uint16_t l);            // 行の再表示
    void scroll_up();                                 // 1行分スクリーンのスクロールアップ
    void scroll_down();                               // 1行分スクリーンのスクロールダウン
    void delete_char() ;                              // 現在のカーソル位置の文字削除
    inline uint8_t getDevice() {return dev;};         // 文字入力元デバイス種別の取得        ***********
    void Insert_char(uint16_t c);                     // 現在のカーソル位置に文字を挿入
    void movePosNextNewChar();                        // カーソルを１文字分次に移動
    void movePosPrevChar();                           // カーソルを1文字分前に移動
    void movePosNextChar();                           // カーソルを1文字分次に移動
    void movePosNextLineChar();                       // カーソルを次行に移動
    void movePosPrevLineChar();                       // カーソルを前行に移動
    void moveLineEnd();                               // カーソルを行末に移動
    void moveBottom();                                // スクリーン表示の最終表示の行先頭に移動
    void locate(uint16_t x, uint16_t y);              // カーソルを指定位置に移動
    virtual uint8_t edit();                           // スクリーン編集
    uint8_t enter_text();                             // 行入力確定ハンドラ
    virtual void newLine();                           // 改行出力
    void Insert_newLine(uint16_t l);                  // 指定行に空白挿入
    uint8_t edit_scrollUp();                          // スクロールして前行の表示
    uint8_t edit_scrollDown();                        // スクロールして次行の表示
    uint16_t vpeek(uint16_t x, uint16_t y);           // カーソル位置の文字コード取得
    
    uint8_t *getText() { return &text[0]; };   // 確定入力の行データアドレス参照
    uint8_t *getScreen() { return screen; };   // スクリーン用バッファアドレス参照
    uint16_t c_x() { return pos_x;};           // 現在のカーソル横位置参照
    uint16_t c_y() { return pos_y;};           // 現在のカーソル縦位置参照
    uint16_t getWidth() { return width;};      // スクリーン横幅取得
    uint16_t getHeight() { return height;};    // スクリーン縦幅取得
    uint16_t getScreenByteSize() {return width*height;}; // スクリーン領域バイトサイズ
    int16_t getLineNum(int16_t l);             // 指定行の行番号の取得
    uint8_t isShiftJIS(uint8_t  c) {           // シフトJIS1バイト目チェック
      return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc)))?1:0;
    };
    
    void splitLine();                           // カーソル位置で行を分割する
    void margeLine();                           // 現在行の末尾に次の行を結合する
    void deleteLine(uint16_t l);                // 指定行を削除
#if USE_EDITLINE == 1
    virtual uint8_t editLine();                 // ライン編集
#endif
 };


#endif

