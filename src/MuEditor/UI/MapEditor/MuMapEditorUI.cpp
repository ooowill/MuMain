#include "stdafx.h"

#ifdef _EDITOR

#include "MuMapEditorUI.h"

#include "Core/MuEditorCore.h"
#include "Map/MapEditorSession.h"
#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace
{
    constexpr float MapEditorWidth = 440.0f;
    constexpr float MapEditorHeight = 700.0f;
    constexpr float ObjectListHeight = 225.0f;

    bool ContainsCaseInsensitive(const std::string& value, const std::string& filter)
    {
        if (filter.empty())
            return true;

        return std::search(
            value.begin(), value.end(),
            filter.begin(), filter.end(),
            [](char left, char right)
            {
                return std::tolower(static_cast<unsigned char>(left))
                    == std::tolower(static_cast<unsigned char>(right));
            }) != value.end();
    }
}

CMuMapEditorUI& CMuMapEditorUI::GetInstance()
{
    static CMuMapEditorUI instance;
    return instance;
}

void CMuMapEditorUI::Render(bool* open)
{
    ImGui::SetNextWindowPos(ImVec2(10.0f, 50.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(MapEditorWidth, MapEditorHeight), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Editor de cenario - World75", open))
    {
        ImGui::End();
        return;
    }

    if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
        || ImGui::IsAnyItemHovered()
        || ImGui::IsAnyItemActive())
    {
        g_MuEditorCore.SetHoveringUI(true);
    }

    if (!g_MapEditorSession.IsAvailable())
    {
        ImGui::TextWrapped("O painel fica disponivel quando a selecao de personagens World75 esta carregada.");
        ImGui::End();
        return;
    }

    const std::vector<MapEditorObjectInfo> objects = g_MapEditorSession.GetObjects();
    ImGui::Text("World75 | %d objetos", static_cast<int>(objects.size()));
    ImGui::SameLine();
    if (ImGui::SmallButton("Limpar selecao"))
    {
        g_MapEditorSession.ClearSelection();
        m_LoadedSelection = 0;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##MapObjectFilter", "Filtrar por nome ou tipo", m_Filter, sizeof(m_Filter));

    if (ImGui::BeginChild("MapObjectList", ImVec2(0.0f, ObjectListHeight), true))
    {
        const std::string filter(m_Filter);
        for (const MapEditorObjectInfo& object : objects)
        {
            const std::string searchable = object.Name + " " + std::to_string(object.Type);
            if (!ContainsCaseInsensitive(searchable, filter))
                continue;

            char label[256]{};
            snprintf(
                label,
                sizeof(label),
                "%02d  %s  [%.0f, %.0f, %.0f]##%llu",
                object.Type,
                object.Name.c_str(),
                object.Position[0],
                object.Position[1],
                object.Position[2],
                static_cast<unsigned long long>(object.Id));

            const bool selected = object.Id == m_LoadedSelection;
            if (ImGui::Selectable(label, selected))
            {
                g_MapEditorSession.SelectObject(object.Id);
                m_LoadedSelection = 0;
                RefreshSelectedObject();
            }
        }
    }
    ImGui::EndChild();

    MapEditorObjectInfo selected;
    if (g_MapEditorSession.GetSelectedObject(selected))
    {
        if (selected.Id != m_LoadedSelection)
            RefreshSelectedObject();

        ImGui::SeparatorText("Objeto selecionado");
        ImGui::Text("%s | tipo %d", selected.Name.c_str(), selected.Type);

        ImGui::TextUnformatted("Posicao");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##SelectedPosition", m_Position.data(), 5.0f, 0.0f, 25600.0f, "%.1f"))
            ApplyTransform();

        ImGui::TextUnformatted("Rotacao");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat3("##SelectedRotation", m_Angle.data(), 1.0f, -360.0f, 360.0f, "%.1f"))
            ApplyTransform();

        ImGui::TextUnformatted("Escala");
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::DragFloat("##SelectedScale", &m_Scale, 0.01f, 0.01f, 200.0f, "%.3f"))
            ApplyTransform();

        if (ImGui::Button("Duplicar"))
        {
            if (g_MapEditorSession.DuplicateSelected())
            {
                m_LoadedSelection = 0;
                RefreshSelectedObject();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Excluir da copia"))
            ImGui::OpenPopup("Confirmar exclusao do World75");

        if (ImGui::BeginPopupModal("Confirmar exclusao do World75", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Excluir %s da cena em memoria?", selected.Name.c_str());
            if (ImGui::Button("Excluir objeto"))
            {
                g_MapEditorSession.DeleteSelected();
                m_LoadedSelection = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancelar"))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
    }
    else
    {
        m_LoadedSelection = 0;
        ImGui::SeparatorText("Objeto selecionado");
        ImGui::TextDisabled("Nenhum objeto selecionado");
    }

    ImGui::SeparatorText("Adicionar objeto carregado");
    const std::string currentModelName = g_MapEditorSession.GetModelName(m_NewObjectType);
    ImGui::SetNextItemWidth(255.0f);
    if (ImGui::BeginCombo("##NewMapObject", currentModelName.c_str()))
    {
        for (int index = 0; index < g_MapEditorSession.GetEditableModelCount(); ++index)
        {
            const int type = g_MapEditorSession.GetEditableModelType(index);
            const std::string modelName = g_MapEditorSession.GetModelName(type);
            const bool selectedModel = type == m_NewObjectType;
            const std::string label = std::to_string(type) + "  " + modelName;
            if (ImGui::Selectable(label.c_str(), selectedModel))
                m_NewObjectType = type;
            if (selectedModel)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Adicionar no cursor"))
    {
        if (g_MapEditorSession.CreateAtCursor(m_NewObjectType))
        {
            m_LoadedSelection = 0;
            RefreshSelectedObject();
        }
    }

    ImGui::SeparatorText("Salvar com seguranca");
    if (ImGui::Button("Salvar rascunho completo"))
        g_MapEditorSession.SaveDraft();
    ImGui::SameLine();
    if (ImGui::Button("Salvar objetos na copia"))
        g_MapEditorSession.SaveObjectMap();
    ImGui::SameLine();
    if (ImGui::SmallButton("Abrir pasta"))
        g_MapEditorSession.OpenOutputFolder();

    ImGui::TextWrapped("O jogo instalado nao e sobrescrito por este editor.");
    ImGui::TextWrapped("%s", g_MapEditorSession.GetStatus().c_str());
    ImGui::End();
}

void CMuMapEditorUI::RefreshSelectedObject()
{
    MapEditorObjectInfo selected;
    if (!g_MapEditorSession.GetSelectedObject(selected))
    {
        m_LoadedSelection = 0;
        return;
    }

    m_LoadedSelection = selected.Id;
    m_Position = selected.Position;
    m_Angle = selected.Angle;
    m_Scale = selected.Scale;
}

bool CMuMapEditorUI::ApplyTransform()
{
    if (g_MapEditorSession.SetSelectedTransform(m_Position, m_Angle, m_Scale))
        return true;

    RefreshSelectedObject();
    return false;
}

#endif // _EDITOR
