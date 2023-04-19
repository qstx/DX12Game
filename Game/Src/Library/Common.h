#pragma once

#include <windows.h>
#include <wrl.h>
#include <exception>
#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "RTTI.h"
#include "GameException.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <dinput.h>
#include <d3dx12.h>

//#define DeleteObject(object) if((object) != NULL) { delete object; object = NULL; }
//#define DeleteObjects(objects) if((objects) != NULL) { delete[] objects; objects = NULL; }
//#define ReleaseObject(object) if((object) != NULL) { object->Release(); object = NULL; }

#ifndef ThrowIfFailed
#define ThrowIfFailed(x,str)                                          \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    if(FAILED(hr__)) { throw GameException(str, hr__); }              \
}
#endif

#ifndef ReleaseCom
#define ReleaseCom(x) { if(x){ x->Release(); x = 0; } }
#endif

namespace Library
{
    typedef unsigned char byte;
}

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;