//*****************************************************************************
// File: LoginWin.cpp
//*****************************************************************************

#include "stdafx.h"
#include "UI/Windows/LoginWin.h"
#include "Core/Input/Input.h"
#include "UI/Legacy/UIMng.h"
#include "Render/Models/ZzzBMD.h"
#include "Render/Textures/ZzzOpenglUtil.h"
#include "Render/Textures/ZzzTexture.h"
#include "Engine/Object/ZzzInfomation.h"
#include "Engine/Object/ZzzObject.h"
#include "Engine/Object/ZzzCharacter.h"
#include "Engine/Object/ZzzInterface.h"
#include "Core/Utilities/Log/ErrorReport.h"
#include "Network/Reconnect/ReconnectManager.h"
#include "UI/Legacy/UIControls.h"
#include "Scenes/SceneCore.h"
#include "I18N/All.h"

#include "Audio/DSPlaySound.h"
#include "UI/NewUI/NewUISystem.h"

#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "Network/Server/ServerListManager.h"
#ifdef _WIN32
#include <dpapi.h>
#include <ws2tcpip.h>
#endif

#include "Data/GameConfig/GameConfig.h"
#include "Data/GameConfig/GameConfigConstants.h"

#define	LIW_ACCOUNT		0
#define	LIW_PASSWORD	1

#define LIW_OK			0
#define LIW_CANCEL		1



extern int g_iChatInputType;
extern int  LogIn;
extern int m_AutoLoginOnce;
extern wchar_t LogInID[MAX_USERNAME_SIZE + 1];
extern wchar_t g_WebStoreTicket[65];
extern BYTE Version[SIZE_PROTOCOLVERSION];
extern BYTE Serial[SIZE_PROTOCOLSERIAL + 1];

namespace
{
constexpr std::uintptr_t NoGoogleSocket = static_cast<std::uintptr_t>(~0ull);
constexpr std::uint64_t GoogleLoginTimeoutMs = 180000;
constexpr bool GoogleOnlyLogin = true;
constexpr int LoginPanelWidth = 312;
constexpr int LoginPanelHeight = 194;
constexpr int GoogleLoginButtonWidth = 220;
constexpr int GoogleLoginButtonHeight = 55;
constexpr int GoogleLoginButtonX = (LoginPanelWidth - GoogleLoginButtonWidth) / 2;
constexpr int GoogleLoginButtonY = 104;
constexpr int GoogleLoginMaxRequestBytes = 4096;
constexpr GLuint GoogleLoginButtonNormalTexture = BITMAP_LOG_IN + 18;
constexpr GLuint GoogleLoginButtonHoverTexture = BITMAP_LOG_IN + 19;
bool g_googleLoginActive = false;
std::uintptr_t g_googleListenerSocket = NoGoogleSocket;
std::uintptr_t g_googleClientSocket = NoGoogleSocket;
std::uint64_t g_googleLoginStartedTick = 0;
std::string g_googleLoginNonce;
std::string g_googleLoginRequestBuffer;
std::wstring g_googleLoginStatus;
bool g_transientTicketLogin = false;
bool g_antiCheatTicketLoaded = false;

struct AntiCheatLaunchTicketData
{
    std::string Ticket;
    std::string ClientBuildId;
    std::string ManifestVersion;
    std::string LauncherVersion;
    std::string PolicyVersion;
};

AntiCheatLaunchTicketData g_antiCheatLaunchTicket;

enum GoogleLoginButtonTexture
{
    GOOGLE_LOGIN_BUTTON_NORMAL = 0,
    GOOGLE_LOGIN_BUTTON_HOVER,
    GOOGLE_LOGIN_BUTTON_MAX
};

struct GoogleLoginButtonTextureInfo
{
    const wchar_t* Path;
    GLuint Index;
};

constexpr GoogleLoginButtonTextureInfo GoogleLoginButtonTextures[GOOGLE_LOGIN_BUTTON_MAX] =
{
    { L"Interface\\GoogleLogin\\google_login_normal.tga", GoogleLoginButtonNormalTexture },
    { L"Interface\\GoogleLogin\\google_login_hover.tga", GoogleLoginButtonHoverTexture },
};

std::wstring AsciiToWide(const std::string& text)
{
    std::wstring wide;
    wide.reserve(text.size());
    for (const unsigned char ch : text)
    {
        wide.push_back(static_cast<wchar_t>(ch));
    }
    return wide;
}

void CopyAsciiToWideBuffer(const std::string& source, wchar_t* destination, std::size_t destinationCount)
{
    if (destination == nullptr || destinationCount == 0)
    {
        return;
    }

    const std::size_t copyCount = std::min(source.size(), destinationCount - 1);
    for (std::size_t i = 0; i < copyCount; ++i)
    {
        destination[i] = static_cast<wchar_t>(static_cast<unsigned char>(source[i]));
    }
    destination[copyCount] = L'\0';
}

bool IsSafeAsciiValue(const std::string& value, std::size_t maxLength)
{
    if (value.empty() || value.size() > maxLength)
    {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](const unsigned char ch)
    {
        return ch >= 33 && ch <= 126;
    });
}

