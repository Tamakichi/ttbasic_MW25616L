//
// Arduino Uno互換機+「アクティブマトリクス蛍光表示管（CL-VFD）MW25616L 実験用表示モジュール」対応
// BASIC 追加基本コマンド・関数
// 2019/06/08 by たま吉さん 
//

#include "Arduino.h"
#include "basic.h"

// 16進文字出力
// HEX$(数値,桁数) or HEX$(数値)
void ihex(uint8_t devno) {
  int16_t value; // 値
  int16_t d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,4,false)) return;  
  }
  if (checkClose()) return;  
  putHexnum(value, d, devno);    
}

// 2進数出力
// BIN$(数値, 桁数) or BIN$(数値)
void ibin(uint8_t devno) {
  int16_t value; // 値
  int16_t d = 0; // 桁数(0で桁数指定なし)

  if (checkOpen()) return;
  if (getParam(value,false)) return;  
  if (*cip == I_COMMA) {
     cip++;
     if (getParam(d,0,16,false)) return;  
  }
  if (checkClose()) return;
  putBinnum(value, d, devno);    
}

// CHR$() 全角対応
void ichr(uint8_t devno) {
  uint16_t value; // 値
  if (checkOpen()) return;
  for(;;) {
    if (getParam(value,false)) return;
    if (value <= 0xff) {
       c_putch(value, devno);
    } else {
       c_putch(value>>8,  devno);
       c_putch(value&0xff,devno);
    }
    if (*cip == I_COMMA) {
       cip++;
       continue;
    }
    break;
  }
  if (checkClose()) return;
}

// MAP(V,L1,H1,L2,H2)関数の処理
int16_t imap() {
  int32_t value,l1,h1,l2,h2,rc;
  if (checkOpen()) return 0;
  if ( getParam(value,true)||getParam(l1,true)||
       getParam(h1,true)||getParam(l2,true)||getParam(h2,false) ) 
    return 0;
  if (checkClose()) return 0;
  if (l1 >= h1 || l2 >= h2 || value < l1 || value > h1) {
    err = ERR_VALUE;
    return 0;
  }
  rc = (value-l1)*(h2-l2)/(h1-l1)+l2;
  return rc;  
}

#if USE_GRADE == 1
// 関数GRADE(値,配列番号,配列データ数)
int16_t igrade() {
  int16_t value,arrayIndex,len,rc=-1;

  if (checkOpen()) return 0;
  if ( getParam(value, true) )  return 0;
  if ( getParam(arrayIndex, 0, SIZE_ARRY-1, true) )  return 0;
  if ( getParam(len, 0, SIZE_ARRY, false) )  return 0;
  if ( checkClose()) return 0;    
 
  if ( arrayIndex+len > SIZE_ARRY ) {
    err = ERR_VALUE;
    return 0;    
  }
  for (int16_t i=0; i < len; i++) {
    if (value >= arr[arrayIndex+i])  {
      rc = i;
      break;
    }
  }
  return rc;
}
#endif  

// ASC(文字列)
// ASC(文字列,文字位置)
// ASC(変数,文字位置)
int16_t iasc() {
  uint16_t value =0;
  int16_t len;     // 文字列長
  int16_t pos =1;  // 文字位置
  int16_t index;   // 配列添え字
  uint8_t* str;    // 文字列先頭位置
  
  if (checkOpen()) return 0;
  if ( *cip == I_STR) {  // 文字列定数の場合
     cip++;  len = *cip; // 文字列長の取得
     cip++;  str = cip;  // 文字列先頭の取得
     cip+=len;
  } else if ( *cip == I_VAR) {   // 変数の場合
     cip++;   str = v2realAddr(var[*cip]);
     len = *str;
     str++;
     cip++;     
  } else if ( *cip == I_ARRAY) { // 配列変数の場合
     cip++; 
     if (getParam(index, 0, SIZE_ARRY-1, false)) return 0;
     str = v2realAddr(arr[index]);
     len = *str;
     str++;
  } else {
    err = ERR_SYNTAX;
    return 0;
  }
  if ( *cip == I_COMMA) {
    cip++;
    if (getParam(pos,1,len,false)) return 0;
  }

  int16_t tmpPos = 0;
  int16_t i;
  for (i = 0; i < len; i++) {      
    if (pos == tmpPos+1)
      break;
    if (isZenkaku(str[i])) {
      i++;  
    }
    tmpPos++;
  }
  if (pos != tmpPos+1) {
    value = 0;
  } else {
    value = str[i];
    if(isZenkaku(str[i])) {
      value<<=8;
      value+= str[i+1];
    }
  }
  checkClose();
  return value;

}

