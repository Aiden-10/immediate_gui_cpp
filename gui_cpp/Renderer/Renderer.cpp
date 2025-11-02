#include "Renderer.h"
#include "Texture/WicTextureLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

Renderer::Renderer(ID3D11Device* dev, ID3D11DeviceContext* ctx)
    : device(dev), context(ctx)
{
    ui = std::make_unique<Ui>(this);

    HRESULT hr = CreateWICTextureFromFile(device, context, L"font.png", nullptr, &fontTextureView);
    if (FAILED(hr)) {
        std::cerr << "Failed to load font texture\n";
    }
    else {
        LoadFontMap("font.fnt");
        
        ID3D11Resource* fontRes = nullptr;
        fontTextureView->GetResource(&fontRes);

        ID3D11Texture2D* fontTex = nullptr;
        fontRes->QueryInterface(&fontTex);

        D3D11_TEXTURE2D_DESC desc;
        fontTex->GetDesc(&desc);

        textureWidth = desc.Width;
        textureHeight = desc.Height;

        fontTex->Release();
        fontRes->Release();
    }

    

    InitPipeline();
}

Renderer::~Renderer() {
    if (gpuVertexBuffer) gpuVertexBuffer->Release();
    if (inputLayout) inputLayout->Release();
    if (vertexShader) vertexShader->Release();
    if (pixelShader) pixelShader->Release();
}

void Renderer::InitPipeline() {
    const char* vsSrc = R"(
    struct VS_IN {
        float3 pos : POSITION;
        float4 color : COLOR;
        float2 uv : TEXCOORD0;
        float texIndex : TEXCOORD1;
    };

    struct PS_IN {
        float4 pos : SV_POSITION;
        float4 color : COLOR;
        float2 uv : TEXCOORD0;
        float texIndex : TEXCOORD1;
    };

    PS_IN main(VS_IN input) {
        PS_IN o;
        o.pos = float4(input.pos, 1.0f);
        o.color = input.color;
        o.uv = input.uv;
        o.texIndex = input.texIndex;
        return o;
    }
    )";

    const char* psSrc = R"(
    Texture2D fontTex : register(t0);
    SamplerState fontSampler : register(s0);

    struct PS_IN {
        float4 pos : SV_POSITION;
        float4 color : COLOR;
        float2 uv : TEXCOORD0;
        float texIndex : TEXCOORD1;
    };

    float4 main(PS_IN input) : SV_Target {
        if (input.texIndex < 0.5f) {
            // geometry (no texture)
            return input.color;
        } else {
            float4 texc = fontTex.Sample(fontSampler, input.uv);
            return texc * input.color;
        }
    }
    )";

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = D3DCompile(vsSrc, strlen(vsSrc), nullptr, nullptr, nullptr, "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) { std::cerr << "[VS] " << (char*)errorBlob->GetBufferPointer() << std::endl; errorBlob->Release(); }
        return;
    }
    hr = D3DCompile(psSrc, strlen(psSrc), nullptr, nullptr, nullptr, "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) { std::cerr << "[PS] " << (char*)errorBlob->GetBufferPointer() << std::endl; errorBlob->Release(); }
        vsBlob->Release();
        return;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &vertexShader);
    if (FAILED(hr)) std::cerr << "Failed to create vertex shader\n";
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &pixelShader);
    if (FAILED(hr)) std::cerr << "Failed to create pixel shader\n";

    // Input layout matching Vertex struct:
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, 0,                           D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT,0, 12,                          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,      0, 28,                          D3D11_INPUT_PER_VERTEX_DATA, 0 }, // uv -> TEXCOORD0
        { "TEXCOORD", 1, DXGI_FORMAT_R32_FLOAT,         0, 36,                          D3D11_INPUT_PER_VERTEX_DATA, 0 }, // texIndex -> TEXCOORD1
    };
    UINT numElements = ARRAYSIZE(layout);
    hr = device->CreateInputLayout(layout, numElements, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &inputLayout);
    if (FAILED(hr)) std::cerr << "Failed to create input layout\n";

    vsBlob->Release();
    psBlob->Release();

    // create initial dynamic gpu vertex buffer (size in vertices)
    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(Vertex) * 4096; // start capacity
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&bd, nullptr, &gpuVertexBuffer);
    if (FAILED(hr)) std::cerr << "Failed to create dynamic vertex buffer\n";
    gpuBufferCapacity = 4096;

    // create blend state & sampler 
    D3D11_BLEND_DESC blend_desc = {};
    blend_desc.RenderTarget[0].BlendEnable = TRUE;
    blend_desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend_desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blend_desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device->CreateBlendState(&blend_desc, &alphaBlendState);

    D3D11_SAMPLER_DESC samp = {};
    samp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device->CreateSamplerState(&samp, &fontSampler);
}

