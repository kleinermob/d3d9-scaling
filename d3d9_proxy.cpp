#include <windows.h>
#include <d3d9.h>
#include <atomic>

#pragma comment(lib, "d3d9.lib")

// Path to real d3d9.dll
static HMODULE real_d3d9 = nullptr;

// Function pointer typedefs
typedef HRESULT(WINAPI* PFN_D3DCreate9)(UINT);
typedef HRESULT(WINAPI* PFN_D3DPRESENT)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
typedef HRESULT(WINAPI* PFN_D3DRESET)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

// Forward declarations
extern "C" {
    HRESULT WINAPI D3DCreate9(UINT);
}

// Function pointers to real functions
PFN_D3DCreate9 real_D3DCreate9 = nullptr;

// Original functions
typedef HRESULT(WINAPI* PFN_D3D9DEVICE_RESET)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
typedef HRESULT(WINAPI* PFN_D3D9DEVICE_PRESENT)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);

PFN_D3D9DEVICE_RESET real_Reset = nullptr;
PFN_D3D9DEVICE_PRESENT real_Present = nullptr;

// Custom device wrapper
class DeviceWrapper : public IDirect3DDevice9 {
public:
    IDirect3DDevice9* realDevice;

    DeviceWrapper(IDirect3DDevice9* device) : realDevice(device) {}

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) {
        return realDevice->QueryInterface(riid, ppvObject);
    }
    STDMETHOD_(ULONG, AddRef)() {
        return realDevice->AddRef();
    }
    STDMETHOD_(ULONG, Release)() {
        ULONG ref = realDevice->Release();
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    // Forward all other methods to realDevice
    // For brevity, only override Present and Reset
    STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
        // Call our custom Present handler
        return PresentHook(realDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) {
        return ResetHook(realDevice, pPresentationParameters);
    }

    // Implement other methods as needed...
    // For simplicity, forward all other calls directly
    // (In production, you should implement all methods properly)
    // Here, we just provide a macro to forward them.
    // For demonstration, only Present and Reset are overridden.
};

// Globals to store original device
static IDirect3DDevice9* g_device = nullptr;

// Target resolution
static int target_width = 1920;  // Change as needed
static int target_height = 1080;

// Flag to indicate if we've initialized scaling
static std::atomic<bool> initialized(false);

// Function to calculate scaled resolution
void GetScaledResolution(int& width, int& height, int nativeWidth, int nativeHeight) {
    float aspectNative = (float)nativeWidth / nativeHeight;
    float aspectTarget = (float)width / height;

    if (width == nativeWidth && height == nativeHeight) {
        // No change
        return;
    }

    // Check aspect ratio
    if (abs(aspectNative - aspectTarget) > 0.01f) {
        // Stretch to fill
        width = target_width;
        height = target_height;
    } else {
        // Integer scaling
        int scaleX = nativeWidth / width;
        int scaleY = nativeHeight / height;
        int scale = min(scaleX, scaleY);
        width = nativeWidth / scale;
        height = nativeHeight / scale;
    }
}

// Hooked Present function
HRESULT WINAPI PresentHook(IDirect3DDevice9* device, const RECT* pSourceRect, const RECT* pDestRect,
    HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {

    if (!initialized.load()) {
        // Initialization code if needed
        initialized.store(true);
    }

    // Get backbuffer description to find native resolution
    IDirect3DSurface9* backBuffer = nullptr;
    if (SUCCEEDED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer))) {
        D3DSURFACE_DESC desc;
        if (SUCCEEDED(backBuffer->GetDesc(&desc))) {
            int width = target_width;
            int height = target_height;

            // Adjust resolution
            GetScaledResolution(width, height, desc.Width, desc.Height);

            // Set new presentation parameters
            D3DPRESENT_PARAMETERS params = {};
            // Obtain current params (not straightforward, so assume known)
            // For simplicity, assume current params are accessible or stored
            // In real code, you'd need to keep track of current params

            // For demo, we just call the original Present
            backBuffer->Release();
            return real_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
        }
        backBuffer->Release();
    }
    return real_Present(device, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

// Hooked Reset function
HRESULT WINAPI ResetHook(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pPresentationParameters) {
    // Adjust backbuffer size here if needed
    int width = pPresentationParameters->BackBufferWidth;
    int height = pPresentationParameters->BackBufferHeight;

    // Retrieve native resolution if possible
    // For simplicity, assume native resolution as 1920x1080
    int nativeWidth = 1920;
    int nativeHeight = 1080;

    GetScaledResolution(width, height, nativeWidth, nativeHeight);

    pPresentationParameters->BackBufferWidth = width;
    pPresentationParameters->BackBufferHeight = height;

    return real_Reset(device, pPresentationParameters);
}

// Our IDirect3DDevice9 wrapper
class DeviceWrapperImpl : public IDirect3DDevice9 {
public:
    IDirect3DDevice9* realDevice;

    DeviceWrapperImpl(IDirect3DDevice9* device) : realDevice(device) {}

    // IUnknown
    STDMETHOD(QueryInterface)(REFIID riid, void** ppvObject) {
        return realDevice->QueryInterface(riid, ppvObject);
    }
    STDMETHOD_(ULONG, AddRef)() {
        return realDevice->AddRef();
    }
    STDMETHOD_(ULONG, Release)() {
        ULONG ref = realDevice->Release();
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    // Forward
    STDMETHOD(Present)(const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) {
        return PresentHook(realDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    STDMETHOD(Reset)(D3DPRESENT_PARAMETERS* pPresentationParameters) {
        return ResetHook(realDevice, pPresentationParameters);
    }

    // Forward all other methods similarly...
    // For brevity, you should implement all methods, but omitted here.
};

// Exported functions
extern "C" {

__declspec(dllexport) HRESULT WINAPI D3DCreate9(UINT SDKVersion) {
    if (!real_d3d9) {
        // Load real d3d9.dll
        char systemPath[MAX_PATH];
        GetSystemDirectoryA(systemPath, MAX_PATH);
        strcat_s(systemPath, "\\d3d9.dll");
        real_d3d9 = LoadLibraryA(systemPath);
        if (!real_d3d9) return E_FAIL;

        real_D3DCreate9 = (PFN_D3DCreate9)GetProcAddress(real_d3d9, "Direct3DCreate9");
        if (!real_D3DCreate9) return E_FAIL;
    }
    return real_D3DCreate9(SDKVersion);
}

// Forward other functions as needed, or load the real DLL's exports dynamically
}

// DLL entry point
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    return TRUE;
}