bool IsValidGameTicket(const std::string& ticket)
{
    if (ticket.size() != 19 || ticket.front() != 'G')
    {
        return false;
    }

    return std::all_of(ticket.begin(), ticket.end(), [](const unsigned char ch)
    {
        return std::isalnum(ch) != 0;
    });
}

bool IsValidStoreTicket(const std::string& ticket)
{
    if (ticket.size() != 33 || ticket.front() != 'S')
    {
        return false;
    }

    return std::all_of(ticket.begin(), ticket.end(), [](const unsigned char ch)
    {
        return std::isalnum(ch) != 0;
    });
}

bool IsSafeAntiCheatValue(const std::string& value, std::size_t maxLength, bool allowEmpty = false)
{
    if ((!allowEmpty && value.empty()) || value.size() > maxLength)
    {
        return false;
    }

    return std::all_of(value.begin(), value.end(), [](const unsigned char ch)
    {
        return std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == ':';
    });
}

std::vector<std::string> SplitLines(const std::string& value)
{
    std::vector<std::string> lines;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        lines.push_back(line);
    }

    return lines;
}

void LoadAntiCheatLaunchTicketFromPipe()
{
    if (g_antiCheatTicketLoaded)
    {
        return;
    }

    g_antiCheatTicketLoaded = true;

#ifdef _WIN32
    char* pipeNameRaw = nullptr;
    std::size_t pipeNameLength = 0;
    if (_dupenv_s(&pipeNameRaw, &pipeNameLength, "MUONLINE_AC_PIPE") != 0 || pipeNameRaw == nullptr)
    {
        return;
    }

    std::string pipeName(pipeNameRaw);
    free(pipeNameRaw);
    if (!IsSafeAntiCheatValue(pipeName, 96) || pipeName.rfind("muonline-ac-", 0) != 0)
    {
        return;
    }

    const std::string pipePath = "\\\\.\\pipe\\" + pipeName;
    if (!WaitNamedPipeA(pipePath.c_str(), 3000))
    {
        return;
    }

    HANDLE pipe = CreateFileA(
        pipePath.c_str(),
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return;
    }

    std::string payload;
    char buffer[512] = {};
    DWORD bytesRead = 0;
    while (ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        payload.append(buffer, buffer + bytesRead);
        if (payload.size() > 2048)
        {
            break;
        }
    }

    CloseHandle(pipe);

    const auto lines = SplitLines(payload);
    if (lines.size() < 5)
    {
        return;
    }

    if (lines[0].size() != 44 || lines[0].front() != 'L' || !IsSafeAntiCheatValue(lines[0], 64)
        || !IsSafeAntiCheatValue(lines[1], 120)
        || !IsSafeAntiCheatValue(lines[2], 80)
        || !IsSafeAntiCheatValue(lines[3], 80, true)
        || !IsSafeAntiCheatValue(lines[4], 80, true))
    {
        return;
    }

    g_antiCheatLaunchTicket.Ticket = lines[0];
    g_antiCheatLaunchTicket.ClientBuildId = lines[1];
    g_antiCheatLaunchTicket.ManifestVersion = lines[2];
    g_antiCheatLaunchTicket.LauncherVersion = lines[3];
    g_antiCheatLaunchTicket.PolicyVersion = lines[4];
#endif
}

void SendAntiCheatLaunchTicketIfAvailable()
{
    LoadAntiCheatLaunchTicketFromPipe();
    if (g_antiCheatLaunchTicket.Ticket.empty())
    {
        return;
    }

    if (SocketClient == nullptr || SocketClient->ToGameServer() == nullptr)
    {
        return;
    }

    const std::wstring ticket = AsciiToWide(g_antiCheatLaunchTicket.Ticket);
    const std::wstring clientBuildId = AsciiToWide(g_antiCheatLaunchTicket.ClientBuildId);
    const std::wstring manifestVersion = AsciiToWide(g_antiCheatLaunchTicket.ManifestVersion);
    const std::wstring launcherVersion = AsciiToWide(g_antiCheatLaunchTicket.LauncherVersion);
    const std::wstring policyVersion = AsciiToWide(g_antiCheatLaunchTicket.PolicyVersion);

    SocketClient->ToGameServer()->SendAntiCheatLaunchTicket(
        ticket.c_str(),
        clientBuildId.c_str(),
        manifestVersion.c_str(),
        launcherVersion.c_str(),
        policyVersion.c_str());
}

std::string CreateGoogleNonce()
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    std::random_device randomDevice;
    std::uniform_int_distribution<int> distribution(0, 15);

    std::string nonce;
    nonce.reserve(36);
    for (int i = 0; i < 36; ++i)
    {
        nonce.push_back(HexDigits[distribution(randomDevice)]);
    }
    return nonce;
}

