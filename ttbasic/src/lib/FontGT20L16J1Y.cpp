//
// 日本語フォントROM GT20L16J1Y 利用ドライバ
// 最終更新日2019/05/28 by たま吉さん
//

#include "FontGT20L16J1Y.h"
#include <avr/pgmspace.h>    //PROGMEM定義ライブラリ
#include <SPI.h>
#define ASC8x16S 255968  // 8x16 ASCII 粗体字符(半角)
#define ASC8x16N 257504  // 8x16 ASCII 日文假名(半角)

#if USE_HAN2ZEN == 1
// SJISコード 半角全角変換用テーブル
PROGMEM const uint16_t zentable[] PROGMEM = {
  0x8140,0x8149,0x8168,0x8194,0x8190,0x8193,0x8195,0x8166,
  0x8169,0x816A,0x8196,0x817B,0x2124,0x817C,0x8144,0x815E,
  0x824F,0x8250,0x8251,0x8252,0x8253,0x8254,0x8255,0x8256,
  0x8257,0x8258,0x8146,0x8147,0x8183,0x8181,0x8184,0x8148,
  0x8197,0x8260,0x8261,0x8262,0x8263,0x8264,0x8265,0x8266,
  0x8267,0x8268,0x8269,0x826A,0x826B,0x826C,0x826D,0x826E,
  0x826F,0x8270,0x8271,0x8272,0x8273,0x8274,0x8275,0x8276,
  0x8277,0x8278,0x8279,0x816D,0x818F,0x816E,0x814F,0x8151,
  0x8165,0x8281,0x8282,0x8283,0x8284,0x8285,0x8286,0x8287,
  0x8288,0x8289,0x828A,0x828B,0x828C,0x828D,0x828E,0x828F,
  0x8290,0x8291,0x8292,0x8293,0x8294,0x8295,0x8296,0x8297,
  0x8298,0x8299,0x829A,0x816F,0x8162,0x8170,0x8160,0x8142,
  0x8175,0x8176,0x8141,0x8145,0x8392,0x8340,0x8342,0x8344,
  0x8346,0x8348,0x8383,0x8385,0x8387,0x8362,0x815B,0x8341,
  0x8343,0x8345,0x8347,0x8349,0x834A,0x834C,0x834E,0x8350,
  0x8352,0x8354,0x8356,0x8358,0x835A,0x835C,0x835E,0x8360,
  0x8363,0x8365,0x8367,0x8369,0x836A,0x836B,0x836C,0x836D,
  0x836E,0x8371,0x8374,0x8377,0x837A,0x837D,0x837E,0x8380,
  0x8381,0x8382,0x8384,0x8386,0x8388,0x8389,0x838A,0x838B,
  0x838C,0x838D,0x838F,0x8393,0x814A,0x814B,
};
#endif

// 全角JISコードからフォント格納先頭アドレスを求める
//   GT20L16J1Y データシート VER 4.0 2013-3のサンプルを参照した
//   (サンプルでは区点コードからの変換であることに注意)
uint32_t FontGT20L16J1Y::calcAddr(uint16_t jiscode) {
  uint32_t MSB;   
  uint32_t LSB;
  uint32_t Address = 0;

  // 上位、下位を区点コードに変換
  MSB = (jiscode >> 8) - 0x20;
  LSB = (jiscode & 0xff) -0x20;
  
  // データ格納アドレスを求める
  if(MSB >=1 && MSB <= 15 && LSB >=1 && LSB <= 94)
    Address =( (MSB - 1) * 94 + (LSB - 01))*32;
  else if(MSB >=16 && MSB <= 47 && LSB >=1 && LSB <= 94)
    Address =( (MSB - 16) * 94 + (LSB - 1))*32+43584;
  else if(MSB >=48 && MSB <=84 && LSB >=1 && LSB <= 94)
    Address = ((MSB - 48) * 94 + (LSB - 1))*32+ 138464;
  else if(MSB ==85 && LSB >=0x01 && LSB <= 94)
    Address = ((MSB - 85) * 94 + (LSB - 1))*32+ 246944;
  else if(MSB >=88 && MSB <=89 && LSB >=1 && LSB <= 94)
    Address = ((MSB - 88) * 94 + (LSB - 1))*32+ 249952;
  
  return Address;
}

//
// SJIS半角コードをSJIS全角コードに変換する
// (変換できない場合は元のコードを返す)
//   sjis(in): SJIS文字コード
//   戻り値: 変換コード
//
uint16_t FontGT20L16J1Y::HantoZen(uint16_t sjis) {
#if USE_HAN2ZEN == 1
  uint16_t code;
  if (sjis < 0x20 || sjis > 0xdf) {
     code = sjis;
  } else if (sjis < 0xa1) {
     code = pgm_read_word(&zentable[sjis-0x20]);
  } else {
     code = pgm_read_word(&zentable[sjis+95-0xa1]);
  }
  return code;
#else
  return sjis;
#endif
}

