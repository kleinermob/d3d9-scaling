#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <cmath>

// Globals
HMODULE realD3D9 = nullptr;
UINT nativeWidth = 0;
UINT nativeHeight = 0;
bool initialized = false;

// Function pointers for real d3d9 functions
typedef IDirect3D9* (WINAPI *Direct3DCreate9_t)(UINT SDKVersion);
Direct3DCreate9_t RealDirect3DCreate9 = nullptr;

// Helper function to get the real d3d9.dll functions
template<typename T>
T GetRealFunction(const char* funcName)
{
    if (!realD3D9)
    {
        char systemPath[MAX_PATH];
        GetSystemDirectoryA(systemPath, MAX_PATH);
        strcat_s(systemPath, "\\d3d9.dll");
        realD3D9 = LoadLibraryA(systemPath);
        if (!realD3D9)
            return nullptr;
    }
    
    return (T)GetProcAddress(realD3D9, funcName);
}

// Modified Device class to handle present
class MyDirect3DDevice9 : public IDirect3DDevice9
{
private:
    IDirect3DDevice9* realDevice;
    D3DPRESENT_PARAMETERS presentParams;
    bool forceIntegerScaling;
    RECT sourceRect;
    RECT destRect;

public:
    MyDirect3DDevice9(IDirect3DDevice9* real, D3DPRESENT_PARAMETERS* pPresentationParameters) 
        : realDevice(real), presentParams(*pPresentationParameters), forceIntegerScaling(false)
    {
        CalculateScalingRects();
    }

    void CalculateScalingRects()
    {
        if (presentParams.Windowed)
        {
            // Get window client area
            RECT clientRect;
            GetClientRect(presentParams.hDeviceWindow, &clientRect);
            
            // For borderless windowed, treat it like fullscreen
            if (IsBorderlessFullscreen(presentParams.hDeviceWindow))
            {
                // Use the entire screen
                sourceRect.left = 0;
                sourceRect.top = 0;
                sourceRect.right = presentParams.BackBufferWidth;
                sourceRect.bottom = presentParams.BackBufferHeight;
                
                destRect.left = 0;
                destRect.top = 0;
                destRect.right = nativeWidth;
                destRect.bottom = nativeHeight;
            }
            else
            {
                // Regular windowed - no scaling
                sourceRect.left = 0;
                sourceRect.top = 0;
                sourceRect.right = presentParams.BackBufferWidth;
                sourceRect.bottom = presentParams.BackBufferHeight;
                
                destRect = clientRect;
            }
        }
        else
        {
            // Fullscreen mode
            sourceRect.left = 0;
            sourceRect.top = 0;
            sourceRect.right = presentParams.BackBufferWidth;
            sourceRect.bottom = presentParams.BackBufferHeight;
            
            destRect.left = 0;
            destRect.top = 0;
            destRect.right = nativeWidth;
            destRect.bottom = nativeHeight;
        }

        // Check if we should apply integer scaling
        if (nativeWidth > 0 && nativeHeight > 0 && 
            presentParams.BackBufferWidth != nativeWidth && 
            presentParams.BackBufferHeight != nativeHeight)
        {
            float originalAspect = static_cast<float>(presentParams.BackBufferWidth) / presentParams.BackBufferHeight;
            float nativeAspect = static_cast<float>(nativeWidth) / nativeHeight;
            
            if (std::abs(originalAspect - nativeAspect) < 0.01f)
            {
                UINT scaleX = nativeWidth / presentParams.BackBufferWidth;
                UINT scaleY = nativeHeight / presentParams.BackBufferHeight;
                
                if (scaleX >= 1 && scaleY >= 1)
                {
                    forceIntegerScaling = true;
                    
                    // Calculate centered rects
                    UINT scaledWidth = presentParams.BackBufferWidth * scaleX;
                    UINT scaledHeight = presentParams.BackBufferHeight * scaleY;
                    
                    sourceRect.left = 0;
                    sourceRect.top = 0;
                    sourceRect.right = presentParams.BackBufferWidth;
                    sourceRect.bottom = presentParams.BackBufferHeight;
                    
                    destRect.left = (nativeWidth - scaledWidth) / 2;
                    destRect.top = (nativeHeight - scaledHeight) / 2;
                    destRect.right = destRect.left + scaledWidth;
                    destRect.bottom = destRect.top + scaledHeight;
                }
            }
        }
    }