std::string UrlEncode(const std::string& value)
{
    std::ostringstream encoded;
    encoded << std::uppercase << std::hex;

    for (const unsigned char ch : value)
    {
        if (std::isalnum(ch) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            encoded << static_cast<char>(ch);
            continue;
        }

        encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }

    return encoded.str();
}

int HexValue(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    return -1;
}

std::string UrlDecode(const std::string& value)
{
    std::string decoded;
    decoded.reserve(value.size());

    for (std::size_t i = 0; i < value.size(); ++i)
    {
        if (value[i] == '+')
        {
            decoded.push_back(' ');
            continue;
        }

        if (value[i] == '%' && i + 2 < value.size())
        {
            const int high = HexValue(value[i + 1]);
            const int low = HexValue(value[i + 2]);
            if (high >= 0 && low >= 0)
            {
                decoded.push_back(static_cast<char>((high << 4) | low));
                i += 2;
                continue;
            }
        }

        decoded.push_back(value[i]);
    }

    return decoded;
}

std::unordered_map<std::string, std::string> ParseQueryString(const std::string& query)
{
    std::unordered_map<std::string, std::string> values;
    std::size_t start = 0;
    while (start <= query.size())
    {
        const std::size_t end = query.find('&', start);
        const std::string part = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t equals = part.find('=');
        if (equals != std::string::npos)
        {
            values[UrlDecode(part.substr(0, equals))] = UrlDecode(part.substr(equals + 1));
        }

        if (end == std::string::npos)
        {
            break;
        }
        start = end + 1;
    }
    return values;
}

std::string ExtractHttpTarget(const std::string& request)
{
    const std::size_t lineEnd = request.find("\r\n");
    const std::string firstLine = request.substr(0, lineEnd == std::string::npos ? request.size() : lineEnd);
    const std::size_t methodEnd = firstLine.find(' ');
    if (methodEnd == std::string::npos || firstLine.substr(0, methodEnd) != "GET")
    {
        return {};
    }

    const std::size_t targetEnd = firstLine.find(' ', methodEnd + 1);
    if (targetEnd == std::string::npos)
    {
        return {};
    }

    return firstLine.substr(methodEnd + 1, targetEnd - methodEnd - 1);
}

std::wstring BuildGoogleLoginUrl(unsigned short port, const std::string& nonce)
{
    std::string callback;
    callback.reserve(48);
    callback.append("http://127.0.0.1:");
    callback.append(std::to_string(port));
    callback.append("/google-login/");

    const std::string encodedNonce = UrlEncode(nonce);
    const std::string encodedCallback = UrlEncode(callback);

    std::string url;
    url.reserve(64 + encodedNonce.size() + encodedCallback.size());
    url.append("https://muonline.pt/game-login?launcher_nonce=");
    url.append(encodedNonce);
    url.append("&callback=");
    url.append(encodedCallback);
    return AsciiToWide(url);
}

#ifdef _WIN32
void SendLoopbackResponse(SOCKET client, bool success)
{
    const std::string redirectUrl = success
        ? "https://muonline.pt/game-login-complete?status=success"
        : "https://muonline.pt/game-login-complete?status=error";

    std::string body;
    body.reserve(160 + redirectUrl.size());
    body.append("<!doctype html><meta charset=\"utf-8\"><title>MU Online</title>");
    body.append("<meta http-equiv=\"refresh\" content=\"0;url=");
    body.append(redirectUrl);
    body.append("\"><body>Redirecionando para o MU Online...</body>");

    const std::string contentLength = std::to_string(body.size());
    std::string response;
    response.reserve(220 + redirectUrl.size() + contentLength.size() + body.size());
    response.append("HTTP/1.1 303 See Other\r\nLocation: ");
    response.append(redirectUrl);
    response.append("\r\nContent-Type: text/html; charset=utf-8\r\nCache-Control: no-store\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\nContent-Length: ");
    response.append(contentLength);
    response.append("\r\n\r\n");
    response.append(body);
    send(client, response.data(), static_cast<int>(response.size()), 0);
}
#endif

bool IsCursorInGoogleButton(int baseX, int baseY)
{
    CInput& input = CInput::Instance();
    RECT buttonRect = {
        baseX + GoogleLoginButtonX,
        baseY + GoogleLoginButtonY,
        baseX + GoogleLoginButtonX + GoogleLoginButtonWidth,
        baseY + GoogleLoginButtonY + GoogleLoginButtonHeight,
    };

    return ::PtInRect(&buttonRect, input.GetCursorPos()) != FALSE;
}

bool IsGoogleButtonClicked(int baseX, int baseY)
{
    CInput& input = CInput::Instance();
    return input.IsLBtnUp() && IsCursorInGoogleButton(baseX, baseY);
}

