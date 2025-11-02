#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <windows.h>
#include "Window.h"
#include "Renderer/Renderer.h"


int main()
{
    Window window("GUI Test", { 1080, 720 });
    Renderer* renderer = window.getRenderer();
    Renderer::Ui& ui = renderer->GetUI();

    bool checkboxValue = false;
    float sliderValue1 = 0.1f;
    float sliderValue2 = 0.1f;
    float sliderValue3 = 0.1f;
    std::string buffer = "";

    struct Shape {
        float x, y;
        float r, g, b;
        float size;
    };

    std::vector<Shape> shapes;

    while (window.isRun())
    {
        if (!window.broadcast()) break;

        // Begin Frame
        window.onUpdate();
        renderer->Begin();

        // --- Render Here ---

        // Begin Menu
        ui.BeginWindow("Settings", 100, 100, 200, 300);
        ui.AddText("Adjust Settings", Color(1, 1, 1, 1));
        ui.AddText("Red", Color(1, 1, 1));
        ui.AddSlider("Slider 1", &sliderValue1);
        ui.AddText("Green", Color(1, 1, 1));
        ui.AddSlider("Slider 2", &sliderValue2);
        ui.AddText("Blue", Color(1, 1, 1));
        ui.AddSlider("Slider 3", &sliderValue3);
        ui.AddButton("Spawn Square", [&]() {
            Shape s;
            s.x = 50.f + shapes.size() * 50.f; 
            s.y = 100.f;
            s.r = sliderValue1;
            s.g = sliderValue2;
            s.b = sliderValue3;
            s.size = 30.f;
            shapes.push_back(s);
        });
        ui.AddCheckbox("Show Fps!", &checkboxValue);
        if (checkboxValue) window.displayFPS();
        // (Work In Progress) ui.AddTextInput("Name", &userName);
        ui.EndWindow();



        // Render Shapes
        for (auto& s : shapes) {
            renderer->AddRectangleFilled({ s.x, s.y }, { s.size, s.size }, { s.r, s.g, s.b, 1.f });
        }


        // End Frame
        renderer->End();
        window.present();
    }

    return 0;
}