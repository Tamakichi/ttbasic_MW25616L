// 
// 美咲フォントドライバ v1.1 by たま吉さん 2019/06/11
// 内部フラッシュメモリバージョン SJIS 500文字版
//
// 修正 2019/06/30 FTABLESIZEを実配列から算出に変更
//

#ifndef misakiSJIS500_h
#define misakiSJIS500_h

#include <avr/pgmspace.h>
#include <Arduino.h>

#define FTABLESIZE     (sizeof ftable / sizeof ftable[0])       // フォントテーブルデータサイズ
#define FONT_LEN       7         // 1フォントのバイト数

int16_t findcode(uint16_t  sjiscode) ;						    // フォントコード検索
boolean getFontDataBySJIS(byte* fontdata, uint16_t sjis) ;      // SJISに対応する美咲フォントデータ8バイトを取得
uint16_t HantoZen(uint16_t sjis); 				      	        // SJIS半角コードをSJIS全角コードに変換
char* getFontData(byte* fontdata,char *pSJIS,bool h2z=true);    // フォントデータ取得
const uint8_t*  getFontTableAddress();						    // フォントデータテーブル先頭アドレス取得

#endif
