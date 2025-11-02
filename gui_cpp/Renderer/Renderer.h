#pragma once
#include <d3d11.h>
#include <vector>
#include <unordered_map>
#include <Windows.h>
#include <string>
#include <functional>
#include <memory>

#include "RendererPrimitives.h"
#include "RendererStyles.h"
#include "Texture/WICTextureLoader.h"

class Renderer {
public:
    Renderer(ID3D11Device* device, ID3D11DeviceContext* context);
    ~Renderer();

    // frame control
    void Begin();
    void End();

    // basic drawing
    void AddTriangle(Vec2 a, Vec2 b, Vec2 c, const Color& color);
    void AddLine(Vec2 a, Vec2 b, const Color& color, float thickness);
    void AddRectangle(Vec2 topLeft, Vec2 size, const Color& color, float thickness = 1.f);
    void AddRectangleFilled(Vec2 topLeft, Vec2 size, const Color& color);
    void AddCircle(Vec2 center, float radius, const Color& color, float thickness = 1.f, int segments = 32);
    void AddCircleFilled(Vec2 center, float radius, const Color& color, int segments = 32);
    void AddText(float x, float y, const std::string& text, const Color& color, float scale = 1.f);

    // Access UI
    class Ui;
    Ui& GetUI() { return *ui; }

    // Window configuration
    void SetHWND(HWND h) { hwnd = h; }
    void SetWindowSize(int w, int h) { windowWidth = w; windowHeight = h; }
    HWND GetHwnd() { return hwnd; }
    POINT GetWindowSize() { return { windowWidth, windowHeight }; };

private:

    // helpers
    void InitPipeline();
    void FlushBatch();
    void EnsureBufferSize(size_t count);
    bool LoadFontMap(const std::string& path);
    void DrawChar(float x, float y, float w, float h, float u0, float v0, float u1, float v1, float r, float g, float b, float a);
private:

    // Core D3D resources
    HWND hwnd = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;
    ID3D11Buffer* gpuVertexBuffer = nullptr;
    size_t gpuBufferCapacity = 0;
    ID3D11InputLayout* inputLayout = nullptr;
    ID3D11VertexShader* vertexShader = nullptr;
    ID3D11PixelShader* pixelShader = nullptr;
    std::vector<Vertex> vertexBufferData;
    size_t vertexCount = 0;
    ID3D11Buffer* fontVertexBuffer = nullptr;
    ID3D11InputLayout* fontInputLayout = nullptr;
    ID3D11VertexShader* fontVertexShader = nullptr;
    ID3D11PixelShader* fontPixelShader = nullptr;
    ID3D11ShaderResourceView* fontTextureView = nullptr;
    ID3D11BlendState* alphaBlendState = nullptr;
    ID3D11SamplerState* fontSampler = nullptr;

    // font map
    std::unordered_map<int, FontChar> fontMap;
    int textureWidth = 254, textureHeight = 376;

    // window state
    int windowWidth = 1200, windowHeight = 720;

    std::unique_ptr<Ui> ui;
};

class Renderer::Ui {
public:
    struct Context {
        POINT mousePos{};
        bool mouseDown = false;     // currently held
        bool mousePrevDown = false; // last frame
        bool mouseClicked = false;  // pressed this frame
        bool mouseReleased = false; // released this frame

        // text typed this frame
        std::string textInput; 

        bool keyBackspace = false;
        bool keyEnter = false;
        bool keyLeft = false;
        bool keyRight = false;

        float deltaTime = 0.f;

    };
    
    // base for all components
    struct Component {
        Component() = default;
        virtual ~Component() = default;
        virtual void Update(const Context& ctx, float offsetX, float offsetY) = 0;
        virtual void Draw(Renderer* renderer, float offsetX, float offsetY) = 0;
    };

    // Window
    struct Window {
        std::string title;
        float x, y, w, h;           // position/size on screen
        bool visible = true;        // is it visible
        bool dragging = false;      // is it being dragged
        bool isDraggable = true;    // should it be dragged
        float offset = 0.f;         // y value - where to place next component

