//
// TI2CEPPROM I2C接続EEPROMクラス 簡易ファイルシステム
// 2018/02/25 by たま吉さん
//

#include "TI2CEPPROM.h"
#include <Wire.h>

#define BLKSIE        16 // 一回ごとのアクセスバイト数(I2Cのバッファ32バイト制限考慮)
#define HEADSIZE      16 // ヘッダーサイズ
#define FILEINFOSIZE  16 // ファイル管理情報サイズ
#define POS_SIGN      0  // シグニチャ名位置
#define POS_VOLUME    4  // ボリューム名位置
#define POS_RCDNUM    14 // レコード数位置
#define POS_FTYPE     14 // ファイルタイプ位置
#define POS_BSIZE     15 // ブロックサイズ位置

#define SZ_SIGN      4   // シグニチャ名バイト数
#define SZ_VOLUME    10  // ボリューム名バイト数
#define SZ_RCDNUM    1   // レコード数バイト数
#define SZ_FTYPE     1   // ファイルタイプバイト数
#define SZ_BSIZE     1   // ブロックサイズバイト数
#define SZ_FNAME     14  // ファイル名バイト数

// 16バイト迄の読み込み
uint8_t TI2CEPPROM::read16(uint16_t addr, uint8_t* buf,uint16_t len) {
  Wire.beginTransmission(_devaddr);
  Wire.write(addr >> 8);     // 上位アドレス
  Wire.write(addr & 0xff);   // 下位アドレス
  Wire.endTransmission(false);
  Wire.requestFrom(_devaddr, (int)len);
  for (uint16_t i = 0; i < len ; i++ ,buf++) {
    *buf = Wire.read();         
  }
  return  Wire.endTransmission();
}

////////////////////////////////////////////////////
// 指定アドレスのデータ読込
// 引数
//  addr : EEPROM データ読込先頭アドレス(0 ～ 0xFFFF)
//  buf  : 読込データ格納アドレス
//  len  : 読込データ長さ
// 戻り値
//  0: 正常終了、1～4:I2Cデバイスエラー 
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::read(uint16_t addr, uint8_t* buf,uint16_t len) {
  uint8_t  rc;
  uint16_t cnt = len / BLKSIE; 
  uint16_t rst = len % BLKSIE;
  uint16_t pos = 0;

  // Wireのバッファサイズ対策のため、16バイト毎に読込を行う
  for (uint8_t i=0; i < cnt; i++, addr += BLKSIE,buf+=BLKSIE) {    
    // 16バイト読込トランザクション
    if ( rc = this->read16(addr, buf, 16) )
      return rc;
  }

  // 16バイト未満の読み込み
  if (rst) {
    if ( rc = this->read16(addr, buf, rst) )
      return rc;
  }
  return 0;
}

// 16バイト迄の書込み
uint8_t TI2CEPPROM::write16(uint16_t addr, uint8_t* buf, uint16_t len) {
  uint8_t  rc;
  Wire.beginTransmission(_devaddr);
  Wire.write(addr >> 8);     // 上位アドレス
  Wire.write(addr & 0xff);   // 下位アドレス
  Wire.write(buf,len); 
  rc = Wire.endTransmission();
  delay(5);
  return rc;
}

////////////////////////////////////////////////////
// 指定アドレスへのデータ書込み
// 引数
//  addr : EEPROM データ書込み先頭アドレス(0 ～ 0xFFFF)
//  buf  : 書込みデータ格納アドレス
//  len  : 書込みデータ長さ
// 戻り値
//  0: 正常終了、1～4:I2Cデバイスエラー 

