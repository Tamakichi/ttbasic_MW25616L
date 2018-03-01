//
// TI2CEPPROM I2C接続EEPROMクラス 簡易ファイルシステム
// 2018/02/25 by たま吉さん
//

#ifndef __TI2CEPPROM_H__
#define __TI2CEPPROM_H__

/*
このライブラリは、I2C接続EEPROMに簡易ファイルシステムを構築するためのものです。
用途としては、Tiny BASICでのプログラム、データの保存を想定しています。

[仕様]
・対応EEPROM：24系で64kバイトまでの容量
・簡易的なファイル管理テーブルにてファイル名を指定したファイルの保存と読み込みが可能
・

・EEPROMのデータフォーマット(括弧内はバイトサイズ)
   0x0000 - 0x000F: ヘッダー部(16) :  シグニチャ(4)+ボリューム名(10)+レコード数(1)+ブロックサイズ(1)
     シグニチャ    : フォーマット確認用4文字ID
     ボリューム名  : 複数のEEPROMの識別用 10文字の文字列
     レコード数    : 保存出来るファイル数 0 ～ 255迄 
     ブロックサイズ: 1ファイル(1ページ) 当たりのサイズ、256倍してサイズ(バイト数)                                

   0x0010 - 0x0010+16xレコード数-1: ファイル管理テーブル(16xレコード数)
     レコード(16) :ファイル名(14)+ 種別(1) + 予備(1)
        ファイル名 ：0～14文字のファイル名(重複名は禁止,先頭1バイトが0の場合はブランクと判定)
        種別      ：ファイルタイプ(0:ブランク, 1:プログラム(デフォルト), 2:データ)
        予備      ：未使用
    
     レコードは0～レコード数-1のレコード番号で管理する。
     レコード番号は、ファイルデータ本体の位置特定にも利用する

 0x0010+16xレコード数 - : ファイルデータ本体(ブロックサイズx256)xレコード数
     レコード番号は、ファイルデータ本体の位置特定にも利用する     

対応EEPROM容量の想定
 レコードサイズ512の場合
   4k: 4096バイト  (ヘッダ(16),ファイル管理(16)x7,  ファイル数 7)
   8k: 8192バイト  (ヘッダ(16),ファイル管理(16)x15, ファイル数 15)
  16k:16384バイト  (ヘッダ(16),ファイル管理(16)x31, ファイル数 31)
  32k:32768バイト  (ヘッダ(16),ファイル管理(16)x62, ファイル数 62)
  64k:65536バイト  (ヘッダ(16),ファイル管理(16)x124, ファイル数124)

 レコードサイズ1024の場合
   4k: 4096バイト  (ヘッダ(16),ファイル管理(16)x3,  ファイル数 3)
   8k: 8192バイト  (ヘッダ(16),ファイル管理(16)x7,  ファイル数 7)
  16k:16384バイト  (ヘッダ(16),ファイル管理(16)x15, ファイル数 15)
  32k:32768バイト  (ヘッダ(16),ファイル管理(16)x31, ファイル数 31)
  64k:65536バイト  (ヘッダ(16),ファイル管理(16)x63, ファイル数 63)

 レコードサイズ4096の場合
   4k: 4096バイト  利用不可
   8k: 8192バイト  (ヘッダ(16),ファイル管理(16)x1,  ファイル数 1)
  16k:16384バイト  (ヘッダ(16),ファイル管理(16)x3,  ファイル数 3)
  32k:32768バイト  (ヘッダ(16),ファイル管理(16)x7,  ファイル数 7)
  64k:65536バイト  (ヘッダ(16),ファイル管理(16)x15, ファイル数 15)
 
*/

#include <Arduino.h>

#define TI2CEPPROM_FSIZE    512  // デフォルトの１ファイルのサイズ(バイト)
#define TI2CEPPROM_FNAMESIZ 14   // ファイル名長さ

#define TI2CEPPROM_F_NONE    0   // ファイル種別:ブランク
#define TI2CEPROM_F_PRG      1   // ファイル種別:プログラム
#define TI2CEEPROM_F_PRG     2   // ファイル種別:データ

class TI2CEPPROM {
 private:
   uint8_t _devaddr;  // I2Cスレーブアドレス

 public:
   TI2CEPPROM(uint8_t addr);                                              // コンストラクタ
   void setSlaveAddr(uint8_t addr) { _devaddr = addr;};                   // I2Cスレーブアドレスの設定
   uint8_t checkSign(uint8_t* sign);                                      // シグニチャのチェック
   int16_t pageSize();                                                    // ページサイズ(最大ファイル保存サイズ)の取得
   int16_t maxFiles();                                                    // 最大ファイル数の取得
   int16_t countFiles() ;                                                 // 保存ファイル数の取得
   uint8_t getDevName(uint8_t* devname);                                  // デバイス名の取得
   uint8_t format(uint8_t* sign, uint8_t* devname,uint8_t numtable, uint16_t fsize=TI2CEPPROM_FSIZE); // EEPROMのフォーマット
   uint8_t load(uint8_t* fname, uint8_t* pos, uint8_t*ptr, uint16_t len); // データのロード
   uint8_t save(uint8_t* fname, uint8_t* pos, uint8_t*ptr, uint16_t len, uint8_t ftyple=TI2CEPROM_F_PRG); // データの保存
   uint8_t del(uint8_t* fname);                                           // ファイルの削除
   int16_t find(uint8_t* fname);                                          // ファイルを検索し、インデックスを返す
   int16_t findEmpty();                                                   // 空きテーブルのインデックスを返す
   uint8_t getTable(uint8_t* table, uint8_t index);                       // 指定管理テーブルの取得

   uint8_t read(uint16_t addr, uint8_t* buf,uint16_t len);                // 指定アドレスのデータ読込
   uint8_t write(uint16_t addr, uint8_t* buf, uint16_t len);              // 指定アドレスへのデータ書込み
   uint8_t read16(uint16_t addr, uint8_t* buf,uint16_t len);
   uint8_t write16(uint16_t addr, uint8_t* buf, uint16_t len);
};

#endif
