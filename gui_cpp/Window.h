#pragma once
#include <Windows.h>
#include <iostream>
#include <d3d11.h>
#include <d3dcommon.h>
#include <chrono>

#include "Renderer/Renderer.h"
class Renderer;


class Window
{

public:
	Window(const char name_[24], POINT size);
	~Window();

	bool release();
	bool isRun() const { return b_is_run; }
	void onResize(UINT width, UINT height);
	void onDestroy();
	void onUpdate();
	bool broadcast();
	void displayFPS();
	void present();

	HWND getHwnd() const { return m_hwnd; }
	IDXGISwapChain* getSwapChain() const { return swap_chain; }
	Renderer* getRenderer() const { return renderer; }

	POINT size;
private:
	bool b_is_run;
	HWND m_hwnd;
	Renderer* renderer = nullptr;
	ID3D11Device* device{ nullptr };
	ID3D11DeviceContext* device_context{ nullptr };
	IDXGISwapChain* swap_chain{ nullptr };
	ID3D11RenderTargetView* render_target_view{ nullptr };
	D3D_FEATURE_LEVEL level{};
};