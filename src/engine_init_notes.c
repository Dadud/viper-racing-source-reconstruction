/*
 * Evidence-backed DirectDraw startup reconstruction for GameBin/race.bin.
 *
 * Analyst-written pseudocode expressed as C-shaped wrappers, not original
 * source. The engine uses wrapper functions and global interface storage;
 * call sites are given as virtual addresses at preferred base 0x400000.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ddraw.h>

typedef void (*VrLogFailure)(const char *message, HRESULT result);
typedef void (*VrFatalCleanup)(unsigned int reason);
typedef HRESULT (*VrCreateDraw)(GUID *driver, LPDIRECTDRAW *out, IUnknown *outer);

typedef struct VrDrawState {
    LPDIRECTDRAW draw;
    LPDIRECTDRAW2 draw2;
    IUnknown *secondary_interface;
    HRESULT last_result;
} VrDrawState;

/* Internal wrappers observed in this binary, not Windows API names. */
extern HRESULT __cdecl vr_create_draw_wrapper(GUID *, LPDIRECTDRAW *, IUnknown *); /* 0x4BAE26 */
extern void __cdecl vr_log_hresult(const char *, HRESULT);                        /* 0x452770 */
extern void __cdecl vr_fatal_shutdown(unsigned int);                             /* indirect 0x56D488 */

/* Strings from race.bin: RVAs 0xDB8AC and 0xDB8D0. */
static const char kCreateDrawError[] = "Unable to create DirectDraw Object";
static const char kGetDraw2Error[] = "Unable to get DirectDraw2 interface";
static const char kGetSecondaryError[] = "Can't find IDirect3D2";

/*
 * Reconstructs the observed function at VA 0x458470:
 *  - call wrapper 0x4BAE26 with driver GUID and output interface;
 *  - on failure log the create-object string, invoke fatal shutdown(2);
 *  - otherwise QueryInterface for IID_IDirectDraw2 (GUID at 0x4C8AC0);
 *  - release the base IDirectDraw reference after the query;
 *  - on failure log the HRESULT and DirectDraw2 error string;
 *  - pass the successful interface onward through helper 0x414170.
 * The helper's returned object and subsequent setup are still unresolved.
 */
HRESULT vr_engine_create_draw_stage(VrDrawState *state, GUID *driver,
                                    VrLogFailure log_failure,
                                    VrFatalCleanup fatal_cleanup)
{
    LPDIRECTDRAW base = NULL;
    HRESULT hr;
    if (!state)
        return E_POINTER;
    state->draw = NULL;
    state->draw2 = NULL;
    state->secondary_interface = NULL;

    hr = vr_create_draw_wrapper(driver, &base, NULL);
    state->last_result = hr;
    if (FAILED(hr)) {
        if (log_failure) log_failure(kCreateDrawError, hr);
        if (fatal_cleanup) fatal_cleanup(2);
        return hr;
    }

    /* Original vtable call is QueryInterface(IID_IDirectDraw2, &draw2). */
    hr = base->lpVtbl->QueryInterface(base, &IID_IDirectDraw2,
                                      (void **)&state->draw2);
    state->last_result = hr;
    base->lpVtbl->Release(base);
    if (FAILED(hr)) {
        if (log_failure) log_failure(kGetDraw2Error, hr);
        if (fatal_cleanup) fatal_cleanup(2);
        return hr;
    }
    return S_OK;
}

/*
 * VA 0x458940 queries a secondary interface using GUID data at 0x4C8B40.
 * The failure string names IDirect3D2, but the queried GUID/type is not
 * resolved here. Keep the IID and result type supplied by the caller.
 */
HRESULT vr_engine_query_secondary_stage(VrDrawState *state, REFIID iid,
                                        VrLogFailure log_failure,
                                        VrFatalCleanup fatal_cleanup)
{
    HRESULT hr;
    if (!state || !state->draw2)
        return E_POINTER;
    hr = ((IUnknown *)state->draw2)->lpVtbl->QueryInterface(
        (IUnknown *)state->draw2, iid, (void **)&state->secondary_interface);
    state->last_result = hr;
    if (FAILED(hr)) {
        if (log_failure) log_failure(kGetSecondaryError, hr);
        if (fatal_cleanup) fatal_cleanup(2);
    }
    return hr;
}

/*
 * Additional confirmed graphics failure points:
 *
 * - VA 0x44E080 calls 0x44E460, then logs "Unable to initialize DirectDraw"
 *   (RVA 0xD9D28) on failure.
 * - VA 0x458E55 logs "can't query directdrawsurface3" (RVA 0xDBAE8) when a
 *   surface QueryInterface path fails.
 * - VA 0x4BAA77 logs "intro: Can't get DirectDraw stuff" (RVA 0xE93A0).
 *
 * These are distinct failure paths; the object graph and exact ordering among
 * them need additional xref and data-flow analysis before merging into one
 * initialization routine.
 */