// WLEN(文字列)
// 全角文字列長取得
int16_t iwlen(uint8_t flgZen) {
  int16_t len;     // 文字列長
  int16_t index;   // 配列添え字
  uint8_t* str;    // 文字列先頭位置
  int16_t wlen = 0;
  int16_t pos = 0;
  
  if (checkOpen()) 
    return 0;
    
  if ( *cip == I_VAR)  {
    // 変数の場合
     cip++;
     str = v2realAddr(var[*cip]);
     len = *str; // 文字列長の取得
     str++;      // 文字列先頭
     cip++;     
  } else if ( *cip == I_ARRAY) {
    // 配列変数の場合
     cip++; 
     if (getParam(index, 0, SIZE_ARRY-1, false)) return 0;
     str = v2realAddr(arr[index]);
     len = *str; // 文字列長の取得
     str++;      // 文字列先頭
  } else if ( *cip == I_STR) {
    // 文字列定数の場合
     cip++;  len = *cip; // 文字列長の取得
     cip++;  str = cip;  // 文字列先頭の取得
     cip+=len;
  } else {
    err = ERR_SYNTAX;
  }
  checkClose();
  if (flgZen) {
    // 文字列をスキャンし、長さを求める
    while(pos < len) {
      if (isZenkaku(*str)) {
        str++;
        pos++;  
      }
      wlen++;
      str++;
      pos++;
    }  
  } else {
    wlen = len;
  }
  return wlen;
}

#if USE_DMP == 1
// 小数点数値出力 DMP$(数値) or DMP(数値,小数部桁数) or DMP(数値,小数部桁数,整数部桁指定)
void idmp(uint8_t devno) {
  int32_t value;     // 値
  int32_t v1,v2;
  int16_t n = 2;    // 小数部桁数
  int16_t dn = 0;   // 整数部桁指定
  int32_t base=1;
  
  if (checkOpen()) return;
  if (getParam(value, false)) return;
  if (*cip == I_COMMA) { 
    cip++; 
    if (getParam(n, 0,4,false)) return;
    if (*cip == I_COMMA) { 
       cip++; 
      if (getParam(dn,-6,6,false)) return;
    }  
  }
  if (checkClose()) return;
  
  for (int16_t i=0; i<n;i++) {
    base*=10;
  }
  v1 = value / base;
  v2 = value % base;
  if (v1 == 0 && value <0)
    c_putch('-',devno);  
  putnum(v1, dn, devno);
  if (n) {
    c_putch('.',devno);
    putnum(v2<0 ?-v2:v2, -n, devno);
  }
}
#endif

// 文字列参照 WSTR$(変数)
// WSTR(文字列参照変数|文字列参照配列変数|文字列定数,[pos,n])
// ※変数,配列は　[LEN][文字列]への参照とする
// 引数
//  devno: 出力先デバイス番号
// 戻り値
//  なし
//
void iwstr(uint8_t devno) {
  int16_t len;  // 文字列長
  int16_t top;  // 文字取り出し位置
  int16_t n;    // 取り出し文字数
  int16_t index;
  uint8_t *ptr;  // 文字列先頭
  
  if (checkOpen()) return;
  if (*cip == I_VAR) {
    // 変数
    cip++;
    ptr = v2realAddr(var[*cip]);
    len = *ptr;
    ptr++;
    cip++;
  } else if (*cip == I_ARRAY) {
    // 配列変数
    cip++; 
    if (getParam(index, 0, SIZE_ARRY-1, false)) return;
    ptr = v2realAddr(arr[index]);
    len = *ptr;
    ptr++;    
  } else if (*cip == I_STR) {
    // 文字列定数
    cip++;
    len = *cip;
    cip++;
    ptr = cip;
    cip+=len;    
  } else {
    err = ERR_SYNTAX;
    return;
  }
  top = 1; // 文字取り出し位置
  n = len; // 取り出し文字数
  if (*cip == I_COMMA) {
    // 引数：文字取り出し位置、取り出し文字数の取得 
    cip++;
    if (getParam(top, 1,len,true)) return;
    if (getParam(n,1,len-top+1,false)) return;
  }
  if (checkClose()) return;

  // 全角を考慮した文字位置の取得
  int16_t i;
  int16_t wtop = 1;
  for (i=0; i < len; i++) {
    if (wtop == top) {
      break;
    }
    if (isZenkaku(ptr[i])) {
      i++;  
    }
    wtop++;
  }
  if (wtop == top) {
    //実際の取り出し位置
    top = i+1;
  } else {
    err = ERR_VALUE;
    return;
  }
  
  // 全角を考慮した取り出し文字列の出力
  int16_t cnt=0;
  for (int16_t i = top-1 ; i < len; i++) {
    if (cnt == n) {
      break;
    }  
    c_putch(ptr[i], devno);
    if (isZenkaku(ptr[i])) {
      i++; c_putch(ptr[i], devno);
    }
    cnt++;
  }
  return;
}

// メモリ参照
// PEEK(adr)
int16_t ipeek() {
  int16_t value =0, vadr;
  uint8_t* radr;
  
  vadr = getparam();
  if (err) return 0;
  radr = v2realAddr(vadr);
  if (radr)
    value = *radr;
  else 
    err = ERR_VALUE;
  return value;
}

// POKEコマンド POKE ADR,データ[,データ,..データ]
void ipoke() {
  uint8_t* adr;
  int16_t value;
  int16_t vadr;
  
  // アドレスの指定
  if ( getParam(vadr, 0, 32767, false) ) return;

  // 例: 1,2,3,4,5 の連続設定処理
  do {
    adr = v2realAddr(vadr);
    if (!adr) {
      err = ERR_VALUE;
      break;
    }
    cip++;          // 中間コードポインタを次へ進める
    if (getParam(value,false)) return; 
    *((uint8_t*)adr) = (uint8_t)value;
    vadr++;
  } while(*cip == I_COMMA);
}