void Renderer::Begin() {
    vertexCount = 0;
    context->IASetInputLayout(inputLayout);
    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);
    context->OMSetBlendState(alphaBlendState, nullptr, 0xffffffff);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


    //viewport
    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<FLOAT>(windowWidth);
    vp.Height = static_cast<FLOAT>(windowHeight);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    context->RSSetViewports(1, &vp);

    //update mouse
    ui->UpdateMouseAndKey(hwnd);
}

void Renderer::AddTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color)
{
    if (windowWidth == 0 || windowHeight == 0) return;

    
    float area = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);

    // If area < 0, vertices are clockwise, so swap to make CCW
    if (area < 0) {
        std::swap(b, c);
    }

    auto toNDC = [this](Vec2 p) -> Vec2 {
        return {
            (p.x / windowWidth) * 2.0f - 1.0f,
            1.0f - (p.y / windowHeight) * 2.0f
        };
        };

    Vec2 ndc_a = toNDC(a);
    Vec2 ndc_b = toNDC(b);
    Vec2 ndc_c = toNDC(c);

    vertexBufferData.push_back({ ndc_a.x, ndc_a.y, 0.f, color.r, color.g, color.b, color.a, 0.f, 0.f, 0.f });
    vertexBufferData.push_back({ ndc_b.x, ndc_b.y, 0.f, color.r, color.g, color.b, color.a, 0.f, 0.f, 0.f });
    vertexBufferData.push_back({ ndc_c.x, ndc_c.y, 0.f, color.r, color.g, color.b, color.a, 0.f, 0.f, 0.f });
}

void Renderer::AddLine(Vec2 a, Vec2 b, const Color& color, float thickness)
{
    Vec2 dir = b - a;
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len == 0.0f) return;
    dir.x /= len;
    dir.y /= len;

    // Perpendicular vector scaled by half thickness
    Vec2 normal = { -dir.y * thickness * 0.5f, dir.x * thickness * 0.5f };

    // Four corners of quad
    Vec2 v0 = a + normal;
    Vec2 v1 = b + normal;
    Vec2 v2 = b - normal;
    Vec2 v3 = a - normal;

    AddTriangle(v0, v1, v2, color);
    AddTriangle(v2, v3, v0, color);
}

void Renderer::AddRectangle(Vec2 topLeft, Vec2 size, const Color& color, float thickness)
{
    Vec2 topRight = { topLeft.x + size.x, topLeft.y };
    Vec2 bottomRight = { topLeft.x + size.x, topLeft.y + size.y };
    Vec2 bottomLeft = { topLeft.x, topLeft.y + size.y };

    AddLine({topLeft.x - 1.f , topLeft.y - 1.f}, topRight, color, thickness);
    AddLine(topRight, bottomRight, color, thickness);
    AddLine(bottomRight, bottomLeft, color, thickness);
    AddLine(bottomLeft, topLeft, color, thickness);
}

void Renderer::AddRectangleFilled(Vec2 topLeft, Vec2 size, const Color& color)
{
    Vec2 topRight = { topLeft.x + size.x, topLeft.y };
    Vec2 bottomRight = { topLeft.x + size.x, topLeft.y + size.y };
    Vec2 bottomLeft = { topLeft.x, topLeft.y + size.y };

    AddTriangle(topLeft, topRight, bottomRight, color);
    AddTriangle(bottomRight, bottomLeft, topLeft, color);
}

