#pragma once

class BMD;
class OBJECT;

namespace LoginSceneEnvironment
{
    void LoadModels();
    void Release();
    void CreateObject(OBJECT* object);
    bool MoveObject(OBJECT* object);
    void RenderCharacterFountainEffects();
    void RenderCharacterSceneSkyEffects();
    bool ShouldRenderBeyondBlockCulling(int type);
    bool ShouldDeferUntilOpaqueObjectsRendered(int type);
    bool RenderObjectVisual(OBJECT* object, BMD* model);
    bool RenderObject(OBJECT* object, BMD* model);
}
