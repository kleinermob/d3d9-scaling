#include <windows.h>
#include <d3d9.h>
#include <cmath> // for std::abs

// Forward declarations for hook functions
HRESULT WINAPI PresentHook(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect,
    HWND hDestWindowOverride, const RGNDATA* pDirtyRegion);

HRESULT WINAPI ResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters);

// Existing declaration for D3DCreate9
extern "C" {
    IDirect3D9* WINAPI D3DCreate9(UINT SDKVersion);
}

// Your other code remains unchanged...

// Example usage: ensure these functions are defined somewhere in your code
// For example:
HRESULT WINAPI PresentHook(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect,
    HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
    // Your hook implementation...
    return S_OK;
}

HRESULT WINAPI ResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    // Your hook implementation...
    return S_OK;
}

// Example of fixing the `abs` call in your code
void YourFunction() {
    float aspectNative = /* your value */;
    float aspectTarget = /* your value */;
    if (std::abs(aspectNative - aspectTarget) > 0.01f) {
        // Your logic
    }
}
