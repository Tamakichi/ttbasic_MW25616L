//
// 日本語フォントROM GT20L16J1Y 利用ドライバ
//

#ifndef _FontGT20L16J1Y_H_
#define _FontGT20L16J1Y_H_

#include <Arduino.h>
#define USE_HAN2ZEN 0
class FontGT20L16J1Y {
  private:
    uint8_t bytes;
  public:
    FontGT20L16J1Y() {};                           // コンストラクタ
    ~FontGT20L16J1Y() {};                          // ディストラクタ
    uint8_t begin();                               // フォント利用開始
    uint8_t end();                                 // フォント利用終了    
    uint32_t calcAddr(uint16_t jiscode);           // 全角JISコードからフォント格納先頭アドレスを求める
    uint16_t HantoZen(uint16_t sjis);              // SJIS半角コードからSJIS全角コードに変換
    uint16_t SJIS2JIS(uint16_t sjis);              // SJISからJISx0208変換
//    void convFont(uint8_t* src,uint8_t flgHan=false);  // フォント形式を横並びに変換
    uint8_t prvBytes() {return bytes;};            // 直前のgetFontData(),getFontDataBySJISで処理した文字のバイト長を調べる

    // 第1バイト全角判定
    boolean isZenkaku(uint8_t c) {                    
      return (((c>=0x81)&&(c<=0x9f))||((c>=0xe0)&&(c<=0xfc))) ? 1:0;
    };
    
    // フォントデータの取得
    boolean getFontDataBySJIS(uint8_t* fontdata, uint16_t sjis);

    // SJIS文字列に対応する先頭文字のフォントデータ取得
    char* getFontData(uint8_t* fontdata,char *pSJIS,bool h2z=false);  
};

#endif