bool EnsureGoogleLoginButtonTexture(GoogleLoginButtonTexture texture)
{
    static bool loaded[GOOGLE_LOGIN_BUTTON_MAX] = {};

    const int index = static_cast<int>(texture);
    if (index < 0 || index >= GOOGLE_LOGIN_BUTTON_MAX)
    {
        return false;
    }

    if (loaded[index] && Bitmaps.FindTexture(GoogleLoginButtonTextures[index].Index) != nullptr)
    {
        return true;
    }

    loaded[index] = LoadBitmap(
        GoogleLoginButtonTextures[index].Path,
        GoogleLoginButtonTextures[index].Index,
        GL_LINEAR,
        GL_CLAMP_TO_EDGE,
        false);
    g_ErrorReport.Write(
        L"[GoogleLoginButtonUI] %ls %ls (%u)\r\n",
        loaded[index] ? L"loaded" : L"failed",
        GoogleLoginButtonTextures[index].Path,
        GoogleLoginButtonTextures[index].Index);

    return loaded[index];
}

void RenderGoogleLoginButton(int baseX, int baseY)
{
    GoogleLoginButtonTexture texture = GOOGLE_LOGIN_BUTTON_NORMAL;
    if (IsCursorInGoogleButton(baseX, baseY)
        && EnsureGoogleLoginButtonTexture(GOOGLE_LOGIN_BUTTON_HOVER))
    {
        texture = GOOGLE_LOGIN_BUTTON_HOVER;
    }

    if (EnsureGoogleLoginButtonTexture(texture))
    {
        RenderBitmap(
            GoogleLoginButtonTextures[texture].Index,
            static_cast<float>(baseX + GoogleLoginButtonX),
            static_cast<float>(baseY + GoogleLoginButtonY),
            static_cast<float>(GoogleLoginButtonWidth),
            static_cast<float>(GoogleLoginButtonHeight),
            0.0f,
            0.0f,
            1.0f,
            1.0f,
            false,
            false);
        return;
    }

    RenderColor(
        static_cast<float>(baseX + GoogleLoginButtonX),
        static_cast<float>(baseY + GoogleLoginButtonY),
        static_cast<float>(GoogleLoginButtonWidth),
        static_cast<float>(GoogleLoginButtonHeight),
        0.35f,
        0);
    EndRenderColor();
}

void CloseGoogleLogin()
{
#ifdef _WIN32
    if (g_googleClientSocket != NoGoogleSocket)
    {
        closesocket(static_cast<SOCKET>(g_googleClientSocket));
        g_googleClientSocket = NoGoogleSocket;
    }

    if (g_googleListenerSocket != NoGoogleSocket)
    {
        closesocket(static_cast<SOCKET>(g_googleListenerSocket));
        g_googleListenerSocket = NoGoogleSocket;
    }
#endif

    g_googleLoginActive = false;
    g_googleLoginRequestBuffer.clear();
}

void FailGoogleLogin(const wchar_t* statusText)
{
    CloseGoogleLogin();
    g_googleLoginStatus = statusText != nullptr ? statusText : L"Falha no login Google.";
}

void BeginGoogleLogin()
{
    if (g_googleLoginActive)
    {
        g_googleLoginStatus = L"Aguardando navegador...";
        return;
    }

#ifdef _WIN32
    CloseGoogleLogin();

    static bool s_wsaInitialised = false;
    if (!s_wsaInitialised)
    {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        {
            FailGoogleLogin(L"Falha ao iniciar rede local.");
            return;
        }
        s_wsaInitialised = true;
    }

    const SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
    {
        FailGoogleLogin(L"Falha ao abrir callback local.");
        return;
    }

    g_googleListenerSocket = static_cast<std::uintptr_t>(listener);

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR)
    {
        FailGoogleLogin(L"Falha ao reservar porta local.");
        return;
    }

    if (listen(listener, 1) == SOCKET_ERROR)
    {
        FailGoogleLogin(L"Falha ao aguardar callback.");
        return;
    }

    sockaddr_in boundAddress = {};
    int boundAddressLength = sizeof(boundAddress);
    if (getsockname(listener, reinterpret_cast<sockaddr*>(&boundAddress), &boundAddressLength) == SOCKET_ERROR)
    {
        FailGoogleLogin(L"Falha ao identificar porta local.");
        return;
    }

    u_long nonBlocking = 1;
    ioctlsocket(listener, FIONBIO, &nonBlocking);

    g_googleLoginNonce = CreateGoogleNonce();
    g_googleLoginRequestBuffer.clear();
    g_googleLoginStartedTick = GetTickCount64();
    g_googleLoginActive = true;
    g_googleLoginStatus = L"Conclua no navegador...";

    const std::wstring loginUrl = BuildGoogleLoginUrl(ntohs(boundAddress.sin_port), g_googleLoginNonce);
    const INT_PTR result = reinterpret_cast<INT_PTR>(
        ShellExecuteW(g_hWnd, L"open", loginUrl.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (result <= 32)
    {
        FailGoogleLogin(L"Nao foi possivel abrir o navegador.");
    }
#else
    FailGoogleLogin(L"Login Google indisponivel nesta plataforma.");
#endif
}
} // namespace

void SendAntiCheatLaunchTicketBeforeLogin()
{
    SendAntiCheatLaunchTicketIfAvailable();
}

