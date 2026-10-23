/*
 * SEH exception-dispatch reconstruction from race.bin disassembly.
 * This is an analyst-written table, not original source code.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct VrExceptionMapEntry {
    DWORD exception_code;
    unsigned int runtime_exception_code;
} VrExceptionMapEntry;

static const VrExceptionMapEntry g_exception_map[] = {
    { 0xC000008Eu, 0x83u }, /* floating point divide by zero */
    { 0xC0000090u, 0x81u }, /* invalid floating point operation */
    { 0xC0000091u, 0x84u }, /* floating point overflow */
    { 0xC0000093u, 0x85u }, /* floating point underflow */
    { 0xC000008Du, 0x82u }, /* floating point denormal operand */
    { 0xC000008Fu, 0x86u }, /* floating point inexact result */
    { 0xC0000092u, 0x8Au }  /* floating point stack check */
};

/*
 * Code at VA 0x4C1300 locates a matching registered runtime handler, maps
 * several NT floating-point exception codes to internal values 0x81..0x8A,
 * and invokes a callback with category 8 and the mapped value. It restores
 * the prior mapped-code global after dispatch. This table captures only the
 * observed exception-to-code mapping, not the complete callback protocol.
 */
static unsigned int vr_runtime_code_for_exception(DWORD exception_code)
{
    unsigned int i;
    for (i = 0; i < sizeof(g_exception_map) / sizeof(g_exception_map[0]); ++i)
        if (g_exception_map[i].exception_code == exception_code)
            return g_exception_map[i].runtime_exception_code;
    return 0;
}
