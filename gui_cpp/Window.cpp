#include "Window.h"

Window* window = nullptr;

LRESULT CALLBACK FuserWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Window* window = (Window*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	switch (msg)
	{
	case WM_CREATE:
	{
		Window* window = (Window*)((LPCREATESTRUCT)lparam)->lpCreateParams;
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)window);
		break;
	}

	case WM_CHAR:
	{
		
		break;
	}

	case WM_SIZE:
	{
		if (window && wparam != SIZE_MINIMIZED) {
			window->onResize(LOWORD(lparam), HIWORD(lparam));
		}
		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hwnd);
		return 0;
	}

	case WM_DESTROY:
	{
		if (window) {
			window->onDestroy();
		}
		return 0;
	}

	default:
		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	return 0;
}

Window::Window(const char name_[24], POINT size)
{
	WNDCLASSEX windowclass = {};
	windowclass.cbClsExtra = NULL;
	windowclass.cbSize = sizeof(WNDCLASSEX);
	windowclass.cbWndExtra = NULL;
	windowclass.hbrBackground = (HBRUSH)COLOR_WINDOW;
	windowclass.style = CS_DBLCLKS;
	windowclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	windowclass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	windowclass.hInstance = GetModuleHandle(NULL);
	windowclass.lpszClassName = name_;
	windowclass.lpszMenuName = "";
	windowclass.lpfnWndProc = &FuserWndProc;

	if (!RegisterClassEx(&windowclass)) {}
	// error

	m_hwnd = CreateWindowEx(
		WS_EX_APPWINDOW,					
		name_,								
		name_,								// Window title
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,	
		GetSystemMetrics(SM_CXSCREEN) / 4,
		GetSystemMetrics(SM_CYSCREEN) / 4,
		size.x, size.y,						
		NULL,								
		NULL,								// No menu
		NULL,								// Instance handle
		this								// Pointer to additional data
	);
	if (!m_hwnd)
	{
		std::cerr << "Failed to create Fuser window: " << GetLastError() << "\n";
	}

	DXGI_SWAP_CHAIN_DESC sd{};
	sd.BufferDesc.RefreshRate.Numerator = 60U;
	sd.BufferDesc.RefreshRate.Denominator = 1U;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.SampleDesc.Count = 1U;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = 2U;
	sd.OutputWindow = m_hwnd;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	constexpr D3D_FEATURE_LEVEL levels[2]
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_0
	};

	D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0U,
		levels,
		2U,
		D3D11_SDK_VERSION,
		&sd,
		&swap_chain,
		&device,
		&level,
		&device_context
	);

	ID3D11Texture2D* back_buffer{ nullptr };
	swap_chain->GetBuffer(0U, IID_PPV_ARGS(&back_buffer));

	if (back_buffer)
	{
		device->CreateRenderTargetView(back_buffer, nullptr, &render_target_view);
		back_buffer->Release();
	}

	ShowWindow(m_hwnd, SW_SHOW);

	UpdateWindow(m_hwnd);

	renderer = new Renderer(device, device_context);

	renderer->SetHWND(m_hwnd);
	renderer->SetWindowSize(size.x, size.y);

	b_is_run = true;
}

Window::~Window()
{

}

bool Window::release()
{
	if (!DestroyWindow(m_hwnd))
		return false;

	return true;
}

void Window::onResize(UINT width, UINT height)
{
	size.x = width;
	size.y = height;

	if (device) {
		if (render_target_view) { render_target_view->Release(); render_target_view = nullptr; }
		swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
		ID3D11Texture2D* pBackBuffer;
		swap_chain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
		device->CreateRenderTargetView(pBackBuffer, nullptr, &render_target_view);
		pBackBuffer->Release();

		if (renderer)
			renderer->SetWindowSize(width, height);
	}
}

void Window::onDestroy()
{
	b_is_run = false;


	if (swap_chain) swap_chain->Release();
	if (device_context) device_context->Release();
	if (device) device->Release();
	if (render_target_view) render_target_view->Release();


}

void Window::onUpdate()
{
	constexpr float color[4]{ 0.f, 0.f, 0.f, 1.f };
	device_context->OMSetRenderTargets(1U, &render_target_view, nullptr);
	device_context->ClearRenderTargetView(render_target_view, color);
}

bool Window::broadcast()
{
	MSG msg;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		Sleep(0);
	}

	return b_is_run;;
}

void Window::displayFPS()
{
	static auto lastTime = std::chrono::high_resolution_clock::now();
	static int frameCount = 0;
	static float fps = 0.0f;

	frameCount++;

	auto currentTime = std::chrono::high_resolution_clock::now();
	float delta = std::chrono::duration<float>(currentTime - lastTime).count();

	if (delta >= 1.0f)
	{
		fps = frameCount / delta;
		frameCount = 0;
		lastTime = currentTime;
	}

	std::string fpsText = "FPS: " + std::to_string((int)fps);

	float textWidth = fpsText.length() * 8.0f;
	float x = size.x - textWidth - 10.0f;
	float y = 10.0f;

	renderer->AddText(x, y, fpsText.c_str(), Color(1.f, 1.f, 1.f, 1.f));
}

void Window::present()
{
	swap_chain->Present(1, 0);
}



