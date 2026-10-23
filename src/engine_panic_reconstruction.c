/*
 * Analyst-written reconstruction of the shared panic reporter at 0x411000.
 * This file is documentation/source reconstruction, not wired into the
 * portable CMake target. The fixed-size formatting behavior and null write
 * intentionally preserve the observed legacy path; do not execute this code.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

typedef struct VrPanicBuffer {
    char bytes[255];
    char logging_gate_byte;
} VrPanicBuffer;

extern int DAT_004ecfd8;
extern int DAT_004ed010;
extern char DAT_004d1810;
extern volatile char DAT_004d2978;
extern const char DAT_004d182c[]; /* 70 '=' characters */

extern void enter_log_module(int module_index); /* 0x4151A0 */
extern void dispatch_log_line(const char *line); /* 0x410D90 */
extern void leave_log_module(int module_index); /* 0x4151F0 */
extern void no_op_4111c0(void);                /* observed body is RET */

static void report_panic_v(const char *format, va_list arguments)
{
    VrPanicBuffer report;
    strcpy(report.bytes, "Panic : ");
    report.logging_gate_byte = '\0';

    /* The original uses unbounded _vsprintf into this 255-byte local area. */
    (void)vsprintf(report.bytes + strlen(report.bytes), format, arguments);

    if (report.logging_gate_byte == '\0') {
        if (DAT_004ecfd8 != 0 && DAT_004ed010 != 0) {
            enter_log_module(DAT_004ecfd8);
            dispatch_log_line(DAT_004d182c);
            dispatch_log_line(report.bytes);
            dispatch_log_line(DAT_004d182c);
            leave_log_module(DAT_004ecfd8);
        } else if (DAT_004d1810 == '\0') {
            no_op_4111c0();
            no_op_4111c0();
            no_op_4111c0();
            no_op_4111c0();
        } else {
            (void)MessageBoxA(NULL, report.bytes, "Last-Resort Log", 0x30);
        }
    }

    /* 0x416020 stores the panic flag, then attempts MOV byte ptr [0], AL. */
    DAT_004d2978 = 1;
    *(volatile char *)0 = 0;
}

void vr_panic(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    report_panic_v(format, arguments);
    va_end(arguments);
}