    bool IsBorderlessFullscreen(HWND hWnd)
    {
        if (!hWnd) return false;
        
        RECT windowRect, clientRect;
        GetWindowRect(hWnd, &windowRect);
        GetClientRect(hWnd, &clientRect);
        
        // Convert client rect to screen coordinates
        POINT pt = {0, 0};
        ClientToScreen(hWnd, &pt);
        clientRect.left = pt.x;
        clientRect.top = pt.y;
        pt.x = clientRect.right;
        pt.y = clientRect.bottom;
        ClientToScreen(hWnd, &pt);
        clientRect.right = pt.x;
        clientRect.bottom = pt.y;
        
        // Check if window covers the entire screen
        return (windowRect.left <= 0 && windowRect.top <= 0 &&
                windowRect.right >= (LONG)nativeWidth && 
                windowRect.bottom >= (LONG)nativeHeight &&
                clientRect.left <= 0 && clientRect.top <= 0 &&
                clientRect.right >= (LONG)nativeWidth && 
                clientRect.bottom >= (LONG)nativeHeight);
    }

    // IUnknown methods
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj) { return realDevice->QueryInterface(riid, ppvObj); }
    ULONG WINAPI AddRef() { return realDevice->AddRef(); }
    ULONG WINAPI Release() { return realDevice->Release(); }

    // IDirect3DDevice9 methods (forward most calls)
    HRESULT WINAPI TestCooperativeLevel() { return realDevice->TestCooperativeLevel(); }
    UINT WINAPI GetAvailableTextureMem() { return realDevice->GetAvailableTextureMem(); }
    HRESULT WINAPI EvictManagedResources() { return realDevice->EvictManagedResources(); }
    HRESULT WINAPI GetDirect3D(IDirect3D9** ppD3D9) { return realDevice->GetDirect3D(ppD3D9); }
    // ... (forward all other IDirect3DDevice9 methods)

    // Modified Present method
    HRESULT WINAPI Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
    {
        if (forceIntegerScaling)
        {
            return realDevice->Present(&sourceRect, &destRect, hDestWindowOverride, pDirtyRegion);
        }
        else if (IsBorderlessFullscreen(presentParams.hDeviceWindow) || !presentParams.Windowed)
        {
            // For borderless fullscreen or regular fullscreen, use our calculated rects
            return realDevice->Present(&sourceRect, &destRect, hDestWindowOverride, pDirtyRegion);
        }
        
        // Default behavior for regular windowed mode
        return realDevice->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }
};

// Modified CreateDevice function
class MyDirect3D9 : public IDirect3D9
{
private:
    IDirect3D9* realD3D;
    
public:
    MyDirect3D9(IDirect3D9* real) : realD3D(real) {}
    
    // IUnknown methods
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj) { return realD3D->QueryInterface(riid, ppvObj); }
    ULONG WINAPI AddRef() { return realD3D->AddRef(); }
    ULONG WINAPI Release() { return realD3D->Release(); }
    
    // IDirect3D9 methods (forward most calls)
    HRESULT WINAPI RegisterSoftwareDevice(void* pInitializeFunction) { return realD3D->RegisterSoftwareDevice(pInitializeFunction); }
    UINT WINAPI GetAdapterCount() { return realD3D->GetAdapterCount(); }
    HRESULT WINAPI GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) { return realD3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier); }
    // ... (forward all other IDirect3D9 methods)

    HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
    {
        // Store native resolution if not already set
        if (!initialized)
        {
            D3DDISPLAYMODE displayMode;
            if (SUCCEEDED(realD3D->GetAdapterDisplayMode(Adapter, &displayMode))
            {
                nativeWidth = displayMode.Width;
                nativeHeight = displayMode.Height;
                initialized = true;
            }
        }

        HRESULT hr = realD3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);
        if (SUCCEEDED(hr))
        {
            *ppReturnedDeviceInterface = new MyDirect3DDevice9(*ppReturnedDeviceInterface, pPresentationParameters);
        }
        
        return hr;
    }
};

// Our modified Direct3DCreate9 function
extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT SDKVersion)
{
    if (!RealDirect3DCreate9)
    {
        RealDirect3DCreate9 = GetRealFunction<Direct3DCreate9_t>("Direct3DCreate9");
        if (!RealDirect3DCreate9)
            return nullptr;
    }
    
    IDirect3D9* realD3D = RealDirect3DCreate9(SDKVersion);
    if (!realD3D)
        return nullptr;
    
    return new MyDirect3D9(realD3D);
}

// DllMain and exports remain the same as previous version
