#include "Arduino.h"
#include "basic.h"

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

// BEEP
void c_beep() {
  addch(0x07);
}

void c_addch(uint8_t c) {
  if (c=='\n')
    c_newLine();
  else
    addch(c);
}

uint8_t c_getch() {
  return getch();
}

// 改行
void c_newLine() {
  int16_t x,y;
  getyx(y,x);
  if (y>=MCURSES_LINES-1) {
    scroll();
    move(y,0);
  } else {
    move(y+1,0);  
  }  
}

// コンソール初期化
void init_console() {
  // mcursesの設定
  setFunction_putchar(Arduino_putchar);  // 依存関数
  setFunction_getchar(Arduino_getchar);  // 依存関数
  initscr();                             // 依存関数
  setscrreg(0,MCURSES_LINES);  
}

// カーソル表示制御
void c_show_curs(uint8_t mode) {
  flgCurs = mode;
  curs_set(mode);
}

// 画面クリア
void icls() {
  clear();
  move(0,0);
}

// カーソル移動 LOCATE x,y
void ilocate() {
  int16_t x,  y;

  if ( getParam(x, true) ||
      getParam(y, false) 
  ) return;
  
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
const PROGMEM  uint8_t tbl_attr[]  =
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
  attrset(pgm_read_word(&tbl_fcolor[fc])|(pgm_read_word(&tbl_fcolor[bc])<<4));
}

// 文字属性の指定 ATTRコマンド
void iattr() {
  int16_t attr;
  if ( getParam(attr, 0, 4, false) ) return;
  attrset(pgm_read_byte(&tbl_attr[attr]));
}

// 文末空白文字のトリム処理
char* tlimR(char* str) {
  uint16_t len = strlen(str);
  for (uint16_t i = len - 1; i>0 ; i--) {
    if (str[i] != ' ') break;
    str[i] = 0;
  }
  return str;
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
uint8_t isJMS( uint8_t *str, uint16_t nPos ) {
  uint16_t i;
  uint8_t state; // { 0, 1, 2 }

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
  uint8_t len = strlen((char *)str); // 文字列長さ 
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

// カーソルを１文字前に移動
void line_movePrevChar(uint8_t* str, uint8_t ln, uint8_t& pos) { 
  if (pos > 0) {
    pos--;
  }
  if ( (pos-2 >= 0) && (isJMS(lbuf,pos-1) !=2) ) {
    // 全角文字の2バイト目の場合、全角1バイト目にカーソルを移動する
    pos--;
  } 
  move(ln,pos+1);
}

// カーソルを１文字次に移動
void line_moveNextChar(uint8_t* str, uint8_t ln, uint8_t& pos) {
  uint8_t len = strlen((char *)str); // 文字列長さ 
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
  c_puts((char *)lbuf);
  move(ln,pos+1);  
}

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
  clearlbuf();
  while ((c = c_getch()) != KEY_CR) { //改行でなければ繰り返す
    if (c == KEY_TAB) {  // [TAB]キー
      if  (len == 0) {
        if (errorLine > 0) {
          // 空白の場合は、直前のエラー発生行の内容を表示する
          text = (uint8_t*)tlimR((char*)getLineStr(errorLine));
        } else {
          text = (uint8_t*)tlimR((char*)lbuf);          
        }
        if (text) {
          // 指定した行が存在する場合は、その内容を表示する
          //strcpy(lbuf,text);
          pos = 0;
          len = strlen((char*)lbuf);
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
            text = (uint8_t*)tlimR(getLineStr(value));
            if (text) {
              // 指定した行が存在する場合は、その内容を表示する
              //strcpy(lbuf,text);
              pos = 0;
              len = strlen((char*)lbuf);
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
        text = (uint8_t*)tlimR(getLineStr(line_index));
        //strcpy((char *)lbuf, (char *)text);
        pos = 0;
        len = strlen((char *)lbuf);
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
    } else if (c>=32 && (len < (SIZE_LINE - 1))) { 
      //表示可能な文字が入力された場合の処理（バッファのサイズを超えないこと）      
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