void Renderer::AddCircle(Vec2 center, float radius, const Color& color, float thickness, int segments)
{
    float step = 2.0f * 3.14159265358979323846 / segments;
    for (int i = 0; i < segments; i++)
    {
        float theta1 = i * step;
        float theta2 = (i + 1) * step;

        Vec2 p1 = { center.x + radius * std::cos(theta1),
                    center.y + radius * std::sin(theta1) };

        Vec2 p2 = { center.x + radius * std::cos(theta2),
                    center.y + radius * std::sin(theta2) };

        AddLine(p1, p2, color, thickness);
    }
}

void Renderer::AddCircleFilled(Vec2 center, float radius, const Color& color, int segments)
{
    float step = 2.0f * 3.14159265358979323846 / segments;
    for (int i = 0; i < segments; i++)
    {
        float theta1 = i * step;
        float theta2 = (i + 1) * step;

        Vec2 p1 = { center.x + radius * std::cos(theta1),
                    center.y + radius * std::sin(theta1) };

        Vec2 p2 = { center.x + radius * std::cos(theta2),
                    center.y + radius * std::sin(theta2) };

        AddTriangle(center, p1, p2, color);
    }
}

void Renderer::DrawChar(float x, float y, float w, float h,
    float u0, float v0, float u1, float v1,
    float r, float g, float b, float a)
{
    // Convert pixel coords to NDC
    float ndcX = (x / windowWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (y / windowHeight) * 2.0f;
    float ndcW = (w / windowWidth) * 2.0f;
    float ndcH = (h / windowHeight) * 2.0f;

    // Triangle 1
    vertexBufferData.push_back({ ndcX,          ndcY,          0.0f, r, g, b, a, u0, v0, 1.0f });
    vertexBufferData.push_back({ ndcX + ndcW,   ndcY,          0.0f, r, g, b, a, u1, v0, 1.0f });
    vertexBufferData.push_back({ ndcX,          ndcY - ndcH,   0.0f, r, g, b, a, u0, v1, 1.0f });
    // Triangle 2
    vertexBufferData.push_back({ ndcX + ndcW,   ndcY,          0.0f, r, g, b, a, u1, v0, 1.0f });
    vertexBufferData.push_back({ ndcX + ndcW,   ndcY - ndcH,   0.0f, r, g, b, a, u1, v1, 1.0f });
    vertexBufferData.push_back({ ndcX,          ndcY - ndcH,   0.0f, r, g, b, a, u0, v1, 1.0f });
}

void Renderer::AddText(float x, float y, const std::string& text, const Color& color, float scale)
{
    float cursorX = x;
    if (text.empty()) return;
    for (char ch : text) {
        int charId = static_cast<unsigned char>(ch);
        auto it = fontMap.find(charId);
        if (it == fontMap.end()) continue;

        const FontChar& fc = it->second;

        float xpos = cursorX + fc.xoffset * scale;
        float ypos = y + fc.yoffset * scale;
        float w = fc.w * scale;
        float h = fc.h * scale;

        DrawChar(xpos, ypos, w, h, fc.u0, fc.v0, fc.u1, fc.v1, color.r, color.g, color.b, color.a);

        cursorX += fc.xadvance * scale;
    }
}

void Renderer::FlushBatch()
{
    size_t count = vertexBufferData.size();
    if (count == 0) return;

    EnsureBufferSize(count);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr = context->Map(gpuVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;

    memcpy(mapped.pData, vertexBufferData.data(), count * sizeof(Vertex));
    context->Unmap(gpuVertexBuffer, 0);

    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetInputLayout(inputLayout);
    context->IASetVertexBuffers(0, 1, &gpuVertexBuffer, &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->VSSetShader(vertexShader, nullptr, 0);
    context->PSSetShader(pixelShader, nullptr, 0);

    context->PSSetShaderResources(0, 1, &fontTextureView);
    context->PSSetSamplers(0, 1, &fontSampler);

    // alpha blending
    float blendFactor[4] = { 0,0,0,0 };
    context->OMSetBlendState(alphaBlendState, blendFactor, 0xffffffff);

    // Draw all vertices in order
    context->Draw((UINT)count, 0);

    vertexBufferData.clear();

    ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
    context->PSSetShaderResources(0, 1, nullSRV);
}

void Renderer::EnsureBufferSize(size_t required)
{
    if (required <= gpuBufferCapacity) return;

    if (gpuVertexBuffer) gpuVertexBuffer->Release();

    D3D11_BUFFER_DESC desc = {};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = (UINT)(required * sizeof(Vertex));
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&desc, nullptr, &gpuVertexBuffer);
    if (FAILED(hr)) {
        std::cerr << "Failed to grow GPU buffer\n";
        return;
    }

    gpuBufferCapacity = required;
}

bool Renderer::LoadFontMap(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open font map: " << path << "\n";
        return false;
    }

    int texWidth = 0, texHeight = 0;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);

        std::string token;
        ss >> token;

        if (token == "common") {
            
            while (ss >> token) {
                if (token.find("scaleW=") == 0)
                    texWidth = std::stoi(token.substr(7));
                else if (token.find("scaleH=") == 0)
                    texHeight = std::stoi(token.substr(7));
            }
        }
        else if (token == "char") {
            FontChar fc{};
            int id = 0;
            while (ss >> token) {
                if (token.find("id=") == 0)
                    id = std::stoi(token.substr(3));
                else if (token.find("x=") == 0)
                    fc.x = std::stoi(token.substr(2));
                else if (token.find("y=") == 0)
                    fc.y = std::stoi(token.substr(2));
                else if (token.find("width=") == 0)
                    fc.w = std::stoi(token.substr(6));
                else if (token.find("height=") == 0)
                    fc.h = std::stoi(token.substr(7));
                else if (token.find("xoffset=") == 0)
                    fc.xoffset = std::stoi(token.substr(8));
                else if (token.find("yoffset=") == 0)
                    fc.yoffset = std::stoi(token.substr(8));
                else if (token.find("xadvance=") == 0)
                    fc.xadvance = std::stoi(token.substr(9));
            }

            if (texWidth > 0 && texHeight > 0) {
                fc.u0 = static_cast<float>(fc.x) / texWidth;
                fc.v0 = static_cast<float>(fc.y) / texHeight;
                fc.u1 = static_cast<float>(fc.x + fc.w) / texWidth;
                fc.v1 = static_cast<float>(fc.y + fc.h) / texHeight;
            }

            fontMap[id] = fc;
        }
    }

    std::cout << "[Font] Loaded " << fontMap.size() << " characters\n";
    return true;
}