        std::vector<std::unique_ptr<Component>> components;
    };

    // Button
    struct Button : public Component {
        std::string label;
        float x, y, w, h;
        bool hovered = false;
        std::function<void()> onClick;

        void Update(const Context& ctx, float offsetX, float offsetY) override {
            hovered = ctx.mousePos.x >= x + offsetX && ctx.mousePos.x <= x + offsetX + w &&
                ctx.mousePos.y >= y + offsetY && ctx.mousePos.y <= y + offsetY + h;
            if (hovered && ctx.mouseClicked && onClick) onClick();
        }

        void Draw(Renderer* renderer, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;
            renderer->AddRectangleFilled({ drawX, drawY }, { w, h }, hovered ? UserInterfaceColors::ButtonHover : UserInterfaceColors::ButtonNormal);
            renderer->AddRectangle({ drawX, drawY }, { w, h }, Color(1, 1, 1, 1.f));
            renderer->AddText(drawX + 5, drawY + 5, label, Color(1, 1, 1, 1));
        }
    };

    // Checkbox
    struct Checkbox : public Component {
        std::string label;
        float x, y, size;
        bool checked = false;
        bool hovered = false;
        std::function<void(bool)> onToggle;

        void Update(const Context& ctx, float offsetX, float offsetY) override {
            hovered = ctx.mousePos.x >= x + offsetX && ctx.mousePos.x <= x + offsetX + size &&
                ctx.mousePos.y >= y + offsetY && ctx.mousePos.y <= y + offsetY + size;

            if (hovered && ctx.mouseClicked) {
                checked = !checked;
                if (onToggle) onToggle(checked);
            }
        }

        void Draw(Renderer* renderer, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;
            renderer->AddRectangle({ drawX, drawY }, { size, size }, Color(1, 1, 1, 1));
            if (checked) renderer->AddRectangleFilled({ drawX + 2, drawY + 2 }, { size - 5, size - 5 }, Color(1, 1, 1, 1));
            renderer->AddText(drawX + size + 5, drawY, label, Color(1, 1, 1, 1));
        }
    };

    // Slider
    struct Slider : public Component {
        Slider() = default;
        std::string label;
        float* value;              
        float x = 0.f, y = 0.f;
        float width = UserInterfaceStyles::SliderWidth;
        float height = UserInterfaceStyles::SliderHeight;
        bool hovered = false;
        bool dragging = false;
        std::function<void(float)> onSlide;

        void Update(const Context& ctx, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;

            hovered = ctx.mousePos.x >= drawX && ctx.mousePos.x <= drawX + width &&
                ctx.mousePos.y >= drawY && ctx.mousePos.y <= drawY + height;

            if (hovered && ctx.mouseClicked) {
                dragging = true;
            }
            if (!ctx.mouseDown) {
                dragging = false;
            }

            if (dragging) {
                float newValue = (ctx.mousePos.x - drawX) / width;
                if (newValue < 0.f) newValue = 0.f;
                if (newValue > 1.f) newValue = 1.f;

                if (value) *value = newValue;
            }
        }

        void Draw(Renderer* renderer, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;

            // Draw slider bar
            float barHeight = height / 3.f;
            renderer->AddRectangleFilled({ drawX, drawY + (height - barHeight) / 2.f }, { width, barHeight }, UserInterfaceColors::SliderBar);

            // knob
            float normalizedValue = *value ? *value : 0.f;
            float knobWidth = UserInterfaceStyles::SliderKnobWidth;
            float knobHeight = height;
            float knobX = drawX + normalizedValue * width - knobWidth / 2.f;
            float knobY = drawY;
            renderer->AddRectangleFilled({ knobX, knobY }, { knobWidth, knobHeight }, hovered || dragging ? UserInterfaceColors::SliderHover : UserInterfaceColors::SliderKnob);

            // Draw label
            // renderer->AddText(drawX, drawY - 15.f, label, UserInterfaceColors::TextColor);
        }
    };

