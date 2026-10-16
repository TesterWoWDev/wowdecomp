#include "WoW/Source/Duskhaven/Highlight/D3DShaderHelper.h"
#include <d3dcompiler.h>
#include <cstring>

namespace Duskhaven::Highlight
{
    typedef HRESULT (WINAPI* PFN_D3DCompile)(
        LPCVOID pSrcData,
        SIZE_T SrcDataSize,
        LPCSTR pSourceName,
        const void* pDefines,
        void* pInclude,
        LPCSTR pEntrypoint,
        LPCSTR pTarget,
        UINT Flags1,
        UINT Flags2,
        ID3DBlob** ppCode,
        ID3DBlob** ppErrorMsgs
    );

    IDirect3DPixelShader9* CompilePixelShaderFromHlsl(IDirect3DDevice9* device, const char* code, const char* entryPoint, const char* target)
    {
        if (!device || !code)
            return nullptr;

        HMODULE hCompiler = LoadLibraryA("d3dcompiler_47.dll");
        if (!hCompiler)
            hCompiler = LoadLibraryA("d3dcompiler_43.dll");
        if (!hCompiler)
            return nullptr;

        PFN_D3DCompile d3dCompile = reinterpret_cast<PFN_D3DCompile>(GetProcAddress(hCompiler, "D3DCompile"));
        if (!d3dCompile)
        {
            FreeLibrary(hCompiler);
            return nullptr;
        }

        ID3DBlob* codeBlob = nullptr;
        ID3DBlob* errorBlob = nullptr;
        HRESULT hr = d3dCompile(code, strlen(code), nullptr, nullptr, nullptr, entryPoint, target, 0, 0, &codeBlob, &errorBlob);
        if (FAILED(hr) || !codeBlob)
        {
            if (errorBlob)
                errorBlob->Release();
            FreeLibrary(hCompiler);
            return nullptr;
        }

        IDirect3DPixelShader9* shader = nullptr;
        hr = device->CreatePixelShader(static_cast<const DWORD*>(codeBlob->GetBufferPointer()), &shader);
        codeBlob->Release();
        if (errorBlob)
            errorBlob->Release();
        FreeLibrary(hCompiler);

        return SUCCEEDED(hr) ? shader : nullptr;
    }
}
