#pragma once

#ifdef _EDITOR

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class OBJECT;

struct MapEditorObjectInfo
{
    std::uintptr_t Id = 0;
    int Type = 0;
    std::string Name;
    std::array<float, 3> Position{};
    std::array<float, 3> Angle{};
    float Scale = 1.0f;
};

class CMapEditorSession
{
public:
    static CMapEditorSession& GetInstance();

    bool IsAvailable() const;
    bool IsActive() const;
    void Update();
    void RenderSelection() const;

    std::vector<MapEditorObjectInfo> GetObjects() const;
    bool GetSelectedObject(MapEditorObjectInfo& object) const;
    bool SelectObject(std::uintptr_t id);
    void ClearSelection();

    bool SetSelectedTransform(const std::array<float, 3>& position,
                              const std::array<float, 3>& angle,
                              float scale);
    bool DuplicateSelected();
    bool DeleteSelected();
    bool CreateAtCursor(int type);

    std::string GetModelName(int type) const;
    int GetEditableModelCount() const;
    int GetEditableModelType(int index) const;

    bool SaveDraft();
    bool SaveObjectMap();
    bool OpenOutputFolder();

    const std::string& GetStatus() const { return m_Status; }
    std::string GetOutputDirectory() const;

private:
    CMapEditorSession() = default;

    OBJECT* FindObject(std::uintptr_t id) const;
    OBJECT* GetSelectedObjectPtr() const;
    bool RelinkObject(OBJECT* object, const std::array<float, 3>& position);
    bool IsEditableModelType(int type) const;
    bool SaveObjectsTo(const std::wstring& path) const;
    bool SaveTerrainAttributeFile(const std::wstring& path) const;
    std::wstring GetExecutableDirectory() const;
    std::wstring GetProjectDirectory() const;
    std::wstring GetOutputDirectoryWide() const;
    void SetStatus(const std::string& status);

    std::uintptr_t m_SelectedId = 0;
    std::string m_Status = "World75 pronta para edicao segura.";
};

#define g_MapEditorSession CMapEditorSession::GetInstance()

#endif // _EDITOR
