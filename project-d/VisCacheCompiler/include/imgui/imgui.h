#pragma once

struct ImVec2 {
    float x;
    float y;

    constexpr ImVec2() : x(0.0f), y(0.0f) {}
    constexpr ImVec2(float in_x, float in_y) : x(in_x), y(in_y) {}
};

struct ImVec4 {
    float x;
    float y;
    float z;
    float w;

    constexpr ImVec4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
    constexpr ImVec4(float in_x, float in_y, float in_z, float in_w)
        : x(in_x), y(in_y), z(in_z), w(in_w) {}
};
