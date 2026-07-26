#pragma once

#ifdef _EDITOR

#include <array>
#include <cstdint>

class CMuMapEditorUI
{
public:
    static CMuMapEditorUI& GetInstance();
    void Render(bool* open);

private:
    CMuMapEditorUI() = default;

    void RefreshSelectedObject();
    bool ApplyTransform();

    char m_Filter[96]{};
    std::uintptr_t m_LoadedSelection = 0;
    std::array<float, 3> m_Position{};
    std::array<float, 3> m_Angle{};
    float m_Scale = 1.0f;
    int m_NewObjectType = 0;
};

#define g_MuMapEditorUI CMuMapEditorUI::GetInstance()

#endif // _EDITOR