void PollGoogleLogin(CLoginWin& loginWin);
void ApplyGoogleLoginTicket(
    CLoginWin& loginWin,
    const std::string& account,
    const std::string& ticket,
    const std::string& storeTicket);

CLoginWin::CLoginWin()
{
    m_pUsernameInputBox = NULL;
    m_pPasswordInputBox = NULL;
}

CLoginWin::~CLoginWin()
{
    CloseGoogleLogin();
    SAFE_DELETE(m_pUsernameInputBox);
    SAFE_DELETE(m_pPasswordInputBox);
}

void CLoginWin::Create()
{
    g_transientTicketLogin = m_AutoLoginOnce != 0;

    if (GoogleOnlyLogin)
    {
        m_RememberMe = 0;
        if (!m_AutoLoginOnce)
        {
            m_Username[0] = L'\0';
            m_Password[0] = L'\0';
            GameConfig::GetInstance().SetRememberMe(false);
            GameConfig::GetInstance().SetEncryptedUsername(L"");
            GameConfig::GetInstance().SetEncryptedPassword(L"");
            GameConfig::GetInstance().Save();
        }
    }
    else
    {
        m_RememberMe = GameConfig::GetInstance().GetRememberMe();
    }

    if (!GoogleOnlyLogin && m_RememberMe)
    {
        // Use the helper we built to fill m_Username[11] and m_Password[21]
        GameConfig::GetInstance().DecryptCredentials(m_Username, m_Password, _countof(m_Username), _countof(m_Password));
    }
    else if (!m_AutoLoginOnce)
    {
        // Ensure they are empty if RememberMe is off
        m_Username[0] = L'\0';
        m_Password[0] = L'\0';
    }

    CWin::Create(LoginPanelWidth, LoginPanelHeight, BITMAP_LOG_IN + 7);

    m_asprInputBox[LIW_ACCOUNT].Create(156, 23, BITMAP_LOG_IN + 8);
    m_asprInputBox[LIW_PASSWORD].Create(156, 23, BITMAP_LOG_IN + 8);

    for (int i = 0; i < 2; ++i)
    {
        m_aBtn[i].Create(54, 30, BITMAP_BUTTON + i, 3, 2, 1);
        CWin::RegisterButton(&m_aBtn[i]);
    }

    m_aBtnRememberMe.Create(16, 16, BITMAP_CHECK_BTN, 2, 0, 0, -1, 1, 1, 1);
    CWin::RegisterButton(&m_aBtnRememberMe);
    if (GoogleOnlyLogin)
    {
        m_aBtnRememberMe.SetCheck(false);
    }

    SAFE_DELETE(m_pUsernameInputBox);

    m_pUsernameInputBox = new CUITextInputBox;
    m_pUsernameInputBox->Init(g_hWnd, 140, 14, MAX_USERNAME_SIZE);
    m_pUsernameInputBox->SetBackColor(0, 0, 0, 25);
    m_pUsernameInputBox->SetTextColor(255, 255, 230, 210);
    m_pUsernameInputBox->SetFont(g_hFixFont);
    m_pUsernameInputBox->SetState(UISTATE_NORMAL);
    if (m_RememberMe || m_AutoLoginOnce) {
        m_pUsernameInputBox->SetText(m_Username);
        if (m_RememberMe) {
            m_aBtnRememberMe.SetCheck(true);
        }
    }

    SAFE_DELETE(m_pPasswordInputBox);

    m_pPasswordInputBox = new CUITextInputBox;
    m_pPasswordInputBox->Init(g_hWnd, 140, 14, MAX_PASSWORD_SIZE, TRUE);
    m_pPasswordInputBox->SetBackColor(0, 0, 0, 25);
    m_pPasswordInputBox->SetTextColor(255, 255, 230, 210);
    m_pPasswordInputBox->SetFont(g_hFixFont);
    m_pPasswordInputBox->SetState(UISTATE_NORMAL);

    m_pUsernameInputBox->SetTabTarget(m_pPasswordInputBox);
    m_pPasswordInputBox->SetTabTarget(m_pUsernameInputBox);

    if (m_RememberMe || m_AutoLoginOnce) {
        m_pPasswordInputBox->SetText(m_Password);
        if (m_RememberMe) {
            m_aBtnRememberMe.SetCheck(true);
        }
    }

    this->FirstLoad = 1;
}

void CLoginWin::PreRelease()
{
    for (int i = 0; i < 2; ++i)
        m_asprInputBox[i].Release();
}

void CLoginWin::SetPosition(int x, int y)
{
	CWin::SetPosition(x, y);

	const int boxOffsetX = x + 109;
	m_asprInputBox[LIW_ACCOUNT].SetPosition(boxOffsetX, y + 106);
	m_asprInputBox[LIW_PASSWORD].SetPosition(boxOffsetX, y + 131);

	if (g_iChatInputType == 1)
	{
		const int boxX = int((x + 115) / g_fScreenRate_x);
		m_pUsernameInputBox->SetPosition(boxX, int((y + 112) / g_fScreenRate_y));
		m_pPasswordInputBox->SetPosition(boxX, int((y + 137) / g_fScreenRate_y));
	}

	m_aBtn[LIW_OK].SetPosition(x + 150, y + 178);
	m_aBtn[LIW_CANCEL].SetPosition(x + 211, y + 178);
	m_aBtnRememberMe.SetPosition(x + 109, y + 156);
}

