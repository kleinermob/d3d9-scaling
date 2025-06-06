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
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj) override { return realDevice->QueryInterface(riid, ppvObj); }
    ULONG WINAPI AddRef() override { return realDevice->AddRef(); }
    ULONG WINAPI Release() override { 
        ULONG ref = realDevice->Release();
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    // IDirect3DDevice9 methods
    HRESULT WINAPI TestCooperativeLevel() override { return realDevice->TestCooperativeLevel(); }
    UINT WINAPI GetAvailableTextureMem() override { return realDevice->GetAvailableTextureMem(); }
    HRESULT WINAPI EvictManagedResources() override { return realDevice->EvictManagedResources(); }
    HRESULT WINAPI GetDirect3D(IDirect3D9** ppD3D9) override { return realDevice->GetDirect3D(ppD3D9); }
    HRESULT WINAPI GetDeviceCaps(D3DCAPS9* pCaps) override { return realDevice->GetDeviceCaps(pCaps); }
    HRESULT WINAPI GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode) override { return realDevice->GetDisplayMode(iSwapChain, pMode); }
    HRESULT WINAPI GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters) override { return realDevice->GetCreationParameters(pParameters); }
    HRESULT WINAPI SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap) override { return realDevice->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap); }
    void WINAPI SetCursorPosition(int X, int Y, DWORD Flags) override { realDevice->SetCursorPosition(X, Y, Flags); }
    BOOL WINAPI ShowCursor(BOOL bShow) override { return realDevice->ShowCursor(bShow); }
    HRESULT WINAPI CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain) override { return realDevice->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain); }
    HRESULT WINAPI GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain) override { return realDevice->GetSwapChain(iSwapChain, pSwapChain); }
    UINT WINAPI GetNumberOfSwapChains() override { return realDevice->GetNumberOfSwapChains(); }
    HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS* pPresentationParameters) override { 
        HRESULT hr = realDevice->Reset(pPresentationParameters);
        if (SUCCEEDED(hr)) {
            presentParams = *pPresentationParameters;
            CalculateScalingRects();
        }
        return hr;
    }
    HRESULT WINAPI Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion) override {
        if (forceIntegerScaling)
        {
            return realDevice->Present(&sourceRect, &destRect, hDestWindowOverride, pDirtyRegion);
        }
        else if (IsBorderlessFullscreen(presentParams.hDeviceWindow) || !presentParams.Windowed)
        {
            return realDevice->Present(&sourceRect, &destRect, hDestWindowOverride, pDirtyRegion);
        }
        return realDevice->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
    }
    HRESULT WINAPI GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer) override { return realDevice->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer); }
    HRESULT WINAPI GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus) override { return realDevice->GetRasterStatus(iSwapChain, pRasterStatus); }
    HRESULT WINAPI SetDialogBoxMode(BOOL bEnableDialogs) override { return realDevice->SetDialogBoxMode(bEnableDialogs); }
    void WINAPI SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp) override { realDevice->SetGammaRamp(iSwapChain, Flags, pRamp); }
    void WINAPI GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp) override { realDevice->GetGammaRamp(iSwapChain, pRamp); }
    HRESULT WINAPI CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle) override { return realDevice->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle); }
    HRESULT WINAPI CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle) override { return realDevice->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle); }
    HRESULT WINAPI CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle) override { return realDevice->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle); }
    HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle) override { return realDevice->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle); }
    HRESULT WINAPI CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle) override { return realDevice->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle); }
    HRESULT WINAPI CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override { return realDevice->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle); }
    HRESULT WINAPI CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override { return realDevice->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle); }
    HRESULT WINAPI UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint) override { return realDevice->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint); }
    HRESULT WINAPI UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture) override { return realDevice->UpdateTexture(pSourceTexture, pDestinationTexture); }
    HRESULT WINAPI GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface) override { return realDevice->GetRenderTargetData(pRenderTarget, pDestSurface); }
    HRESULT WINAPI GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface) override { return realDevice->GetFrontBufferData(iSwapChain, pDestSurface); }
    HRESULT WINAPI StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter) override { return realDevice->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter); }
    HRESULT WINAPI ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color) override { return realDevice->ColorFill(pSurface, pRect, color); }
    HRESULT WINAPI CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle) override { return realDevice->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle); }
    HRESULT WINAPI SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget) override { return realDevice->SetRenderTarget(RenderTargetIndex, pRenderTarget); }
    HRESULT WINAPI GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget) override { return realDevice->GetRenderTarget(RenderTargetIndex, ppRenderTarget); }
    HRESULT WINAPI SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil) override { return realDevice->SetDepthStencilSurface(pNewZStencil); }
    HRESULT WINAPI GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface) override { return realDevice->GetDepthStencilSurface(ppZStencilSurface); }
    HRESULT WINAPI BeginScene() override { return realDevice->BeginScene(); }
    HRESULT WINAPI EndScene() override { return realDevice->EndScene(); }
    HRESULT WINAPI Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil) override { return realDevice->Clear(Count, pRects, Flags, Color, Z, Stencil); }
    HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) override { return realDevice->SetTransform(State, pMatrix); }
    HRESULT WINAPI GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* pMatrix) override { return realDevice->GetTransform(State, pMatrix); }
    HRESULT WINAPI MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* pMatrix) override { return realDevice->MultiplyTransform(State, pMatrix); }
    HRESULT WINAPI SetViewport(CONST D3DVIEWPORT9* pViewport) override { return realDevice->SetViewport(pViewport); }
    HRESULT WINAPI GetViewport(D3DVIEWPORT9* pViewport) override { return realDevice->GetViewport(pViewport); }
    HRESULT WINAPI SetMaterial(CONST D3DMATERIAL9* pMaterial) override { return realDevice->SetMaterial(pMaterial); }
    HRESULT WINAPI GetMaterial(D3DMATERIAL9* pMaterial) override { return realDevice->GetMaterial(pMaterial); }
    HRESULT WINAPI SetLight(DWORD Index, CONST D3DLIGHT9* pLight) override { return realDevice->SetLight(Index, pLight); }
    HRESULT WINAPI GetLight(DWORD Index, D3DLIGHT9* pLight) override { return realDevice->GetLight(Index, pLight); }
    HRESULT WINAPI LightEnable(DWORD Index, BOOL Enable) override { return realDevice->LightEnable(Index, Enable); }
    HRESULT WINAPI GetLightEnable(DWORD Index, BOOL* pEnable) override { return realDevice->GetLightEnable(Index, pEnable); }
    HRESULT WINAPI SetClipPlane(DWORD Index, CONST float* pPlane) override { return realDevice->SetClipPlane(Index, pPlane); }
    HRESULT WINAPI GetClipPlane(DWORD Index, float* pPlane) override { return realDevice->GetClipPlane(Index, pPlane); }
    HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override { return realDevice->SetRenderState(State, Value); }
    HRESULT WINAPI GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue) override { return realDevice->GetRenderState(State, pValue); }
    HRESULT WINAPI CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB) override { return realDevice->CreateStateBlock(Type, ppSB); }
    HRESULT WINAPI BeginStateBlock() override { return realDevice->BeginStateBlock(); }
    HRESULT WINAPI EndStateBlock(IDirect3DStateBlock9** ppSB) override { return realDevice->EndStateBlock(ppSB); }
    HRESULT WINAPI SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus) override { return realDevice->SetClipStatus(pClipStatus); }
    HRESULT WINAPI GetClipStatus(D3DCLIPSTATUS9* pClipStatus) override { return realDevice->GetClipStatus(pClipStatus); }
    HRESULT WINAPI GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture) override { return realDevice->GetTexture(Stage, ppTexture); }
    HRESULT WINAPI SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture) override { return realDevice->SetTexture(Stage, pTexture); }
    HRESULT WINAPI GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue) override { return realDevice->GetTextureStageState(Stage, Type, pValue); }
    HRESULT WINAPI SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value) override { return realDevice->SetTextureStageState(Stage, Type, Value); }
    HRESULT WINAPI GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue) override { return realDevice->GetSamplerState(Sampler, Type, pValue); }
    HRESULT WINAPI SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value) override { return realDevice->SetSamplerState(Sampler, Type, Value); }
    HRESULT WINAPI ValidateDevice(DWORD* pNumPasses) override { return realDevice->ValidateDevice(pNumPasses); }
    HRESULT WINAPI SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries) override { return realDevice->SetPaletteEntries(PaletteNumber, pEntries); }
    HRESULT WINAPI GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries) override { return realDevice->GetPaletteEntries(PaletteNumber, pEntries); }
    HRESULT WINAPI SetCurrentTexturePalette(UINT PaletteNumber) override { return realDevice->SetCurrentTexturePalette(PaletteNumber); }
    HRESULT WINAPI GetCurrentTexturePalette(UINT PaletteNumber) override { return realDevice->GetCurrentTexturePalette(PaletteNumber); }
    HRESULT WINAPI SetScissorRect(CONST RECT* pRect) override { return realDevice->SetScissorRect(pRect); }
    HRESULT WINAPI GetScissorRect(RECT* pRect) override { return realDevice->GetScissorRect(pRect); }
    HRESULT WINAPI SetSoftwareVertexProcessing(BOOL bSoftware) override { return realDevice->SetSoftwareVertexProcessing(bSoftware); }
    BOOL WINAPI GetSoftwareVertexProcessing() override { return realDevice->GetSoftwareVertexProcessing(); }
    HRESULT WINAPI SetNPatchMode(float nSegments) override { return realDevice->SetNPatchMode(nSegments); }
    float WINAPI GetNPatchMode() override { return realDevice->GetNPatchMode(); }
    HRESULT WINAPI DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount) override { return realDevice->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount); }
    HRESULT WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount) override { return realDevice->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount); }
    HRESULT WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override { return realDevice->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride); }
    HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride) override { return realDevice->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride); }
    HRESULT WINAPI ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags) override { return realDevice->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags); }
    HRESULT WINAPI CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl) override { return realDevice->CreateVertexDeclaration(pVertexElements, ppDecl); }
    HRESULT WINAPI SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl) override { return realDevice->SetVertexDeclaration(pDecl); }
    HRESULT WINAPI GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl) override { return realDevice->GetVertexDeclaration(ppDecl); }
    HRESULT WINAPI SetFVF(DWORD FVF) override { return realDevice->SetFVF(FVF); }
    HRESULT WINAPI GetFVF(DWORD* pFVF) override { return realDevice->GetFVF(pFVF); }
    HRESULT WINAPI CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader) override { return realDevice->CreateVertexShader(pFunction, ppShader); }
    HRESULT WINAPI SetVertexShader(IDirect3DVertexShader9* pShader) override { return realDevice->SetVertexShader(pShader); }
    HRESULT WINAPI GetVertexShader(IDirect3DVertexShader9** ppShader) override { return realDevice->GetVertexShader(ppShader); }
    HRESULT WINAPI SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) override { return realDevice->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    HRESULT WINAPI GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override { return realDevice->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    HRESULT WINAPI SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) override { return realDevice->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    HRESULT WINAPI GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override { return realDevice->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    HRESULT WINAPI SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) override { return realDevice->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
    HRESULT WINAPI GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override { return realDevice->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount); }
    HRESULT WINAPI SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride) override { return realDevice->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride); }
    HRESULT WINAPI GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* pOffsetInBytes, UINT* pStride) override { return realDevice->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride); }
    HRESULT WINAPI SetStreamSourceFreq(UINT StreamNumber, UINT Setting) override { return realDevice->SetStreamSourceFreq(StreamNumber, Setting); }
    HRESULT WINAPI GetStreamSourceFreq(UINT StreamNumber, UINT* pSetting) override { return realDevice->GetStreamSourceFreq(StreamNumber, pSetting); }
    HRESULT WINAPI SetIndices(IDirect3DIndexBuffer9* pIndexData) override { return realDevice->SetIndices(pIndexData); }
    HRESULT WINAPI GetIndices(IDirect3DIndexBuffer9** ppIndexData) override { return realDevice->GetIndices(ppIndexData); }
    HRESULT WINAPI CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader) override { return realDevice->CreatePixelShader(pFunction, ppShader); }
    HRESULT WINAPI SetPixelShader(IDirect3DPixelShader9* pShader) override { return realDevice->SetPixelShader(pShader); }
    HRESULT WINAPI GetPixelShader(IDirect3DPixelShader9** ppShader) override { return realDevice->GetPixelShader(ppShader); }
    HRESULT WINAPI SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount) override { return realDevice->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    HRESULT WINAPI GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount) override { return realDevice->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount); }
    HRESULT WINAPI SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount) override { return realDevice->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    HRESULT WINAPI GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount) override { return realDevice->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount); }
    HRESULT WINAPI SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount) override { return realDevice->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
    HRESULT WINAPI GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount) override { return realDevice->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount); }
    HRESULT WINAPI DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo) override { return realDevice->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo); }
    HRESULT WINAPI DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo) override { return realDevice->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo); }
    HRESULT WINAPI DeletePatch(UINT Handle) override { return realDevice->DeletePatch(Handle); }
    HRESULT WINAPI CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery) override { return realDevice->CreateQuery(Type, ppQuery); }
};

