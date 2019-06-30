//
// NeoPixel制御ライブラリ（256色バージョン）
//

#include "Arduino.h"

class NeoPixel {
 private:
  uint8_t* buf;          // NeoPixel ピクセルバッファ
  uint8_t  n;            // ピクセル数
  uint8_t  brightness;   // 輝度

 public:
  void setBuffer(uint8_t* ptr,uint8_t sz);
  void init();
  void update();
  void cls(uint8_t flgUpdate=false);
  void setRGB(uint8_t no, uint16_t color, uint8_t flgUpdate=false) ;
  void setPixel(uint8_t x, uint8_t y,uint8_t color, uint8_t flgUpdatee=false);
  void shiftPixel(uint8_t dir, uint8_t flgUpdate=false);
  uint8_t XYtoNo(uint8_t x, uint8_t y) { return y&1 ? 8*y + x : 8*y + 7 -x;};
  void setBrightness(uint8_t bt) { brightness = bt; } ;
  void scroll(uint8_t dir=0, uint8_t flgUpdate=false);
  void scrollInChar(uint8_t *fnt, uint16_t color, uint16_t tm);
  uint8_t color8(uint8_t R, uint8_t G, uint8_t B) {return (G<<5) | ((R&0b00011100)<<2) | (B&0b00000011);};
};