////////////////////////////////////////////////////
uint8_t TI2CEPPROM::write(uint16_t addr, uint8_t* buf, uint16_t len) {
  uint8_t  rc;
  uint16_t cnt = len / BLKSIE;
  uint16_t rst = len % BLKSIE;
  uint16_t pos = 0;

  // Wireのバッファサイズ対策のため、16バイト毎に読込を行う
  for (uint16_t i=0; i < cnt; i++,buf+=BLKSIE,addr+=BLKSIE) {
    // 16バイト書込みトランザクション
    if ( rc = this->write16(addr,buf, 16) ) 
      return rc;   
  }

  // 16バイト未満書込みトランザクション
  if (rst) {
    if ( rc = this->write16(addr,buf, rst) ) 
      return rc;
  }
  return 0;  
}

////////////////////////////////////////////////////
// コンストラクタ
// 引数
//  addr: I2Cスレーブアドレス
////////////////////////////////////////////////////
TI2CEPPROM::TI2CEPPROM(uint8_t addr) {
  //_sz = sz;         // EEPROM容量(単位 kバイト)
  _devaddr = addr;    // I2Cスレーブアドレス  
}

/////////////////////////////////////////////////////
// EEPROMのフォーマット
// 引数
//  sign     : シグニチャ(4バイト文字列)
//  devName  : デバイス名(10バイト文字列)
//  numtable : テーブル数（ファイル保存可能数)
//  fsize    : 1ファイルの最大ファイルサイズ
// 戻り値
//  0: 正常終了, 1～4:I2Cデバイスエラー 
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::format(uint8_t* sign, uint8_t* devName, uint8_t numtable, uint16_t fsize) {
 uint8_t rc;
 uint16_t blksz = fsize / 256;                       // 最大ファイルサイズ
 
 // ヘッダーの書込み
 if ( rc = this->write(POS_SIGN, sign, SZ_SIGN) )                    // シグニチャの書込み
   return rc;
 if ( rc = this->write(POS_VOLUME, devName, SZ_VOLUME) )             // デバイス名の書込み
   return rc;
 if ( rc = this->write(POS_RCDNUM, (uint8_t*)&numtable, SZ_RCDNUM) ) // テーブル数の書込み
   return rc;
 if ( rc = this->write(POS_BSIZE, (uint8_t*)&blksz, SZ_BSIZE) )      // ブロックサイズの書込み
   return rc;
 
  // テーブルの初期化
  uint8_t table[FILEINFOSIZE];
  memset(table, 0, FILEINFOSIZE);
  uint16_t addr = FILEINFOSIZE;
  for (uint16_t i = 0; i < numtable; i++) {
    if ( rc = this->write(addr, table, FILEINFOSIZE) )  // テーブルの書込み
      return rc;
    addr+= FILEINFOSIZE;
  }
  return 0;
}

////////////////////////////////////////////////////
// シグニチャのチェック
// 引数
//   sign:シグニチャ
// 戻り値
//   0: 正常
//   1: I2Cデバイスエラー
//   2: シグニチャ不一致
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::checkSign(uint8_t* sign) {  
  uint8_t rdSign[SZ_SIGN+1];
  memset(rdSign, 0, SZ_SIGN+1);

  // シグニチャの読み込み
  if (this->read(POS_SIGN, rdSign, SZ_SIGN)) {
    return 1;
  }

 // シグニチャの比較
 return strncmp(sign,rdSign, SZ_SIGN) ? 2:0;
}

////////////////////////////////////////////////////
// ファイル保存サイズの取得
// (事前にシグニチャチェックを行っていること)
// 戻り値
//  -1  : デバイスエラー
//  0～ : テーブル数
////////////////////////////////////////////////////
int16_t TI2CEPPROM::maxFiles() {
  uint8_t rc;
  // テーブル数の読み込み
  if (this->read(POS_RCDNUM, &rc, SZ_RCDNUM)) {
    return -1;
  }
  return (int16_t)rc;     
}

