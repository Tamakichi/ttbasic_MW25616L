//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// ファイルシステム機能に関する機能（内部EEPROM、外部接続I2C EEPROM（オプション機能））
// 2019/06/08 by たま吉さん 
// 修正 2019/07/27 LOADコマンドのエラーコード不具合対応

#include "Arduino.h"
#include "basic.h"

// **** I2C外部接続EEPROMライブラリの利用設定(オプション) ***
#if USE_CMD_I2C == 1 && USE_I2CEEPROM == 1
  #include "src/lib/TI2CEEPROM.h"
  TI2CEEPROM rom(0x50);  // I2Cスレーブアドレスは0x50
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

// プログラムロード/セーブ
// LOAD 保存領域番号|"ファイル名"
// SAVE 保存領域番号|"ファイル名"
// 引数
//  mode     0:ロード、0以外:セーブ
//  flgskip  0:引数チェック有効、0以外 引数チェック無効（自動起動ロード時利用）
//
void iLoadSave(uint8_t mode,uint8_t flgskip) {
  int16_t  prgno = 0;  // プログラム番号
  uint16_t topAddr;    // EEPROMアドレス

  // 引数がファイル名かのチェック
#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
  if (*cip == I_STR) {

    // 外部接続I2C EEPROMへロード/セーブ処理
    uint8_t fname[TI2CEEPROM_FNAMESIZ+1];
    uint8_t rc;   
    if (getFname(fname, TI2CEEPROM_FNAMESIZ)) return;  // ファイル名の取得
    if (mode) {
      rc = rom.save(fname, 0, listbuf, SIZE_LIST);     // プログラムのセーブ  
    } else {
      rc = rom.load(fname, 0, listbuf, SIZE_LIST);     // プログラムのロード
    }
    if (rc == 2)
      err = mode? ERR_NOFSPACE :ERR_FNAME;
    else if (rc)
      err = ERR_I2CDEV;
  } else 
 #endif 
  {
    // 内部EEPROMメモリへロード/セーブ処理
    if (!flgskip) {
      if (*cip == I_EOL) {
        prgno = 0; // 引数省略時はプログラム番号を0とする
      } else if ( getParam(prgno, 0, EEPROM_SAVE_NUM-1, false) ) {
        return;    // 引数エラー
      }
    }    
    topAddr = EEPROM_PAGE_SIZE*prgno;
    if (mode)
      eeprom_update_block((void *)listbuf, (void *)topAddr, SIZE_LIST);  // プログラムのセーブ  
    else
      eeprom_read_block((void *)listbuf, (void *)topAddr, SIZE_LIST);    // プログラムのロード      
  }
}

void iefiles();
void iedel();

// EEPROM上のプログラム消去
// ERASE[プログラム番号[,プログラム番号]|"ファイル名"
void ierase() {
  int16_t  s_prgno, e_prgno;
  uint32_t* topAddr;
  
  // ファイル名指定の場合、I2C EPPROMの指定ファイル削除を行う
  if (*cip == I_STR) {
     iedel();
     return;
  } 

  // 引数省略or番号指定の場合、内部EEPROMのプログラム削除を行う
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

#if USE_I2CEEPROM == 1 && USE_CMD_I2C == 1
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
  }
  cip+= len;
  return 0;  
}
#endif

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
  rc = rom.format((uint8_t*)MYSIGN, devname, fnum, SIZE_LIST);
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
  uint8_t  fname[TI2CEEPROM_FNAMESIZ+1];    // 引数ファイル名
  uint8_t  tmpfname[TI2CEEPROM_FNAMESIZ+1]; // ファイル管理テーブル内ファイル名
  uint8_t ftable[16];                       // ファイル管理テーブル  
  int16_t  num;                             // ファイル管理テーブル数
  uint8_t flgcmp = 1;                       // ファイル比較フラグ
  uint8_t rc;
  
  // ファイル名の取得
  if (getFname(fname, TI2CEEPROM_FNAMESIZ)) 
    return;
  if (strlen((char *)fname) == 0) {
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
  c_puts((char*)tmpfname);
  newline(); //改行
  
  // ファイル名表示ループ
  uint8_t cnt=0,total=0;
  for (uint8_t i=0; i < num; i++) {
    //強制的な中断の判定
    if (isBreak())
      return;
      
    if( (rc = rom.getTable(ftable,i)) ) { // 管理テーブルの取得
      err = ERR_I2CDEV;
      return;
    }
    if (!ftable[0])
      continue; // ブランクの場合はスキップ

    memset(tmpfname,0,TI2CEEPROM_FNAMESIZ+1);
    strncpy((char*)tmpfname, (char*)ftable, TI2CEEPROM_FNAMESIZ);
    if ( !flgcmp || wildcard_match((char*)fname, (char*)tmpfname) ) {
      // 条件に一致      
      c_puts((char*)tmpfname);
      if ((cnt % 4) == 3)
        newline();
      else
        for(uint8_t j=0; j < 18-strlen((char*)tmpfname); j++)
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
    uint8_t fname[TI2CEEPROM_FNAMESIZ+1];
    uint8_t rc;
    
    // ファイル名の取得
    if (getFname(fname, TI2CEEPROM_FNAMESIZ)) 
      return;
  
    // プログラムのロード
    if ( (rc = rom.del(fname)) ) {
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
  uint8_t fname[TI2CEEPROM_FNAMESIZ+1];

  // ファイル名指定の場合、I2C EPPROMの一覧表示を行う
  if (*cip == I_STR) {
    if (getFname(fname, TI2CEEPROM_FNAMESIZ))   return;
    if (strlen((char *)fname) !=1) {
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
