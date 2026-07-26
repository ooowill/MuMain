#include "stdafx.h"

#ifdef _EDITOR

#include "MapEditorSession.h"

#include "Core/MuEditorCore.h"
#include "Engine/Object/ZzzObject.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Terrain/ZzzLodTerrain.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "World/MapInfra/MapManager.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

extern bool SelectFlag;

namespace
{
    constexpr int CharacterWorld = WD_74NEW_CHARACTER_SCENE;
    constexpr int CharacterMapFileNumber = 75;
    constexpr float MinimumObjectScale = 0.01f;
    constexpr float DuplicateOffset = 100.0f;

    constexpr int EditableModelTypes[] = {
        0, 2, 3, 4, 5, 6, 7, 8,
        11, 12, 13, 14, 15, 16, 17,
        24, 25, 26, 30, 31, 33, 34, 36, 38, 39, 40, 41, 42, 43, 44,
        54, 55, 56, 57, 60, 61, 69, 70, 71, 73, 75, 80, 81,
    };

    std::array<float, 3> ToArray(const vec3_t value)
    {
        return { value[0], value[1], value[2] };
    }

    void CopyArray(const std::array<float, 3>& source, vec3_t destination)
    {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
    }

    std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
            return {};

        const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (size <= 1)
            return {};

        std::string result(static_cast<std::size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    std::wstring BuildBackupName()
    {
        SYSTEMTIME localTime{};
        GetLocalTime(&localTime);

        wchar_t name[64]{};
        swprintf_s(
            name,
            L"EncTerrain75_%04u%02u%02u_%02u%02u%02u.obj",
            localTime.wYear,
            localTime.wMonth,
            localTime.wDay,
            localTime.wHour,
            localTime.wMinute,
            localTime.wSecond);
        return name;
    }
}

CMapEditorSession& CMapEditorSession::GetInstance()
{
    static CMapEditorSession instance;
    return instance;
}

bool CMapEditorSession::IsAvailable() const
{
    return gMapManager.WorldActive == CharacterWorld;
}

bool CMapEditorSession::IsActive() const
{
    return IsAvailable()
        && g_MuEditorCore.IsEnabled()
        && g_MuEditorCore.IsShowingMapEditor();
}

void CMapEditorSession::Update()
{
    if (!IsActive())
        return;

    if (m_SelectedId != 0 && GetSelectedObjectPtr() == nullptr)
        m_SelectedId = 0;

    if (g_MuEditorCore.IsHoveringUI() || !MouseLButtonPush)
        return;

    OBJECT* object = CollisionDetectObjects(nullptr);
    m_SelectedId = reinterpret_cast<std::uintptr_t>(object);
    if (object != nullptr)
    {
        std::ostringstream status;
        status << "Selecionado: " << GetModelName(object->Type) << " (tipo " << object->Type << ").";
        SetStatus(status.str());
    }
    else
    {
        SetStatus("Nenhum objeto encontrado nesse ponto.");
    }

    MouseLButton = false;
    MouseLButtonPop = false;
    MouseLButtonPush = false;
    MouseLButtonDBClick = false;
}

void CMapEditorSession::RenderSelection() const
{
    if (!IsActive())
        return;

    const OBJECT* object = GetSelectedObjectPtr();
    if (object == nullptr)
        return;

    const float sizeX = std::abs(object->BoundingBoxMax[0] - object->BoundingBoxMin[0]);
    const float sizeY = std::abs(object->BoundingBoxMax[1] - object->BoundingBoxMin[1]);
    const float sizeZ = std::abs(object->BoundingBoxMax[2] - object->BoundingBoxMin[2]);
    const float radius = std::max(60.0f, std::max({ sizeX, sizeY, sizeZ }) * object->Scale * 0.55f);
    RenderDebugSphere(object->Position, radius, 1.0f, 0.82f, 0.05f);
}

std::vector<MapEditorObjectInfo> CMapEditorSession::GetObjects() const
{
    std::vector<MapEditorObjectInfo> objects;
    if (!IsAvailable())
        return objects;

    for (OBJECT_BLOCK& block : ObjectBlock)
    {
        for (OBJECT* object = block.Head; object != nullptr; object = object->Next)
        {
            if (!object->Live)
                continue;

            MapEditorObjectInfo info;
            info.Id = reinterpret_cast<std::uintptr_t>(object);
            info.Type = object->Type;
            info.Name = GetModelName(object->Type);
            info.Position = ToArray(object->Position);
            info.Angle = ToArray(object->Angle);
            info.Scale = object->Scale;
            objects.push_back(std::move(info));
        }
    }

    return objects;
}

bool CMapEditorSession::GetSelectedObject(MapEditorObjectInfo& info) const
{
    OBJECT* object = GetSelectedObjectPtr();
    if (object == nullptr)
        return false;

    info.Id = reinterpret_cast<std::uintptr_t>(object);
    info.Type = object->Type;
    info.Name = GetModelName(object->Type);
    info.Position = ToArray(object->Position);
    info.Angle = ToArray(object->Angle);
    info.Scale = object->Scale;
    return true;
}

bool CMapEditorSession::SelectObject(std::uintptr_t id)
{
    OBJECT* object = FindObject(id);
    if (object == nullptr)
        return false;

    m_SelectedId = id;
    SetStatus("Objeto selecionado pela lista.");
    return true;
}

void CMapEditorSession::ClearSelection()
{
    m_SelectedId = 0;
}

bool CMapEditorSession::SetSelectedTransform(const std::array<float, 3>& position,
                                             const std::array<float, 3>& angle,
                                             float scale)
{
    OBJECT* object = GetSelectedObjectPtr();
    if (object == nullptr)
        return false;

    if (!RelinkObject(object, position))
    {
        SetStatus("Posicao recusada: o objeto precisa permanecer dentro do mapa.");
        return false;
    }

    CopyArray(position, object->Position);
    CopyArray(position, object->StartPosition);
    CopyArray(angle, object->Angle);
    CopyArray(angle, object->Direction);
    object->Scale = std::max(scale, MinimumObjectScale);
    SetStatus("Transformacao aplicada ao objeto selecionado.");
    return true;
}

bool CMapEditorSession::DuplicateSelected()
{
    OBJECT* source = GetSelectedObjectPtr();
    if (source == nullptr)
        return false;

    vec3_t position;
    vec3_t angle;
    VectorCopy(source->Position, position);
    VectorCopy(source->Angle, angle);
    position[0] += DuplicateOffset;

    OBJECT* duplicate = CreateObject(source->Type, position, angle, source->Scale);
    if (duplicate == nullptr)
    {
        SetStatus("Nao foi possivel duplicar o objeto nessa posicao.");
        return false;
    }

    m_SelectedId = reinterpret_cast<std::uintptr_t>(duplicate);
    SetStatus("Objeto duplicado e selecionado.");
    return true;
}

bool CMapEditorSession::DeleteSelected()
{
    OBJECT* object = GetSelectedObjectPtr();
    if (object == nullptr)
        return false;

    DeleteObject(object, &ObjectBlock[object->Block]);
    m_SelectedId = 0;
    SetStatus("Objeto removido da copia em memoria.");
    return true;
}

bool CMapEditorSession::CreateAtCursor(int type)
{
    if (!IsAvailable() || !IsEditableModelType(type) || !SelectFlag)
    {
        SetStatus("Aponte o cursor para o terreno e escolha um modelo carregado.");
        return false;
    }

    vec3_t angle = { 0.0f, 0.0f, 0.0f };
    OBJECT* object = CreateObject(type, CollisionPosition, angle, 1.0f);
    if (object == nullptr)
    {
        SetStatus("Nao foi possivel criar o objeto nesse ponto.");
        return false;
    }

    m_SelectedId = reinterpret_cast<std::uintptr_t>(object);
    SetStatus("Novo objeto adicionado no cursor.");
    return true;
}

std::string CMapEditorSession::GetModelName(int type) const
{
    if (type >= 0 && type < MAX_WORLD_OBJECTS)
    {
        const BMD& model = Models[MODEL_WORLD_OBJECT + type];
        std::size_t length = 0;
        while (length < sizeof(model.Name) && model.Name[length] != '\0')
            ++length;

        if (length > 0)
        {
            const std::filesystem::path modelPath(std::string(model.Name, length));
            const std::string name = modelPath.filename().string();
            if (!name.empty())
                return name;
        }
    }

    char name[32]{};
    snprintf(name, sizeof(name), "Object%02d", type + 1);
    return name;
}

int CMapEditorSession::GetEditableModelCount() const
{
    return static_cast<int>(std::size(EditableModelTypes));
}

int CMapEditorSession::GetEditableModelType(int index) const
{
    if (index < 0 || index >= GetEditableModelCount())
        return -1;
    return EditableModelTypes[index];
}

bool CMapEditorSession::SaveDraft()
{
    if (!IsAvailable())
        return false;

    try
    {
        const std::filesystem::path output = std::filesystem::path(GetOutputDirectoryWide()) / L"Draft";
        std::filesystem::create_directories(output);

        if (!SaveObjectsTo((output / L"EncTerrain75.obj").wstring()))
            return false;

        std::wstring mapping = (output / L"EncTerrain75.map").wstring();
        std::wstring height = (output / L"TerrainHeight.bmp").wstring();
        std::wstring light = (output / L"TerrainLight.jpg").wstring();

        SaveTerrainMapping(mapping.data(), CharacterMapFileNumber);
        SaveTerrainHeight(height.data());
        SaveTerrainLight(light.data());
        if (!SaveTerrainAttributeFile((output / L"EncTerrain75.att").wstring()))
            return false;

        SetStatus("Rascunho completo salvo em " + WideToUtf8(output.wstring()));
        return true;
    }
    catch (const std::exception& error)
    {
        SetStatus(std::string("Falha ao salvar rascunho: ") + error.what());
        return false;
    }
}

bool CMapEditorSession::SaveObjectMap()
{
    if (!IsAvailable())
        return false;

    try
    {
        const std::filesystem::path outputDirectory(GetOutputDirectoryWide());
        const std::filesystem::path workingDirectory = outputDirectory / L"Working";
        const std::filesystem::path backupDirectory = outputDirectory / L"Backups";
        const std::filesystem::path target = workingDirectory / L"EncTerrain75.obj";
        const std::filesystem::path temporary = outputDirectory / L"EncTerrain75.pending.obj";

        std::filesystem::create_directories(workingDirectory);
        std::filesystem::create_directories(backupDirectory);

        if (!SaveObjectsTo(temporary.wstring()))
            return false;

        if (std::filesystem::exists(target))
        {
            std::filesystem::copy_file(
                target,
                backupDirectory / BuildBackupName(),
                std::filesystem::copy_options::overwrite_existing);
        }

        std::filesystem::copy_file(temporary, target, std::filesystem::copy_options::overwrite_existing);
        std::filesystem::remove(temporary);
        SetStatus("EncTerrain75.obj salvo somente na copia de trabalho.");
        return true;
    }
    catch (const std::exception& error)
    {
        SetStatus(std::string("Falha ao salvar a copia: ") + error.what());
        return false;
    }
}

bool CMapEditorSession::OpenOutputFolder()
{
    try
    {
        const std::filesystem::path output(GetOutputDirectoryWide());
        std::filesystem::create_directories(output);
        const HINSTANCE result = ShellExecuteW(nullptr, L"open", output.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<std::intptr_t>(result) <= 32)
        {
            SetStatus("O Windows nao conseguiu abrir a pasta de trabalho.");
            return false;
        }
        return true;
    }
    catch (const std::exception& error)
    {
        SetStatus(std::string("Falha ao abrir a pasta: ") + error.what());
        return false;
    }
}

std::string CMapEditorSession::GetOutputDirectory() const
{
    return WideToUtf8(GetOutputDirectoryWide());
}

OBJECT* CMapEditorSession::FindObject(std::uintptr_t id) const
{
    if (id == 0)
        return nullptr;

    for (OBJECT_BLOCK& block : ObjectBlock)
    {
        for (OBJECT* object = block.Head; object != nullptr; object = object->Next)
        {
            if (object->Live && reinterpret_cast<std::uintptr_t>(object) == id)
                return object;
        }
    }
    return nullptr;
}

OBJECT* CMapEditorSession::GetSelectedObjectPtr() const
{
    return FindObject(m_SelectedId);
}

bool CMapEditorSession::RelinkObject(OBJECT* object, const std::array<float, 3>& position)
{
    constexpr float BlockWorldSize = 16.0f * TERRAIN_SCALE;
    constexpr int BlockCount = 16;
    const int blockX = static_cast<int>(position[0] / BlockWorldSize);
    const int blockY = static_cast<int>(position[1] / BlockWorldSize);
    if (blockX < 0 || blockX >= BlockCount || blockY < 0 || blockY >= BlockCount)
        return false;

    const BYTE newBlockIndex = static_cast<BYTE>(blockX * BlockCount + blockY);
    if (newBlockIndex == object->Block)
        return true;

    OBJECT_BLOCK& oldBlock = ObjectBlock[object->Block];
    if (object->Prior != nullptr)
        object->Prior->Next = object->Next;
    else
        oldBlock.Head = object->Next;

    if (object->Next != nullptr)
        object->Next->Prior = object->Prior;
    else
        oldBlock.Tail = object->Prior;

    OBJECT_BLOCK& newBlock = ObjectBlock[newBlockIndex];
    object->Prior = newBlock.Tail;
    object->Next = nullptr;
    if (newBlock.Tail != nullptr)
        newBlock.Tail->Next = object;
    else
        newBlock.Head = object;
    newBlock.Tail = object;
    object->Block = newBlockIndex;
    return true;
}

bool CMapEditorSession::IsEditableModelType(int type) const
{
    return std::find(std::begin(EditableModelTypes), std::end(EditableModelTypes), type)
        != std::end(EditableModelTypes);
}

bool CMapEditorSession::SaveObjectsTo(const std::wstring& path) const
{
    std::wstring mutablePath = path;
    const bool saved = SaveObjects(mutablePath.data(), CharacterMapFileNumber);
    if (!saved)
        const_cast<CMapEditorSession*>(this)->SetStatus("Falha ao serializar os objetos do World75.");
    return saved;
}

bool CMapEditorSession::SaveTerrainAttributeFile(const std::wstring& path) const
{
    constexpr std::size_t HeaderSize = 4;
    constexpr std::size_t TerrainCellCount = TERRAIN_SIZE * TERRAIN_SIZE;
    std::vector<BYTE> plain(HeaderSize + TerrainCellCount * sizeof(WORD));
    plain[0] = 0;
    plain[1] = static_cast<BYTE>(CharacterMapFileNumber);
    plain[2] = 0xFF;
    plain[3] = 0xFF;
    std::memcpy(plain.data() + HeaderSize, TerrainWall, TerrainCellCount * sizeof(WORD));

    std::vector<BYTE> encrypted(plain.size());
    MapFileEncrypt(encrypted.data(), plain.data(), static_cast<int>(plain.size()));

    std::ofstream file(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!file)
    {
        const_cast<CMapEditorSession*>(this)->SetStatus("Falha ao criar EncTerrain75.att no rascunho.");
        return false;
    }
    file.write(reinterpret_cast<const char*>(encrypted.data()), static_cast<std::streamsize>(encrypted.size()));
    return file.good();
}

std::wstring CMapEditorSession::GetExecutableDirectory() const
{
    std::wstring path(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    while (length == path.size())
    {
        path.resize(path.size() * 2);
        length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    }
    path.resize(length);
    return std::filesystem::path(path).parent_path().wstring();
}

std::wstring CMapEditorSession::GetOutputDirectoryWide() const
{
    constexpr DWORD EnvironmentPathCapacity = 32768;
    std::wstring configured(EnvironmentPathCapacity, L'\0');
    const DWORD length = GetEnvironmentVariableW(
        L"MU_MAP_EDITOR_OUTPUT",
        configured.data(),
        static_cast<DWORD>(configured.size()));
    if (length > 0 && length < configured.size())
    {
        configured.resize(length);
        return configured;
    }

    return (std::filesystem::path(GetProjectDirectory()) / L"MapEditorOutput" / L"World75").wstring();
}

std::wstring CMapEditorSession::GetProjectDirectory() const
{
    std::filesystem::path directory(GetExecutableDirectory());
    while (!directory.empty())
    {
        if (std::filesystem::exists(directory / L"CMakeLists.txt")
            && std::filesystem::exists(directory / L"src" / L"bin" / L"Data" / L"World75"))
        {
            return directory.wstring();
        }

        const std::filesystem::path parent = directory.parent_path();
        if (parent == directory)
            break;
        directory = parent;
    }

    return GetExecutableDirectory();
}

void CMapEditorSession::SetStatus(const std::string& status)
{
    m_Status = status;
}

#endif // _EDITOR