    // Text
    struct Text : public Component {
        Text() = default;
        std::string label;
        float x, y;
        Color color;

        void Update(const Context& ctx, float offsetX, float offsetY) override {
        }

        void Draw(Renderer* renderer, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;

            renderer->AddText(drawX, drawY, label, UserInterfaceColors::TextColor);
        }
    };

    // Text Input
    struct TextInput : public Component {
        std::string label;
        std::string* value;
        float x, y;
        bool hovered;
        bool focused;
        float blinkTimer = 0.f;
        int caretPos = 0;
        bool caretVisible = true;

        void Update(const Context& ctx, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;

            hovered = ctx.mousePos.x >= drawX && ctx.mousePos.x <= drawX + UserInterfaceStyles::TextInputWidth &&
                ctx.mousePos.y >= drawY && ctx.mousePos.y <= drawY + UserInterfaceStyles::ControlHeight;

            if (hovered && ctx.mouseClicked) {
                focused = true;
            }
            else if (!hovered && ctx.mouseClicked) {
                focused = false; // if click outside then unfocus
            }

            if (focused) {
                // input
                for (auto c : ctx.textInput) {  
                    value->insert(value->begin() + caretPos, c);
                    caretPos++;
                }

                // backspace
                if (ctx.keyBackspace && caretPos > 0) {
                    value->erase(value->begin() + caretPos - 1);
                    caretPos--;
                }

                blinkTimer += ctx.deltaTime;
                if (blinkTimer > 0.5f) {
                    caretVisible = !caretVisible;
                    blinkTimer = 0.f;
                }
            }

        }

        void Draw(Renderer* renderer, float offsetX, float offsetY) override {
            float drawX = x + offsetX;
            float drawY = y + offsetY;

            // background box
            renderer->AddRectangleFilled({ drawX, drawY }, { UserInterfaceStyles::TextInputWidth, UserInterfaceStyles::TextInputHeight },
                focused ? UserInterfaceColors::ButtonHover : UserInterfaceColors::ButtonNormal);

            // text inside input
            renderer->AddText(drawX + UserInterfaceStyles::BasePadding, drawY + UserInterfaceStyles::TextInputTextOffset, *value, UserInterfaceColors::TextColor);

            // caret draw
            if (focused && caretVisible) {
                // measure text width to caret using your text measurement
                // float caretX = drawX + UserInterfaceStyles::BasePadding + renderer->MeasureText(*value.substr(0, caretPos)).x;
                // renderer->AddRectangleFilled({ caretX, drawY + 4.f }, { 1.f, height - 8.f }, UserInterfaceColors::TextColor);
            }
        }
    };


    Ui(Renderer* renderer);
    Window* BeginWindow(const std::string& title, float x, float y, float w, float h);
    void EndWindow() { currentWindow = nullptr; }
    void End();
    void AddSlider(const std::string& label, float* value, std::function<void(float)> onChange = nullptr);
    void AddButton(const std::string& label, std::function<void()> onClick = nullptr);
    void AddText(const std::string& label, Color color);
    void AddTextInput(const std::string& label, std::string* buffer);
    void AddCheckbox(const std::string& label, bool* value = nullptr, std::function<void(bool)> onToggle = nullptr);
    Context* GetContext() { return &context; }
    void UpdateMouseAndKey(HWND hwnd);
    void ResizeCurrentWindow(int x, int y) { if (currentWindow) { currentWindow->w = x; currentWindow->h = y; } }
    Window* GetCurrentWindow() { return currentWindow; } 
    Renderer* GetRenderer() { return renderer; };

private:
    Renderer* renderer;
    Context context;
    std::vector<std::unique_ptr<Window>> windows;
    Window* currentWindow = nullptr;                // current window being built
    Window* m_draggedWindow = nullptr;              // window being dragged
    Component* m_focusedComponent = nullptr;        // component focused
    Vec2 dragOffset;                                // drag distance
};