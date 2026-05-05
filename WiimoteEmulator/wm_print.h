#ifndef WM_PRINT_H
#define WM_PRINT_H

#include <stdint.h>

extern int show_reports;
extern int reports_truncated;

void print_report(const uint8_t * buf, int len);

#endif