////////////////////////////////////////////////////
// 最大ファイルサイズ(ページサイズ)の取得
// (事前にシグニチャチェックを行っていること)
// 戻り値
//  -1  : デバイスエラー
//  0～ : ファイルサイズ
////////////////////////////////////////////////////
int16_t TI2CEPPROM::pageSize() {
  uint8_t rc;
  // 最大ファイルサイズ(ページサイズ)の読み込み
  if (this->read(POS_BSIZE, &rc, SZ_BSIZE)) {
    return -1;
  }
  return ((int16_t)rc)*256;     
}

////////////////////////////////////////////////////
// TI2CEPPROM::保存ファイル数の取得
// (事前にシグニチャチェックを行っていること)
// 戻り値
//  -1  : デバイスエラー
//  0～ : ファイル数
////////////////////////////////////////////////////
int16_t TI2CEPPROM::countFiles() {
  int16_t  num;
  uint8_t  flg;
  
  // テーブル数の取得
  if ( (num = this->maxFiles()) < 0 )
    return -1;   
  
  // 保存ファイル数のカウント
  int16_t cnt = 0;
  for (uint16_t i=0; i < num; i++) {
    // ファイル状態のの読み込み
    if (this->read(HEADSIZE+FILEINFOSIZE*i, &flg, 1)) {
      return -1;
    }
    if (flg)
      cnt++;
  }
  return cnt;
}

//////////////////////////////////////////////////
// デバイス名の取得
// (事前にシグニチャチェックを行っていること)
// 引数
//  devname : 取得したデバイス名の格納アドレス
// 戻り値
//  0:正常終了 ,1～4:I2Cデバイス異常
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::getDevName(uint8_t* devname) {
  uint8_t rc;
  memset(devname,0,SZ_VOLUME+1);
  rc =this->read(POS_VOLUME, devname, SZ_VOLUME);
  return  rc;
}

//////////////////////////////////////////////////
// ファイル名の検索し、インデックスを返す
// (事前にシグニチャチェックを行っていること)
// 引数
//  fname:ファイル名(14文字まで)
// 戻り値
//  -1  : デバイスエラー
//  -2  : 該当なし
////////////////////////////////////////////////////
int16_t TI2CEPPROM::find(uint8_t* fname) {
  int16_t  num, rc = -2;
  uint8_t  table[FILEINFOSIZE];
  
  // テーブル数の取得
  if ((num = this->maxFiles()) < 0)
    return -1;
  
  // テーブルの逐次比較
  for (uint16_t i=0; i < num; i++) {
    // ファイル状態のの読み込み
    if (this->read(HEADSIZE+FILEINFOSIZE*i, table, FILEINFOSIZE)) {
      return -1;
    }
    if (strncmp(fname, table, SZ_FNAME) == 0) {
       rc = i;
       break;
    }
  }
  return rc;                  
}

////////////////////////////////////////////////////
// 空きテーブルのインデックスを返す
// 戻り値
//  -1  : デバイスエラー
//  -2  : 空きなし
////////////////////////////////////////////////////
int16_t TI2CEPPROM::findEmpty() {
  int16_t  num, rc = -2;
  uint8_t  table_top;
  
  
  // テーブル数の取得
  if ((num = this->maxFiles()) < 0 )
    return -1;
  
  // テーブルの逐次比較
  for (uint16_t i=0; i < num; i++) {
    // ファイル状態のの読み込み
    if (this->read(HEADSIZE + FILEINFOSIZE*i, &table_top, 1)) {
      return -1;
    }
    if (table_top == 0) {
       rc = i;
       break;
    }
  }
  return rc;
}

////////////////////////////////////////////////////
// データのロード
// 引数
//  fname : ファイル名
//  pos   : ファイル内データ読込位置
//  ptr   : データ格納アドレス
//  len   : データ長
// 戻り値
//   0: 正常
//   1: I2Cデバイスエラー
//   2: 該当ファイルなし
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::load(uint8_t* fname, uint8_t* pos, uint8_t*ptr, uint16_t len) {
  int32_t index;
  int16_t  num, pageSize;  

  // テーブル数の取得
  if ((num = this->maxFiles()) < 0 )
    return 1;

  // ページサイズの取得
  if ( (pageSize = this->pageSize()) < 0 )
    return 1;

  if ((index = this->find(fname)) < 0) {
    if (index == -1) {
      return 1; // I2Cデバイスエラー
    } else {
      return 2; // 該当ファイルなし
    }
  }
  
  if (this->read(HEADSIZE + FILEINFOSIZE * num + pageSize * index + pos, ptr, len)) {
     return 1;
  }
  return 0;
}

