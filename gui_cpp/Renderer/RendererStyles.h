#pragma once
#include "RendererPrimitives.h"

namespace UserInterfaceColors {
    inline Color WindowBackground = Color(0.09f, 0.09f, 0.10f, 1.f);  
    inline Color WindowTitlebar = Color(0.12f, 0.12f, 0.14f, 1.f);
    inline Color WindowBorder = Color(0.20f, 0.20f, 0.22f, 1.f);

    inline Color TextColor = Color(0.90f, 0.90f, 0.92f, 1.f);  
    inline Color TitleTextColor = Color(1.00f, 1.00f, 1.00f, 1.f);

    inline Color ButtonNormal = Color(0.18f, 0.18f, 0.20f, 1.f);
    inline Color ButtonHover = Color(0.22f, 0.22f, 0.25f, 1.f);

    inline Color SliderBar = Color(0.25f, 0.25f, 0.28f, 1.f);
    inline Color SliderKnob = Color(0.45f, 0.45f, 0.50f, 1.f);
    inline Color SliderHover = Color(0.32f, 0.52f, 0.90f, 1.f);  

    inline Color CheckboxFill = Color(0.32f, 0.52f, 0.90f, 1.f);   
}

namespace UserInterfaceStyles {
    // helpers
    inline float BasePadding = 6.f;          
    inline float ControlHeight = 30.f;

    // Buttons
    inline float ButtonHeight = ControlHeight;
    inline float ButtonMinWidth = 120.f;
    inline float ButtonPadding = 12.f;

    // Checkbox
    inline float CheckboxSize = ControlHeight - (BasePadding * 2.f);
    inline float CheckboxPadding = 10.f;

    // Slider
    inline float SliderHeight = 20.f;
    inline float SliderWidth = 150.f;
    inline float SliderKnobWidth = ControlHeight * 0.25f;
    inline float SliderKnobHeight = ControlHeight * 0.5f;
    inline float SliderPadding = 12.f;

    // Window general spacing
    inline float WindowTitleHeight = ControlHeight;
    inline float WindowPadding = BasePadding;

    // Text
    inline float TextPadding = 8.f;

    // TextInput
    inline float TextInputPadding = 10.f;
    inline float TextInputWidth = 100.f;
    inline float TextInputHeight = 30.f;
    inline float TextInputTextOffset = (TextInputHeight / 2.f - 8.f);
}