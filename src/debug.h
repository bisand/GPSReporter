#define DBG

#ifdef DBG
#ifndef DBG_PRN
#define DBG_PRN(x) Serial.print(x)
#endif
#ifndef DBG_PRNFL
#define DBG_PRNFL(x,y) Serial.print(x,y)
#endif
#ifndef DBG_PRNDEC
#define DBG_PRNDEC(x) Serial.print(x,DEC)
#endif
#ifndef DBG_PRNLN
#define DBG_PRNLN(x) Serial.println(x)
#endif
#else
#ifndef DBG_PRN
#define DBG_PRN(x)
#endif
#ifndef DBG_PRNDEC
#define DBG_PRNDEC(x)
#endif
#ifndef DBG_PRNFL
#define DBG_PRNFL(x,y)
#endif
#ifndef DBG_PRNLN
#define DBG_PRNLN(x)
#endif
#endif