void Renderer::End() {
    ui->End();
    FlushBatch();
}

Renderer::Ui::Ui(Renderer* r) : renderer(r) {}

Renderer::Ui::Window* Renderer::Ui::BeginWindow(const std::string& title, float x, float y, float w, float h) {
    auto it = std::find_if(windows.begin(), windows.end(), [&](const std::unique_ptr<Window>& win) {
        return win->title == title;
    });
    
    if (it != windows.end()) {
        currentWindow = it->get();
        currentWindow->offset = 0.f;
    }
    else {
        // Window was not found, create a new one
        auto newWindow = std::make_unique<Window>();
        newWindow->title = title;
        newWindow->x = x;
        newWindow->y = y;
        newWindow->w = w;
        newWindow->h = h;

        currentWindow = newWindow.get();
        windows.push_back(std::move(newWindow));
    }

    if (currentWindow) {
        currentWindow->components.clear();
    }

    return currentWindow;
}

void Renderer::Ui::End() {

    Window* hotWindow = nullptr;     // window the mouse is currently hovering over.
    Window* activeWindow = nullptr; // window being actively interacted with

    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
        Window* win = it->get();
        if (!win->visible) continue;

        bool mouseIsOverWindow = context.mousePos.x >= win->x && context.mousePos.x <= win->x + win->w &&
            context.mousePos.y >= win->y && context.mousePos.y <= win->y + win->h;

        if (!hotWindow && mouseIsOverWindow) {
            hotWindow = win;
        }

        if (context.mouseClicked && win == hotWindow) {
            bool insideTitleBar = context.mousePos.y <= win->y + UserInterfaceStyles::WindowTitleHeight;
            if (insideTitleBar && win->isDraggable) {
                m_draggedWindow = win;
                dragOffset = { context.mousePos.x - win->x, context.mousePos.y - win->y };

                auto forward_it = (it + 1).base();
                auto window_ptr = std::move(*forward_it);
                windows.erase(forward_it);
                windows.push_back(std::move(window_ptr));
                break;
            }
        }
    }

    if (context.mouseReleased) {
        m_draggedWindow = nullptr;
    }
    if (m_draggedWindow && context.mouseDown) {
        m_draggedWindow->x = context.mousePos.x - dragOffset.x;
        m_draggedWindow->y = context.mousePos.y - dragOffset.y;
    }

    activeWindow = m_draggedWindow ? m_draggedWindow : hotWindow;
    if (activeWindow) {
        for (auto& comp : activeWindow->components)
            comp->Update(context, activeWindow->x, activeWindow->y + UserInterfaceStyles::WindowTitleHeight);
    }



    // Draw All windows
    for (auto& win : windows) {
        if (!win->visible) continue;

        // Background
        renderer->AddRectangleFilled({ win->x, win->y }, { win->w, win->h }, UserInterfaceColors::WindowBackground);
        // Titlebar
        renderer->AddRectangleFilled({ win->x, win->y }, { win->w, UserInterfaceStyles::WindowTitleHeight }, UserInterfaceColors::WindowTitlebar);
        // Background Border
        renderer->AddRectangle({ win->x, win->y }, { win->w, win->h }, UserInterfaceColors::WindowBorder);
        // Title
        renderer->AddText(win->x + 5, win->y + 5, win->title, UserInterfaceColors::TextColor);

        // Update and draw all components relative to window
        for (auto& comp : win->components) {
            comp->Draw(renderer, win->x, win->y + UserInterfaceStyles::WindowTitleHeight);
        }
    }

    currentWindow = nullptr;
}