// Modified CreateDevice function
class MyDirect3D9 : public IDirect3D9
{
private:
    IDirect3D9* realD3D;
    
public:
    MyDirect3D9(IDirect3D9* real) : realD3D(real) {}
    
    // IUnknown methods
    HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj) override { return realD3D->QueryInterface(riid, ppvObj); }
    ULONG WINAPI AddRef() override { return realD3D->AddRef(); }
    ULONG WINAPI Release() override { 
        ULONG ref = realD3D->Release();
        if (ref == 0) {
            delete this;
        }
        return ref;
    }
    
    // IDirect3D9 methods
    HRESULT WINAPI RegisterSoftwareDevice(void* pInitializeFunction) override { return realD3D->RegisterSoftwareDevice(pInitializeFunction); }
    UINT WINAPI GetAdapterCount() override { return realD3D->GetAdapterCount(); }
    HRESULT WINAPI GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier) override { return realD3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier); }
    UINT WINAPI GetAdapterModeCount(UINT Adapter, D3DFORMAT Format) override { return realD3D->GetAdapterModeCount(Adapter, Format); }
    HRESULT WINAPI EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode) override { return realD3D->EnumAdapterModes(Adapter, Format, Mode, pMode); }
    HRESULT WINAPI GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode) override { return realD3D->GetAdapterDisplayMode(Adapter, pMode); }
    HRESULT WINAPI CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed) override { return realD3D->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed); }
    HRESULT WINAPI CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override { return realD3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat); }
    HRESULT WINAPI CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels) override { return realD3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels); }
    HRESULT WINAPI CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override { return realD3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat); }
    HRESULT WINAPI CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat) override { return realD3D->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat); }
    HRESULT WINAPI GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps) override { return realD3D->GetDeviceCaps(Adapter, DeviceType, pCaps); }
    HMONITOR WINAPI GetAdapterMonitor(UINT Adapter) override { return realD3D->GetAdapterMonitor(Adapter); }
    HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface) override {
        // Store native resolution if not already set
        if (!initialized)
        {
            D3DDISPLAYMODE displayMode;
            if (SUCCEEDED(realD3D->GetAdapterDisplayMode(Adapter, &displayMode)))
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
