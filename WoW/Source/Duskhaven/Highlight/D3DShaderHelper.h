#pragma once

#include <d3d9.h>

namespace Duskhaven::Highlight
{
    IDirect3DPixelShader9* CompilePixelShaderFromHlsl(IDirect3DDevice9* device, const char* code, const char* entryPoint, const char* target);
}