// ----------------------------
// AddSlider
// ----------------------------
// Creates a slider control in the current window.
// label: text label shown next to the slider
// value: pointer to a float variable that stores the slider's current value
// onChange: optional callback triggered whenever the slider value changes
//
// Example usage:
// float myValue = 0.5f;
// ui.AddSlider("Brightness", &myValue, [](float v){ 
//     printf("Slider value changed: %f\n", v); 
// });

void Renderer::Ui::AddSlider(const std::string& label, float* value, std::function<void(float)> onChange)
{
    if (!currentWindow) return;
    auto sld = std::make_unique<Slider>();
    sld->label = label;
    sld->x = UserInterfaceStyles::BasePadding;
    sld->y = UserInterfaceStyles::SliderPadding + currentWindow->offset;
    sld->value = value;

    currentWindow->offset += UserInterfaceStyles::SliderHeight + UserInterfaceStyles::SliderPadding;
    currentWindow->components.push_back(std::move(sld));
}

// ----------------------------
// AddButton
// ----------------------------
// Adds a clickable button to the current window.
// label: text shown on the button
// onClick: callback function executed when the button is pressed
//
// Example usage:
// ui.AddButton("Press Me", []() {
//     printf("Button clicked!\n");
// });

void Renderer::Ui::AddButton(const std::string& label, std::function<void()> onClick) {
    if (!currentWindow) return;
    auto btn = std::make_unique<Button>();
    btn->label = label;
    btn->x = UserInterfaceStyles::BasePadding;
    btn->y = UserInterfaceStyles::ButtonPadding + currentWindow->offset;
    btn->w = UserInterfaceStyles::ButtonMinWidth;
    btn->h = UserInterfaceStyles::ButtonHeight;
    btn->onClick = onClick;

    currentWindow->offset += btn->h + UserInterfaceStyles::ButtonPadding;
    currentWindow->components.push_back(std::move(btn));
}

