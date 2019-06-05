#define DEBUG

#ifdef DEBUG
#ifndef DEBUG_PRINT
#define DEBUG_PRINT(x) Serial.print(x)
#endif
#ifndef DEBUG_PRINTFLOAT
#define DEBUG_PRINTFLOAT(x,y) Serial.print(x,y)
#endif
#ifndef DEBUG_PRINTDEC
#define DEBUG_PRINTDEC(x) Serial.print(x,DEC)
#endif
#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(x) Serial.println(x)
#endif
#else
#ifndef DEBUG_PRINT
#define DEBUG_PRINT(x)
#endif
#ifndef DEBUG_PRINTDEC
#define DEBUG_PRINTDEC(x)
#endif
#ifndef DEBUG_PRINTFLOAT
#define DEBUG_PRINTFLOAT(x,y)
#endif
#ifndef DEBUG_PRINTLN
#define DEBUG_PRINTLN(x)
#endif
#endif
