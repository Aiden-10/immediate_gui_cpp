#pragma once
#include <d3d11.h>
#include <DirectXMath.h>

struct Color {
    Color() = default;
    float r, g, b, a;
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
};

struct Vec2 {
    float x, y;

    // Addition
    Vec2 operator+(const Vec2& other) const {
        return { x + other.x, y + other.y };
    }

    // Subtraction
    Vec2 operator-(const Vec2& other) const {
        return { x - other.x, y - other.y };
    }

    // Equality
    bool operator==(const Vec2& other) const {
        return x == other.x && y == other.y;
    }

    // Inequality
    bool operator!=(const Vec2& other) const {
        return !(*this == other);
    }
};

struct Vec3 {
    float x, y, z;

    // Addition
    Vec3 operator+(const Vec3& other) const {
        return { x + other.x, y + other.y, z + other.z };
    }

    // Subtraction
    Vec3 operator-(const Vec3& other) const {
        return { x - other.x, y - other.y, z - other.z };
    }

    // Equality
    bool operator==(const Vec3& other) const {
        return x == other.x && y == other.y && z == other.z;
    }

    // Inequality
    bool operator!=(const Vec3& other) const {
        return !(*this == other);
    }
};

struct Vertex {
    float x, y, z;        // 12 bytes
    float r, g, b, a;     // 16 bytes  (offset 12)
    float u, v;           // 8 bytes   (offset 28)
    float texIndex;       // 4 bytes   (offset 36)
};

struct FontChar {
    int id;
    int x, y;       // Position in texture
    int w, h;       // Size of the character glyph
    int xoffset;    // How far to move horizontally when rendering
    int yoffset;    // Vertical offset for alignment
    int xadvance;   // How far to move to the next character

    float u0, v0;   // Top-left UV coordinate
    float u1, v1;   // Bottom-right UV coordinate
};