// ----------------------------
// AddText
// ----------------------------
// Displays static text in the current window.
// label: text to display
// color: Color struct defining text color
//
// Example usage:
// ui.AddText("Hello, World!", Color(1.0f, 1.0f, 1.0f, 1.0f));

void Renderer::Ui::AddText(const std::string& label, Color color)
{
    if (!currentWindow) return;
    auto txt = std::make_unique<Text>();
    txt->label = label;
    txt->x = UserInterfaceStyles::BasePadding;
    txt->y = UserInterfaceStyles::TextPadding + currentWindow->offset;
    txt->color = color;

    currentWindow->offset += UserInterfaceStyles::TextPadding + 12.f;
    currentWindow->components.push_back(std::move(txt));
}

// ----------------------------
// AddTextInput (Work In Progress)
// ----------------------------
// Creates a text input box for user input.
// label: optional label for the input box
// buffer: pointer to a std::string to store the current text value
//
// Example usage:
// std::string name;
// ui.AddTextInput("Name", &name);

void Renderer::Ui::AddTextInput(const std::string& label, std::string* buffer)
{
    if (!currentWindow) return;
    auto txtInput = std::make_unique<TextInput>();
    txtInput->label = label;
    txtInput->value = buffer;
    txtInput->x = UserInterfaceStyles::BasePadding;
    txtInput->y = UserInterfaceStyles::TextInputPadding + currentWindow->offset;

    currentWindow->offset += UserInterfaceStyles::TextInputPadding;
    currentWindow->components.push_back(std::move(txtInput));

}

// ----------------------------
// AddCheckbox
// ----------------------------
// Adds a checkbox control.
// label: text label next to the checkbox
// value: pointer to a bool storing current checked state
// onToggle: optional callback triggered when checkbox is toggled
//
// Example usage:
// bool showGrid = false;
// ui.AddCheckbox("Show Grid", &showGrid, [](bool checked) {
//     printf("Checkbox is now %s\n", checked ? "checked" : "unchecked");
// });

void Renderer::Ui::AddCheckbox(const std::string& label, bool* value, std::function<void(bool)> onToggle) {
    if (!currentWindow) return;
    auto cb = std::make_unique<Checkbox>();
    cb->label = label;
    cb->x = UserInterfaceStyles::BasePadding;
    cb->y = UserInterfaceStyles::CheckboxPadding + currentWindow->offset;
    cb->size = UserInterfaceStyles::CheckboxSize;
    if (value) cb->checked = *value;
    cb->onToggle = [value, onToggle](bool v) { if (value) *value = v; if (onToggle) onToggle(v); };
    
    currentWindow->offset += UserInterfaceStyles::CheckboxSize + UserInterfaceStyles::CheckboxPadding;
    currentWindow->components.push_back(std::move(cb));
}

void Renderer::Ui::UpdateMouseAndKey(HWND hwnd)
{
    // Mouse
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(hwnd, &p);
    context.mousePos = p;

    bool leftDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    context.mouseClicked = leftDown && !context.mousePrevDown;
    context.mouseReleased = !leftDown && context.mousePrevDown;
    context.mouseDown = leftDown;
    context.mousePrevDown = leftDown;
    // mouse held

    // Keyboard
    context.textInput.clear();
    for (int vk = 0x08; vk <= 0xFF; vk++)  // scan all keys
    {
        bool pressed = (GetAsyncKeyState(vk) & 0x8000) != 0;

        switch (vk)
        {
        case VK_BACK:  context.keyBackspace = pressed; break;
        case VK_RETURN:context.keyEnter = pressed; break;
        case VK_LEFT:  context.keyLeft = pressed; break;
        case VK_RIGHT: context.keyRight = pressed; break;
            // Letters A-Z
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
        case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
        case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
        case 'V': case 'W': case 'X': case 'Y': case 'Z':
            if (pressed)
                context.textInput.push_back((char)vk);  // simple ASCII
            break;
            // Numbers 0-9
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            if (pressed)
                context.textInput.push_back((char)vk);
            break;
        }
    }
}