////////////////////////////////////////////////
// データの保存
// 引数
//  fname : ファイル名
//  pos   : ファイル内データ書込み位置
//  ptr   : データ格納アドレス
//  len   : データ長
//  ftype : ファイルタイプ(0:ブランク, 1:プログラム(デフォルト), 2:データ)
// 戻り値
//   0: 正常
//   1: I2Cデバイスエラー
//   2: 保存領域無し
////////////////////////////////////////////////
uint8_t TI2CEPPROM::save(uint8_t* fname, uint8_t* pos, uint8_t*ptr, uint16_t len, uint8_t ftyple) {
  int32_t  index;
  int16_t  num, pageSize;

  // テーブル数の取得
  if ( (num = this->maxFiles()) < 0 )
    return 1;

  // ページサイズの取得
  if ( (pageSize = this->pageSize()) < 0 )
    return 1;

  // 既存ファイルのチェック
  if ((index = this->find(fname)) < 0) {
    if (index == -1) {
      return 1; // I2Cデバイスエラー
    } else {
      // 該当ファイルなしの場合、秋テーブルを検索する
      if ((index = this->findEmpty()) < 0) {
        if (index == -1) {
           return 1; // I2Cデバイスエラー
        } else if (index == -2) {
           return 2; // 保存領域なし
        }
      }
    }
  }

  // データの保存
  if (this->write(HEADSIZE + FILEINFOSIZE * num + pageSize * index + pos, ptr, len)) {
     return 1;
  }

  // テーブルの更新
  uint8_t tmp[FILEINFOSIZE];
  memset(tmp,0,FILEINFOSIZE);
  strcpy(tmp,fname);        // ファイル名
  tmp[POS_FTYPE] = ftyple;  // ファイル種別
  if (this->write(HEADSIZE + FILEINFOSIZE * index, tmp, 16)) {
     return 1;
  }
  return 0;
}

////////////////////////////////////////////////////
// ファイルの削除
// 引数
//  fname : ファイル名
// 戻り値
//   0: 正常
//   1: I2Cデバイスエラー
//   2: 該当ファイルなし
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::del(uint8_t* fname) {
  int32_t index;
  int16_t  num;  
  uint8_t  table[FILEINFOSIZE];
  
  // テーブル数の取得
  if ((num = this->maxFiles()) < 0 )
    return 1;
    
  if ((index = this->find(fname)) < 0) {
    if (index == -1) {
      return 1; // I2Cデバイスエラー
    } else {
      return 2; // 該当ファイルなし
    }
  }

  // 該当ファイルの削除
  memset(table,0,FILEINFOSIZE);
  if (this->write(HEADSIZE+FILEINFOSIZE*index, table, FILEINFOSIZE)) {
     return 1;
  }
  return 0;  
}

////////////////////////////////////////////////////
// 指定管理テーブルファイル名の取得
// 戻り値
//   0: 正常
//   1: I2Cデバイスエラー
//   2: 範囲指定外
////////////////////////////////////////////////////
uint8_t TI2CEPPROM::getTable(uint8_t* table, uint8_t index) {
  int16_t  num;
  uint16_t adr = HEADSIZE+FILEINFOSIZE*index;

    // テーブル数の取得
  if ((num = this->maxFiles()) < 0 )
    return 1;

  if (index >= num) {
    return 2;
  }
  
  if (this->read(adr, table, FILEINFOSIZE)) {
    return 1;
  }
  return 0;
}