void CLoginWin::Show(bool bShow)
{
    CWin::Show(bShow);

    for (int i = 0; i < 2; ++i)
    {
        m_asprInputBox[i].Show(bShow);
        m_aBtn[i].Show(!GoogleOnlyLogin && bShow);
    }
    m_aBtnRememberMe.Show(!GoogleOnlyLogin && bShow);

    // Drive the text fields' state so a hidden login screen releases keyboard
    // focus (portable fields stop SDL text input when hidden, #447).
    const int iState = (!GoogleOnlyLogin && bShow) ? UISTATE_NORMAL : UISTATE_HIDE;
    if (m_pUsernameInputBox) m_pUsernameInputBox->SetState(iState);
    if (m_pPasswordInputBox) m_pPasswordInputBox->SetState(iState);
}

bool CLoginWin::CursorInWin(int nArea)
{
    if (!CWin::m_bShow)
        return false;

    switch (nArea)
    {
    case WA_MOVE:
        return false;
    }

    return CWin::CursorInWin(nArea);
}

void CLoginWin::UpdateWhileActive(double)
{
    PollGoogleLogin(*this);

    if (m_AutoLoginOnce
        && CurrentProtocolState == RECEIVE_JOIN_SERVER_SUCCESS
        && wcslen(m_Username) > 0
        && wcslen(m_Password) > 0)
    {
        m_AutoLoginOnce = 0;
        RequestLogin();
        return;
    }

    if (GoogleOnlyLogin && CInput::Instance().IsKeyDown(VK_RETURN))
    {
        PlayBuffer(SOUND_CLICK01);
        BeginGoogleLogin();
        return;
    }

	if (!GoogleOnlyLogin && (m_aBtn[LIW_OK].IsClick() || CInput::Instance().IsKeyDown(VK_RETURN)))
	{
		PlayBuffer(SOUND_CLICK01);
		RequestLogin();
		return;
	}

    if (GoogleOnlyLogin && IsGoogleButtonClicked(GetXPos(), GetYPos()))
    {
        PlayBuffer(SOUND_CLICK01);
        BeginGoogleLogin();
        return;
    }

	if ((!GoogleOnlyLogin && m_aBtn[LIW_CANCEL].IsClick()) || CInput::Instance().IsKeyDown(VK_ESCAPE))
	{
		PlayBuffer(SOUND_CLICK01);
		CancelLogin();
		CUIMng::Instance().SetSysMenuWinShow(false);
		return;
	}

	if (!GoogleOnlyLogin && m_aBtnRememberMe.IsClick())
	{
		m_RememberMe = m_aBtnRememberMe.IsCheck();
		GameConfig::GetInstance().SetRememberMe(m_RememberMe != 0);
	}
}

void CLoginWin::UpdateWhileShow(double dDeltaTick)
{
    if (!GoogleOnlyLogin)
    {
        m_pUsernameInputBox->DoAction();
        m_pPasswordInputBox->DoAction();
    }
}

