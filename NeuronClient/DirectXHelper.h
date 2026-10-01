#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

#if defined(_DEBUG)
#   include <dxgidebug.h>
#endif

#define D3DX12_NO_STATE_OBJECT_HELPERS
#include "d3dx12.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

#define IID_GRAPHICS_PPV_ARGS(ppType) __uuidof(ppType), (ppType).put_void()