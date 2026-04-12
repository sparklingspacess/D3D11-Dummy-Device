// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include "MinHook.h"

#pragma comment(lib, "d3d11.lib")

typedef HRESULT(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
Present opres = nullptr;
ID3D11Device* gdevice = nullptr;
ID3D11DeviceContext* gcon = nullptr;
ID3D11RenderTargetView* grtv = nullptr;
HWND ghwnd = nullptr;
WNDPROC owndproc = nullptr;
bool init = false;
bool rerouteinput = false;


LRESULT CALLBACK HkWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//You can use this for input functionality with things like ImGui.
	if (rerouteinput)
	{
		switch (uMsg)
		{
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
		case WM_MOUSEWHEEL:
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
			return true;
		}
	}
	return CallWindowProc(owndproc, hwnd, uMsg, wParam, lParam);
}

void CreateRenderTarget(IDXGISwapChain* swapchain)
{
	ID3D11Texture2D* back = nullptr;
	swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back);
	gdevice->CreateRenderTargetView(back, NULL, &grtv);
	back->Release();
}

void InitInput(HWND hwnd)
{
	ghwnd = hwnd;
	owndproc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)HkWndProc);
}

void* GetPresentAddress()
{
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = GetForegroundWindow();
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;
	ID3D11Device* device;
	ID3D11DeviceContext* context;
	IDXGISwapChain* swapchain;
	if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &sd, &swapchain, &device, nullptr, &context) != S_OK)
	{
		return nullptr;
	}
	void** vtable = *reinterpret_cast<void***>(swapchain);
	void* present = vtable[8];
	swapchain->Release();
	device->Release();
	context->Release();
	return present;
}

HRESULT __stdcall HkPresent(IDXGISwapChain* swapchain, UINT sync, UINT flags)
{
	if (!init)
	{
		if (SUCCEEDED(swapchain->GetDevice(__uuidof(ID3D11Device), (void**)&gdevice)))
		{
			gdevice->GetImmediateContext(&gcon);
			DXGI_SWAP_CHAIN_DESC sd;
			swapchain->GetDesc(&sd);
			CreateRenderTarget(swapchain);
			InitInput(sd.OutputWindow);
			init = true;
		}
	}
	if (init)
	{
		//Some commented out stuff to get the COM interface for the back buffer texture, not really that good but useful.
		//ID3D11Texture2D* backbuffer = nullptr;
		//swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer);
		gcon->OMSetRenderTargets(1, &grtv, NULL);
		//if (backbuffer)
		//{
		//	backbuffer->Release();
		//}
	}
	return opres(swapchain, sync, flags);
}

DWORD WINAPI MainThread(LPVOID)
{
	while (!GetModuleHandleA("d3d11.dll"))
	{
		Sleep(100);
	}
	void* present = GetPresentAddress();
	MH_Initialize();
	MH_CreateHook(present, &HkPresent, (void**)&opres);
	MH_EnableHook(present);
	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		CreateThread(0, 0, MainThread, 0, 0, 0);
	}
	return TRUE;
}

