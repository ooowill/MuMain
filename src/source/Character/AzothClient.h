#pragma once

namespace AzothClient
{
    constexpr unsigned long long MaxDisplayBalance = 10000000000ULL;
    constexpr unsigned long long MaxAutoConversionReserveZen = 2000000000ULL;

    unsigned long long GetBalance();
    void SetBalance(unsigned long long balance);
    bool IsAutoConversionEnabled();
    unsigned long long GetAutoConversionReserveZen();
    unsigned int GetAutoConversionSettingsVersion();
    void SetAutoConversionSettings(bool enabled, unsigned long long reserveZen);
    void Tick();
    bool HandleBalanceMessage(const wchar_t* senderName, const wchar_t* message);
    void RequestBalance();
    void ClearNpcShopPrices();
    void SetNpcShopPrice(int slot, unsigned long long price);
    bool GetNpcShopPrice(int slot, unsigned long long& price);
    void FormatAmount(unsigned long long amount, wchar_t* text, size_t textLength);
    void ConvertCurrent(long long zen);
    void ConvertCurrentMax();
    void ConvertAccountCharacters();
    void ConfigureAutoConversion(bool enabled, unsigned long long reserveZen);
    void ApplyAutoConversionSettings(bool enabled, unsigned long long reserveZen);
}
