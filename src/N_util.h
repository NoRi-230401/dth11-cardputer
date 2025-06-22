// *******************************************************
//  N_UTIL          by NoRi 2025-04-15
// -------------------------------------------------------
// N_util.h
// *******************************************************
#ifndef _N_UTIL_H
#define _N_UTIL_H
// -------------------------------------------------------
#include <Arduino.h>
#include <SD.h>
#include <M5Cardputer.h>

extern bool SD_ENABLE;
extern const int32_t N_COLS, N_ROWS;
extern int32_t W_CHR, H_CHR;        // Character dimensions
extern int32_t X_WIDTH, Y_HEIGHT;   // Screen dimensions
extern int32_t SC_LINES[];          // Array to store Y coordinates of each line
#define BRIGHT_NORMAL 40            // LCD normal bright level (0-255)

extern void m5stack_begin();
extern void SDU_lobby();
extern bool SD_begin();
extern void dbPrtln(String msg);
extern void dbPrt(String msg);
extern void dispLx(uint8_t Lx, const char *msg);
extern void POWER_OFF();

// -------------------------------------------------------
#endif // _N_UTIL_H
