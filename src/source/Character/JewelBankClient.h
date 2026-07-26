#pragma once

namespace JewelBankClient
{
    constexpr int JewelCount = 22;
    constexpr unsigned long long MaxDisplayCount = 10000000000ULL;

    unsigned long long GetCount(int index);
    bool IsAutoStoreEnabled();
    unsigned int GetStateVersion();
    void SetState(bool autoStore, const unsigned long long* counts, int count);
    void Tick();
    bool HandleBankMessage(const wchar_t* senderName, const wchar_t* message);
    void RequestSync();
    void StoreAll();
    void WithdrawAll();
    void Store(short jewelKey);
    void Withdraw(short jewelKey);
    void SetAutoStore(bool enabled);
}
