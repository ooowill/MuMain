#pragma once

namespace AccountCharacterPaging
{
    constexpr int SlotsPerPage = 5;

    int GetCurrentPage();
    int GetPageCount();
    int GetFirstSlotOnCurrentPage();
    int GetVisibleSlot(int visibleIndex);
    bool IsSlotOnCurrentPage(int slot);
    bool CanGoPrevious();
    bool CanGoNext();

    void ClampCurrentPage();
    void ResetToFirstPageWithoutRefresh();
    bool SetPage(int page);
    bool SetPageForSlot(int slot);
    bool PreviousPage();
    bool NextPage();
    void RefreshVisibleCharacters();
    void ClearVisibleCharacters();
}
