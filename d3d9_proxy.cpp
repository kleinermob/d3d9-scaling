// d3d9_proxy.cpp

#include <windows.h>
#include <d3d9.h>
#include <detours.h> // Download Detours from Microsoft and include in project
#include <atomic>

#pragma comment(lib, "detours.lib")

// Global handle to real d3d9.dll
HMODULE real_d3d9 = nullptr;

// Function pointers to real functions
typedef IDirect3D9* (WINAPI *D3DCreate9_t)(UINT);
D3DCreate9_t real_Direct3DCreate9 = nullptr;

// Forward declaration of our hooked functions
IDirect3D9* WINAPI Hooked_Direct3DCreate9(UINT SDKVersion);
HRESULT WINAPI Hooked_IDirect3DDevice9_Present(IDirect3DDevice9* pDevice, const RECT* pSourceRect,
                                              const RECT* pDestRect, HWND hDestWindowOverride,
                                              const RGNDATA* pDirtyRegion);

// Original function pointers
D3DCreate9_t orig_Direct3DCreate9 = nullptr;

// Our device wrapper
class MyIDirect3DDevice9 : public IDirect3DDevice9 {
public:
    IDirect3DDevice9* realDevice;
    RECT nativeRect; // native resolution
    RECT currentRect; // current resolution
    float aspectRatio; // native aspect ratio

    MyIDirect3DDevice9(IDirect3DDevice9* device, RECT nativeRes)
        : realDevice(device), nativeRect(nativeRes) {
        aspectRatio = (float)nativeRes.right / nativeRes.bottom;
        currentRect.left = 0;
        currentRect.top = 0;
        currentRect.right = nativeRes.right;
        currentRect.bottom = nativeRes.bottom;
    }

    // IUnknown
    ULONG STDMETHODCALLTYPE AddRef() override { return realDevice->AddRef(); }
    ULONG STDMETHODCALLTYPE Release() override { return realDevice->Release(); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        return realDevice->QueryInterface(riid, ppvObject);
    }

    // Forward all methods except Present
    // For brevity, only Present is overridden
    HRESULT STDMETHODCALLTYPE Present(const RECT* pSourceRect, const RECT* pDestRect,
                                        HWND hDestWindowOverride, const RGNDATA* pDirtyRegion) override {
        // Here we implement resolution scaling and aspect ratio correction

        // For simplicity, let's assume we want to scale to a fixed resolution, e.g., 1920x1080
        RECT targetRes = { 0, 0, 1920, 1080 };
        int targetWidth = targetRes.right - targetRes.left;
        int targetHeight = targetRes.bottom - targetRes.top;

        // Get current backbuffer size
        D3DSURFACE_DESC desc;
        IDirect3DSurface9* backBuffer = nullptr;
        realDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        backBuffer->GetDesc(&desc);
        backBuffer->Release();

        int width = desc.Width;
        int height = desc.Height;

        float nativeAR = (float)width / height;
        float targetAR = (float)targetWidth / targetHeight;

        // Decide whether to letterbox/pillarbox or stretch
        if (nativeAR > targetAR) {
            // wider than target, stretch height
            float scale = (float)width / targetWidth;
            int newHeight = (int)(height / scale);
            // Apply stretch
            // For simplicity, just call original Present
        } else {
            // taller than target, stretch width
            float scale = (float)height / targetHeight;
            int newWidth = (int)(width / scale);
            // Apply stretch
        }

        // For demonstration, just call original Present
        return realDevice->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }

    // Forward all other methods
    // For brevity, implement only a few, others should be forwarded similarly
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override {
        return realDevice->TestCooperativeLevel();
    }
    HRESULT STDMETHODCALLTYPE Clear(DWORD Count, const D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override {
        return realDevice->Clear(Count, pRects, Flags, Color, Z, Stencil);
    }
    // ... (Implement all other methods by forwarding)
};

// Function to create our wrapped device
HRESULT CreateWrappedDevice(IDirect3D9* d3d9, UINT adapter, D3DDEVTYPE devType, HWND hFocusWindow,
                            DWORD behaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters,
                            IDirect3DDevice9** outDevice) {
    IDirect3DDevice9* realDevice = nullptr;
    HRESULT hr = d3d9->CreateDevice(adapter, devType, hFocusWindow, behaviorFlags, pPresentationParameters, &realDevice);
    if (FAILED(hr))
        return hr;

    // Wrap the device
    RECT nativeRes = { 0, 0, pPresentationParameters->BackBufferWidth, pPresentationParameters->BackBufferHeight };
    MyIDirect3DDevice9* myDevice = new MyIDirect3DDevice9(realDevice, nativeRes);
    *outDevice = myDevice;
    return S_OK;
}

// Our hooked Direct3DCreate9
IDirect3D9* WINAPI Hooked_Direct3DCreate9(UINT SDKVersion) {
    if (!real_d3d9) return nullptr;
    IDirect3D9* pD3D = ((D3DCreate9_t)orig_Direct3DCreate9)(SDKVersion);
    if (!pD3D) return nullptr;

    // Hook CreateDevice to return our wrapped device
    // For simplicity, let's assume we create a device here
    // But in reality, you'd hook CreateDevice or intercept elsewhere
    // For demonstration, just return the real device
    return pD3D;
}

// DLL Main
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        // Load real d3d9.dll
        real_d3d9 = LoadLibraryA("d3d9.dll");
        if (!real_d3d9) return FALSE;

        // Get original Direct3DCreate9
        orig_Direct3DCreate9 = (D3DCreate9_t)GetProcAddress(real_d3d9, "Direct3DCreate9");
        if (!orig_Direct3DCreate9) return FALSE;

        // Hook Direct3DCreate9
        real_D3DCreate9 = orig_Direct3DCreate9;
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());
        DetourAttach(&(PVOID&)orig_Direct3DCreate9, Hooked_Direct3DCreate9);
        DetourTransactionCommit();
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        // Unhook
        if (orig_Direct3DCreate9) {
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourDetach(&(PVOID&)orig_Direct3DCreate9, Hooked_Direct3DCreate9);
            DetourTransactionCommit();
        }
        if (real_d3d9) FreeLibrary(real_d3d9);
    }
    return TRUE;
}