void CLoginWin::RenderControls()
{
    if (!GoogleOnlyLogin && FirstLoad)
    {
        (wcslen(m_Username) > 0 ? m_pPasswordInputBox : m_pUsernameInputBox)->GiveFocus();
        FirstLoad = 0;
    }

    CWin::RenderButtons();

    g_pRenderText->SetFont(g_hFixFont);
    g_pRenderText->SetBgColor(0, 0, 0, 0);
    g_pRenderText->SetTextColor(CLRDW_WHITE);

    const int baseX = GetXPos();
    const int baseY = GetYPos();

    if (GoogleOnlyLogin)
    {
        const float rateX = g_fScreenRate_x > 0.0f ? g_fScreenRate_x : 1.0f;
        const float rateY = g_fScreenRate_y > 0.0f ? g_fScreenRate_y : 1.0f;

        ::EnableAlphaBlend();
        ::glColor4ub(0, 0, 0, 210);
        ::RenderColor(
            static_cast<float>(baseX + 12) / rateX,
            static_cast<float>(baseY + 43) / rateY,
            static_cast<float>(LoginPanelWidth - 24) / rateX,
            139.0f / rateY,
            0.0f,
            0);
        ::EndRenderColor();
        ::DisableAlphaBlend();

        g_pRenderText->SetBgColor(0, 0, 0, 0);
        g_pRenderText->SetTextColor(210, 202, 213, 255);
        g_pRenderText->RenderText(
            static_cast<int>(baseX / rateX),
            static_cast<int>((baseY + 14) / rateY),
            L"MU Online",
            static_cast<int>(LoginPanelWidth / rateX),
            0,
            RT3_SORT_CENTER);

        wchar_t serverName[MAX_TEXT_LENGTH] = {};
        const wchar_t* serverStatus = g_ServerListManager->GetNonPVPInfo()
            ? I18N::Game::SDServer
            : I18N::Game::SDNonPvPServer;
        mu_swprintf(
            serverName,
            serverStatus,
            g_ServerListManager->GetSelectServerName(),
            g_ServerListManager->GetSelectServerIndex());
        g_pRenderText->SetTextColor(255, 204, 40, 255);
        g_pRenderText->RenderText(
            static_cast<int>(baseX / rateX),
            static_cast<int>((baseY + 56) / rateY),
            serverName,
            static_cast<int>(LoginPanelWidth / rateX),
            0,
            RT3_SORT_CENTER);

        RenderGoogleLoginButton(baseX, baseY);

        if (!g_googleLoginStatus.empty())
        {
            g_pRenderText->SetTextColor(222, 219, 224, 255);
            g_pRenderText->RenderText(
                static_cast<int>(baseX / rateX),
                static_cast<int>((baseY + 169) / rateY),
                g_googleLoginStatus.c_str(),
                static_cast<int>(LoginPanelWidth / rateX),
                0,
                RT3_SORT_CENTER);
        }
        return;
    }

    wchar_t szServerName[MAX_TEXT_LENGTH] = {};
    const wchar_t* pServerStatus = g_ServerListManager->GetNonPVPInfo() ? I18N::Game::SDServer : I18N::Game::SDNonPvPServer;
    mu_swprintf(szServerName, pServerStatus, g_ServerListManager->GetSelectServerName(), g_ServerListManager->GetSelectServerIndex());
    g_pRenderText->RenderText(int((baseX + 111) / g_fScreenRate_x), int((baseY + 80) / g_fScreenRate_y), szServerName);

    m_asprInputBox[LIW_ACCOUNT].Render();
    m_asprInputBox[LIW_PASSWORD].Render();
    m_pUsernameInputBox->Render();
    m_pPasswordInputBox->Render();

    g_pRenderText->RenderText(int((baseX + 30) / g_fScreenRate_x), int((baseY + 113) / g_fScreenRate_y), I18N::Game::Account);
    g_pRenderText->RenderText(int((baseX + 30) / g_fScreenRate_x), int((baseY + 139) / g_fScreenRate_y), I18N::Game::Password);
    g_pRenderText->RenderText(int((baseX + 130) / g_fScreenRate_x), int((baseY + 159) / g_fScreenRate_y), L"Remember me?");
}

void PollGoogleLogin(CLoginWin& loginWin)
{
#ifdef _WIN32
    if (!g_googleLoginActive)
    {
        return;
    }

    if (GetTickCount64() - g_googleLoginStartedTick > GoogleLoginTimeoutMs)
    {
        FailGoogleLogin(L"Tempo esgotado. Tente novamente.");
        return;
    }

    if (g_googleClientSocket == NoGoogleSocket && g_googleListenerSocket != NoGoogleSocket)
    {
        const SOCKET listener = static_cast<SOCKET>(g_googleListenerSocket);
        const SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET)
        {
            const int error = WSAGetLastError();
            if (error == WSAEWOULDBLOCK)
            {
                return;
            }

            FailGoogleLogin(L"Falha ao receber callback.");
            return;
        }

        u_long nonBlocking = 1;
        ioctlsocket(client, FIONBIO, &nonBlocking);

        closesocket(listener);
        g_googleListenerSocket = NoGoogleSocket;
        g_googleClientSocket = static_cast<std::uintptr_t>(client);
    }

    if (g_googleClientSocket == NoGoogleSocket)
    {
        return;
    }

    const SOCKET client = static_cast<SOCKET>(g_googleClientSocket);
    char buffer[1024] = {};
    const int received = recv(client, buffer, sizeof(buffer), 0);
    if (received == SOCKET_ERROR)
    {
        const int error = WSAGetLastError();
        if (error == WSAEWOULDBLOCK)
        {
            return;
        }

        FailGoogleLogin(L"Falha ao ler callback.");
        return;
    }

    if (received == 0)
    {
        FailGoogleLogin(L"Callback local vazio.");
        return;
    }

    g_googleLoginRequestBuffer.append(buffer, buffer + received);
    if (g_googleLoginRequestBuffer.size() > GoogleLoginMaxRequestBytes)
    {
        SendLoopbackResponse(client, false);
        FailGoogleLogin(L"Callback local invalido.");
        return;
    }

    if (g_googleLoginRequestBuffer.find("\r\n\r\n") == std::string::npos)
    {
        return;
    }

    const std::string target = ExtractHttpTarget(g_googleLoginRequestBuffer);
    const std::size_t queryStart = target.find('?');
    const std::string path = target.substr(0, queryStart);
    if ((path != "/google-login" && path != "/google-login/") || queryStart == std::string::npos)
    {
        SendLoopbackResponse(client, false);
        FailGoogleLogin(L"Callback local invalido.");
        return;
    }

    const auto query = ParseQueryString(target.substr(queryStart + 1));
    const auto account = query.find("account");
    const auto ticket = query.find("ticket");
    const auto nonce = query.find("launcher_nonce");
    const auto storeTicket = query.find("store_ticket");
    const bool hasValidStoreTicket = storeTicket == query.end()
        || (IsSafeAsciiValue(storeTicket->second, _countof(g_WebStoreTicket) - 1)
            && IsValidStoreTicket(storeTicket->second));
    const bool isValid = account != query.end()
        && ticket != query.end()
        && nonce != query.end()
        && nonce->second == g_googleLoginNonce
        && IsSafeAsciiValue(account->second, MAX_USERNAME_SIZE)
        && IsSafeAsciiValue(ticket->second, MAX_PASSWORD_SIZE)
        && IsValidGameTicket(ticket->second)
        && hasValidStoreTicket;

    SendLoopbackResponse(client, isValid);
    if (!isValid)
    {
        FailGoogleLogin(L"Ticket Google invalido.");
        return;
    }

    ApplyGoogleLoginTicket(
        loginWin,
        account->second,
        ticket->second,
        storeTicket != query.end() ? storeTicket->second : std::string{});