// SJISからJISx0208変換
//  コード変換は下記のソースを参照して作成しました
//  http://trac.switch-science.com/wiki/KanjiROM
//
uint16_t FontGT20L16J1Y::SJIS2JIS(uint16_t sjis) {
  uint16_t c1 = ((sjis & 0xff00) >> 8);
  uint16_t c2 = (sjis & 0xFF);
  if (c1 >= 0xe0) {
    c1 = c1 - 0x40;
  }
  if (c2 >= 0x80) {
    c2 = c2 - 1;
  }
  if (c2 >= 0x9e) {
    c1 = (c1 - 0x70) * 2;
    c2 = c2 - 0x7d;
  } else {
    c1 = ((c1 - 0x70) * 2) - 1;
    c2 = c2 - 0x1f;
  }
  return (c1<<8)|c2;
}

// フォント利用開始
void FontGT20L16J1Y::begin() {
  //SPI通信の設定
  pinMode (SS, OUTPUT);
  SPI.begin();
  //SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(3);
  SPI.setClockDivider(SPI_CLOCK_DIV16);                                    
}

// フォント利用終了 
void FontGT20L16J1Y::end() {
  SPI.end();
}

// フォントデータの取得
//  fontdata: フォントデータ格納先アドレス
//  sjis    : フォントコード
boolean FontGT20L16J1Y::getFontDataBySJIS(uint8_t* fontdata, uint16_t sjis) {
  //uint8_t data;
  int32_t addr=0;
  uint8_t n;
  uint16_t code;

  if (sjis > 0xFF) {
    // 全角
    code = SJIS2JIS(sjis); 
    addr =calcAddr(code);
    n = 32;
    bytes = 2;
  } else {
    // 半角
    addr = ASC8x16S + (sjis-0x20)*16;
    n = 16;
    bytes = 1;
  }

  digitalWrite(SS, HIGH);
  delayMicroseconds(10);
  digitalWrite(SS, LOW);
  SPI.transfer(0x03);
  SPI.transfer((addr>>16) & 0xff);
  SPI.transfer((addr>>8) & 0xff);
  SPI.transfer(addr & 0xff);
  //SPI.setBitOrder(LSBFIRST);
  
  for(byte i = 0;i< n; i++)  {
    fontdata[i] = SPI.transfer(0x00);
  }
  digitalWrite(SS, HIGH);
  return true;
}

// 指定したSJIS文字列の先頭のフォントデータの取得
//   data(out): フォントデータ格納アドレス
//   pSJIS(in): SJIS文字列
//   h2z(in)  : true :半角を全角に変換する false: 変換しない 
//   戻り値   : 次の文字列位置、取得失敗の場合NULLを返す
//
char*  FontGT20L16J1Y::getFontData(byte* fontdata,char *pSJIS,bool h2z) {
  uint16_t sjis;

  // 文字列終端チェック
  if (pSJIS == NULL || *pSJIS == 0) {
    return NULL;
  }
 
  // 文字列から1文字取り出し
  sjis = *pSJIS;  
  if (isZenkaku(*pSJIS)) {
    sjis<<=8;
    pSJIS++;
    if (*pSJIS == 0) {
      return NULL;
    }
    sjis += *pSJIS;
  }  
  pSJIS++;
#if USE_HAN2ZEN == 1
  // 半角を全角に変換する処理
  if (h2z) {
    sjis = HantoZen(sjis);
  }
#endif
  // フォントデータの取得
  if (false == getFontDataBySJIS(fontdata, sjis)) {
    return NULL;
  }
  return (pSJIS);  
}
/*
// フォント形式を横並びに変換
void FontGT20L16J1Y::convFont(uint8_t* src, uint8_t flgHan) {
  uint8_t dst[32];
  if (flgHan) {
    for (uint8_t x=0; x < 8; x++) {
      for (uint8_t y=0; y < 8; y++) {
        if (src[x] & 0x80>>y) {
         dst[y + (x>>3)] |= 0x80>>(x&7);
        } else {
         dst[y + (x>>3)] &= ~0x80>>(x&7);
        }
        if (src[x+8] & 0x80>>y) {
         dst[y+8 + (x>>3)] |= 0x80>>(x&7);
        } else {
         dst[y+8 + (x>>3)] &= ~0x80>>(x&7);
        }
      }  
    }
    memcpy(src,dst,16);    
   } else {
    for (uint8_t x=0; x < 16; x++) {
      for (uint8_t y=0; y < 8; y++) {
        if (src[x] & 0x80>>y) {
         dst[y*2 + (x>>3)] |= 0x80>>(x&7);
        } else {
         dst[y*2 + (x>>3)] &= ~0x80>>(x&7);
        }
        if (src[x+16] & 0x80>>y) {
         dst[(y+8)*2 + (x>>3)] |= 0x80>>(x&7);
        } else {
         dst[(y+8)*2 + (x>>3)] &= ~0x80>>(x&7);
        }
      }  
    }
    memcpy(src,dst,32);
  } 
}
*/