#endif
}

void ApplyGoogleLoginTicket(
    CLoginWin& loginWin,
    const std::string& account,
    const std::string& ticket,
    const std::string& storeTicket)
{
    CloseGoogleLogin();

    CopyAsciiToWideBuffer(account, m_Username, _countof(m_Username));
    CopyAsciiToWideBuffer(ticket, m_Password, _countof(m_Password));
    g_WebStoreTicket[0] = L'\0';
    if (!storeTicket.empty())
    {
        CopyAsciiToWideBuffer(storeTicket, g_WebStoreTicket, _countof(g_WebStoreTicket));
    }

    m_RememberMe = 0;
    g_transientTicketLogin = true;
    m_AutoLoginOnce = 1;

    if (loginWin.GetUsernameInputBox() != nullptr)
    {
        loginWin.GetUsernameInputBox()->SetText(m_Username);
    }

    if (loginWin.GetPasswordInputBox() != nullptr)
    {
        loginWin.GetPasswordInputBox()->SetText(m_Password);
    }

    g_googleLoginStatus = L"Ticket recebido.";
}

void CLoginWin::RequestLogin()
{
    if (CurrentProtocolState == REQUEST_JOIN_SERVER)
        return;

    CUIMng::Instance().HideWin(this);

    m_pUsernameInputBox->GetText(m_Username, _countof(m_Username));
    m_pPasswordInputBox->GetText(m_Password, _countof(m_Password));

    const bool transientTicketLogin = g_transientTicketLogin;

    if (!transientTicketLogin && m_aBtnRememberMe.IsCheck())
    {
        GameConfig::GetInstance().EncryptAndSaveCredentials(m_Username, m_Password);
    }
    else if (!transientTicketLogin)
    {
        // Clear saved credentials if user unchecked "Remember Me"
        GameConfig::GetInstance().SetEncryptedUsername(L"");
        GameConfig::GetInstance().SetEncryptedPassword(L"");
        GameConfig::GetInstance().Save();
    }

    if (wcslen(m_Username) <= 0)
        CUIMng::Instance().PopUpMsgWin(MESSAGE_INPUT_ID);
    else if (wcslen(m_Password) <= 0)
        CUIMng::Instance().PopUpMsgWin(MESSAGE_INPUT_PASSWORD);
    else
    {
        if (CurrentProtocolState == RECEIVE_JOIN_SERVER_SUCCESS)
        {
            g_ConsoleDebug->Write(MCD_NORMAL, L"Login with the following account: %ls", m_Username);

            g_ErrorReport.Write(L"> Login Request.\r\n");
            g_ErrorReport.Write(L"> Try to Login \"%ls\"\r\n", m_Username);

            LogIn = 1;
            wcscpy(LogInID, (m_Username));
            CurrentProtocolState = REQUEST_LOG_IN;

            SendAntiCheatLaunchTicketIfAvailable();
            SocketClient->ToGameServer()->SendLogin(m_Username, m_Password, Version, Serial);
            g_transientTicketLogin = false;

            // Keep the credentials in memory so auto-reconnect can re-login
            // without prompting after an in-game disconnect.
            ReconnectManager::Instance().CacheCredentials(m_Username, m_Password);

            g_pSystemLogBox->AddText(I18N::Game::VerifyingYourAccount, SEASON3B::TYPE_SYSTEM_MESSAGE);
            g_pSystemLogBox->AddText(I18N::Game::PleaseWait, SEASON3B::TYPE_SYSTEM_MESSAGE);
        }
    }
}

void CLoginWin::CancelLogin()
{
    ConnectConnectionServer();
    CUIMng::Instance().HideWin(this);
}

void CLoginWin::ConnectConnectionServer()
{
    LogIn = 0;
    CurrentProtocolState = REQUEST_JOIN_SERVER;
    CreateSocket(szServerIpAddress, g_ServerPort);
}
