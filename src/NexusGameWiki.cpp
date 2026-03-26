#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <deque>
#include <exception>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../vendor/nexus/Nexus.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/nlohmann/json.hpp"

using json = nlohmann::json;

namespace
{
constexpr const char* kAddonName = "NexusGameWiki";
constexpr const char* kAddonChannel = "NexusGameWiki";
constexpr const char* kWindowTitle = "NexusGameWiki";
constexpr const char* kToggleKeybindId = "NexusGameWiki.Toggle";
constexpr const char* kQuickAccessId = "NexusGameWiki.Shortcut";
constexpr const char* kIconTextureId = "NexusGameWiki.Icon.v2";
constexpr int kDefaultSignature = -26032501;
constexpr int kSearchDebounceMs = 180;
constexpr int kMaxSearchResults = 12;
constexpr long long kSearchCacheTtlSeconds = 6ll * 60ll * 60ll;
constexpr long long kPageCacheTtlSeconds = 24ll * 60ll * 60ll;
constexpr long long kImageCacheTtlSeconds = 30ll * 24ll * 60ll * 60ll;
constexpr long long kImageFileLoadTimeoutSeconds = 5ll;
constexpr long long kImageRetryCooldownSeconds = 15ll;
constexpr int kImageWorkerCount = 4;
constexpr int kMaxImageTextureLoadRequestsPerFrame = 4;
constexpr size_t kMaxRecentEntries = 60;

// The quick-access icon is embedded so public builds can ship as a single DLL.
constexpr unsigned char kEmbeddedIconPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x20,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x73, 0x7A, 0x7A, 0xF4, 0x00, 0x00, 0x00,
    0x01, 0x73, 0x52, 0x47, 0x42, 0x00, 0xAE, 0xCE, 0x1C, 0xE9, 0x00, 0x00,
    0x00, 0x04, 0x67, 0x41, 0x4D, 0x41, 0x00, 0x00, 0xB1, 0x8F, 0x0B, 0xFC,
    0x61, 0x05, 0x00, 0x00, 0x00, 0x09, 0x70, 0x48, 0x59, 0x73, 0x00, 0x00,
    0x0E, 0xC3, 0x00, 0x00, 0x0E, 0xC3, 0x01, 0xC7, 0x6F, 0xA8, 0x64, 0x00,
    0x00, 0x01, 0xAE, 0x49, 0x44, 0x41, 0x54, 0x58, 0x47, 0xED, 0x95, 0x2D,
    0x4F, 0x03, 0x41, 0x10, 0x86, 0x2B, 0x51, 0x04, 0x89, 0x21, 0x41, 0x92,
    0xD4, 0x20, 0x09, 0xAA, 0x3F, 0x01, 0x81, 0xC0, 0x90, 0x60, 0x40, 0x43,
    0x48, 0x48, 0x50, 0x40, 0xAF, 0x58, 0xA0, 0x64, 0x67, 0xB7, 0xA9, 0x29,
    0x29, 0xB4, 0xBB, 0xB7, 0x0B, 0xA9, 0x44, 0x20, 0x10, 0x28, 0x0C, 0xFC,
    0x00, 0x04, 0x86, 0x80, 0x44, 0x36, 0x04, 0x01, 0x99, 0xC2, 0xDE, 0x2D,
    0x73, 0x9F, 0xE5, 0x43, 0x71, 0x4F, 0xB2, 0x69, 0x33, 0xF3, 0xCE, 0x3B,
    0xEF, 0x65, 0xC5, 0x96, 0x4A, 0x05, 0x05, 0x05, 0x39, 0x50, 0x8A, 0xCF,
    0x28, 0xCD, 0x17, 0x7C, 0x03, 0xDB, 0x52, 0xB3, 0x25, 0x69, 0x44, 0xE5,
    0x3B, 0x9A, 0xA1, 0x68, 0xB5, 0x5A, 0x23, 0x4A, 0xF3, 0x55, 0x65, 0xF8,
    0x93, 0x6F, 0xF8, 0x1B, 0x3D, 0x4A, 0xF3, 0x67, 0xE5, 0xF3, 0x9A, 0xD2,
    0xB0, 0x99, 0xA6, 0xC1, 0x40, 0xBD, 0xDE, 0xFE, 0x18, 0xF5, 0xCF, 0x44,
    0x19, 0x7E, 0x1E, 0x31, 0x8C, 0x2C, 0x12, 0xD1, 0xA5, 0x11, 0x0D, 0x1E,
    0xB8, 0xC7, 0x0F, 0xA2, 0x3B, 0x12, 0x91, 0x52, 0x4C, 0x3A, 0x86, 0xB7,
    0x4A, 0xC1, 0xDC, 0x97, 0xBE, 0x11, 0x15, 0xA5, 0xD8, 0x8D, 0xD5, 0x48,
    0xC3, 0x1F, 0xE2, 0x34, 0xBE, 0xE6, 0x97, 0xA1, 0x66, 0x88, 0x2B, 0x71,
    0x03, 0xC4, 0x0D, 0x02, 0x78, 0xB3, 0x27, 0x92, 0x2D, 0x5A, 0x4D, 0x47,
    0xD7, 0x9B, 0x58, 0xA3, 0xBA, 0x2C, 0x9F, 0x44, 0xE8, 0x60, 0xFD, 0x78,
    0x6B, 0x94, 0x37, 0x76, 0xD7, 0x99, 0xF0, 0xCE, 0x40, 0x78, 0x8F, 0xF8,
    0x7B, 0xD4, 0x3E, 0x58, 0xB3, 0x9A, 0x76, 0x67, 0xAF, 0xEB, 0xF6, 0x50,
    0x8B, 0x33, 0xD4, 0x87, 0xEE, 0x49, 0x84, 0x0E, 0xB2, 0x66, 0xB5, 0x8C,
    0xA6, 0xEE, 0x57, 0x76, 0xBB, 0x62, 0x3E, 0xD0, 0x68, 0x60, 0xB6, 0x8E,
    0x9A, 0x41, 0xD8, 0x66, 0xB5, 0x4C, 0x7D, 0x82, 0x05, 0x59, 0xA4, 0x0D,
    0xA2, 0x31, 0x7E, 0x1D, 0x0D, 0x80, 0x35, 0xEC, 0xB9, 0xDA, 0x34, 0x9F,
    0x54, 0xE8, 0xE0, 0xC7, 0x15, 0xD4, 0x76, 0xA0, 0xE1, 0x5D, 0x83, 0xA8,
    0x5D, 0xE0, 0x22, 0x1A, 0x00, 0x6B, 0xD8, 0x43, 0x0D, 0x6A, 0x7F, 0xFD,
    0x0A, 0x98, 0xF0, 0x96, 0x01, 0xBC, 0x09, 0xAB, 0xA1, 0x01, 0x6C, 0x1D,
    0x35, 0xA8, 0xFD, 0xB3, 0x2B, 0xB0, 0x24, 0x05, 0x70, 0xC9, 0xE3, 0x13,
    0x4B, 0x9E, 0xC1, 0xFF, 0x15, 0x20, 0xEE, 0xA1, 0xA1, 0x01, 0xE2, 0x34,
    0xD4, 0x87, 0xEE, 0x49, 0xC4, 0x1D, 0xF4, 0x0D, 0xF4, 0xC3, 0xFF, 0xE1,
    0x91, 0x0A, 0x5E, 0x9C, 0x00, 0xAF, 0xB4, 0x4F, 0x67, 0x7F, 0x10, 0x20,
    0x3C, 0xF1, 0x0F, 0x4D, 0x3E, 0xCD, 0x50, 0x01, 0x06, 0x4F, 0xF1, 0xA7,
    0x51, 0xF2, 0x63, 0x04, 0x57, 0x81, 0xB9, 0x0F, 0x77, 0x71, 0x9A, 0xF0,
    0x31, 0x82, 0xBE, 0xD6, 0x6C, 0xDC, 0xED, 0x67, 0xD2, 0x39, 0x3D, 0x9C,
    0xC2, 0x7B, 0xA5, 0x75, 0x17, 0xA9, 0xF9, 0x8A, 0xD2, 0xB0, 0x41, 0xEB,
    0x2E, 0x5A, 0xB3, 0x69, 0x3C, 0xB4, 0x5E, 0x50, 0x50, 0x50, 0x60, 0x79,
    0x07, 0x90, 0x83, 0xF8, 0xF1, 0xDF, 0xD2, 0xED, 0x48, 0x00, 0x00, 0x00,
    0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

struct KeyOption
{
    const char* Label;
    unsigned short Vk;
};

constexpr KeyOption kKeyOptions[] = {
    { "A", 'A' }, { "B", 'B' }, { "C", 'C' }, { "D", 'D' }, { "E", 'E' }, { "F", 'F' }, { "G", 'G' },
    { "H", 'H' }, { "I", 'I' }, { "J", 'J' }, { "K", 'K' }, { "L", 'L' }, { "M", 'M' }, { "N", 'N' },
    { "O", 'O' }, { "P", 'P' }, { "Q", 'Q' }, { "R", 'R' }, { "S", 'S' }, { "T", 'T' }, { "U", 'U' },
    { "V", 'V' }, { "W", 'W' }, { "X", 'X' }, { "Y", 'Y' }, { "Z", 'Z' },
    { "0", '0' }, { "1", '1' }, { "2", '2' }, { "3", '3' }, { "4", '4' },
    { "5", '5' }, { "6", '6' }, { "7", '7' }, { "8", '8' }, { "9", '9' },
    { "F1", VK_F1 }, { "F2", VK_F2 }, { "F3", VK_F3 }, { "F4", VK_F4 }, { "F5", VK_F5 }, { "F6", VK_F6 },
    { "F7", VK_F7 }, { "F8", VK_F8 }, { "F9", VK_F9 }, { "F10", VK_F10 }, { "F11", VK_F11 }, { "F12", VK_F12 }
};

struct HotkeySettings
{
    bool Ctrl = true;
    bool Alt = false;
    bool Shift = true;
    unsigned short Key = 'G';
};

struct Settings
{
    bool WindowVisible = false;
    HotkeySettings Hotkey;
    bool CollapseContentsByDefault = false;
    bool CollapseNotesByDefault = false;
    bool CollapseTriviaByDefault = false;
    bool CollapseGalleryByDefault = false;
    bool CollapseHistoryByDefault = false;
    bool CollapseSeeAlsoByDefault = false;
    bool CollapseReferencesByDefault = false;
    bool CollapseExternalLinksByDefault = false;
};

struct SearchHit
{
    int PageId = -1;
    std::string Title;
    std::string Snippet;
};

enum class LeftPaneMode
{
    Search,
    Recent,
    Favorites
};

struct SavedPageEntry
{
    int PageId = -1;
    std::string Title;
    long long SavedAt = 0;
};

enum class HtmlInlineType
{
    Text,
    Link,
    Image
};

enum class HtmlBlockType
{
    Paragraph,
    Heading,
    List,
    Table,
    Quote,
    Infobox,
    Rule
};

struct HtmlInline
{
    HtmlInlineType Type = HtmlInlineType::Text;
    std::string Text;
    std::string Href;
    std::string ImageSource;
    std::string ImageAlt;
    int ImageWidth = 20;
    int ImageHeight = 20;
};

struct HtmlTableCell
{
    bool Header = false;
    std::vector<HtmlInline> Inlines;
};

struct HtmlBlock
{
    HtmlBlockType Type = HtmlBlockType::Paragraph;
    int Level = 0;
    std::vector<HtmlInline> Inlines;
    std::vector<HtmlInline> ExtraInlines;
    std::vector<std::vector<HtmlInline>> ListItems;
    std::vector<std::vector<HtmlTableCell>> TableRows;
    std::vector<HtmlBlock> ChildBlocks;
};

struct WikiSectionInfo
{
    std::string Number;
    std::string Index;
    std::string Title;
    int Level = 0;
    int TocLevel = 0;
};

struct ParsedPage
{
    std::vector<HtmlBlock> Blocks;
    std::vector<WikiSectionInfo> Sections;
};

struct PageDocument
{
    int PageId = -1;
    std::string Title;
    std::string DisplayTitle;
    std::string Url;
    std::string Html;
    ParsedPage Parsed;
};

struct CacheMetadata
{
    long long FetchedAt = 0;
    long long LastUsedAt = 0;
};

struct RemoteImage
{
    std::string CanonicalSource;
    std::string CachePath;
    std::string TextureId;
    Texture* LoadedTexture = nullptr;
    long long LoadedFileWriteAt = 0;
    bool FileLoadRequested = false;
    std::chrono::steady_clock::time_point FileLoadStartedAt = std::chrono::steady_clock::time_point{};
    std::string PendingTempPath;
    std::string PendingTextureId;
    Texture* PendingTexture = nullptr;
    bool PendingValidationRequested = false;
    std::chrono::steady_clock::time_point PendingValidationStartedAt = std::chrono::steady_clock::time_point{};
    bool Requested = false;
    bool DownloadPending = false;
    int RequestAttempts = 0;
    std::chrono::steady_clock::time_point LastRequestedAt = std::chrono::steady_clock::time_point{};
};

struct SearchJob
{
    bool Pending = false;
    bool ForceRefresh = false;
    unsigned long long Token = 0;
    std::string QueryRaw;
    std::string QueryNormalized;
};

struct SearchFetchResult
{
    bool Ready = false;
    bool Success = false;
    bool FromCache = false;
    bool ForceRefresh = false;
    bool CacheStale = false;
    unsigned long long Token = 0;
    long long CacheFetchedAt = 0;
    std::string QueryNormalized;
    std::string Error;
    std::vector<SearchHit> Results;
};

struct PageJob
{
    bool Pending = false;
    bool ForceRefresh = false;
    unsigned long long Token = 0;
    int TabId = 0;
    int PageId = -1;
    std::string Title;
};

struct PageFetchResult
{
    bool Ready = false;
    bool Success = false;
    bool FromCache = false;
    bool ForceRefresh = false;
    bool CacheStale = false;
    unsigned long long Token = 0;
    long long CacheFetchedAt = 0;
    int TabId = 0;
    int PageId = -1;
    std::string Error;
    PageDocument Document;
};

struct ImageJob
{
    unsigned long long Generation = 0;
    std::string Source;
    std::string CachePath;
    std::string TempPath;
};

struct ImageFetchResult
{
    bool Success = false;
    unsigned long long Generation = 0;
    std::string Source;
    std::string CachePath;
    std::string TempPath;
    std::string Error;
};

struct SearchState
{
    std::string QueryRaw;
    std::string QueryNormalized;
    std::string LastLoadedQueryNormalized;
    std::string Status;
    std::vector<SearchHit> Results;
    bool Loading = false;
    bool FromCache = false;
    bool CacheStale = false;
    bool HadFailure = false;
    int SelectedIndex = -1;
    long long CacheFetchedAt = 0;
    unsigned long long DispatchedToken = 0;
    std::chrono::steady_clock::time_point LastEditedAt = std::chrono::steady_clock::now();
};

struct ArticleTab
{
    int Id = 0;
    int PageId = -1;
    std::string Title;
    std::string Status;
    bool Loading = false;
    bool FromCache = false;
    bool CacheStale = false;
    bool HadFailure = false;
    long long CacheFetchedAt = 0;
    unsigned long long DispatchedToken = 0;
    PageDocument Document;
    std::unordered_map<std::string, bool> CollapsedSections;
    std::string SelectedSectionIndex;
    std::string PendingScrollSectionIndex;
};

struct WorkerState
{
    std::mutex Mutex;
    std::condition_variable Cv;
    bool Stop = false;
    unsigned long long ImageGeneration = 1;
    SearchJob Search;
    SearchFetchResult SearchResult;
    PageJob Page;
    PageFetchResult PageResult;
    std::deque<ImageJob> ImageJobs;
    std::deque<ImageFetchResult> ImageResults;
    std::thread SearchThread;
    std::thread PageThread;
    std::vector<std::thread> ImageThreads;
};

// AppState owns the live UI/session state. Anything that should survive restarts is
// persisted separately under the addon folder and reloaded into this structure on startup.
struct AppState
{
    AddonAPI* Api = nullptr;
    bool WindowVisible = false;
    bool FocusSearchOnOpen = false;
    bool IconRegistered = false;
    char SearchBuffer[256] = {};
    Settings UserSettings;
    std::string AddonDirectory;
    std::string SettingsPath;
    std::string LibraryPath;
    std::string CacheRoot;
    std::string SearchCacheDirectory;
    std::string PageCacheDirectory;
    std::string ImageCacheDirectory;
    std::unordered_map<std::string, RemoteImage> RemoteImages;
    LeftPaneMode CurrentLeftPaneMode = LeftPaneMode::Search;
    std::vector<SavedPageEntry> RecentPages;
    std::vector<SavedPageEntry> FavoritePages;
    SearchState Search;
    std::vector<ArticleTab> Tabs;
    int ActiveTabId = 0;
    int ImageTextureLoadRequestsThisFrame = 0;
    unsigned long long NextSearchToken = 0;
    unsigned long long NextPageToken = 0;
    int NextTabId = 0;
    int PendingOpenPageId = 0;
    std::string PendingOpenTitle;
};

AppState gState;
WorkerState gWorker;

std::string TrimCopy(std::string value)
{
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos)
    {
        return {};
    }

    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(begin, end - begin + 1);
}

std::string JoinPath(const std::string& left, const std::string& right)
{
    if (left.empty())
    {
        return right;
    }

    if (left.back() == '\\' || left.back() == '/')
    {
        return left + right;
    }

    return left + "\\" + right;
}

void EnsureDirectory(const std::string& path)
{
    if (!path.empty())
    {
        CreateDirectoryA(path.c_str(), nullptr);
    }
}

std::string ReadTextFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return {};
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool WriteTextFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

bool WriteBinaryFile(const std::string& path, const std::string& content)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        return false;
    }

    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    return output.good();
}

void DeleteFileIfExists(const std::string& path)
{
    if (!path.empty())
    {
        DeleteFileA(path.c_str());
    }
}

bool ReplaceFileFromTemp(const std::string& tempPath, const std::string& finalPath)
{
    if (tempPath.empty() || finalPath.empty())
    {
        return false;
    }

    return MoveFileExA(tempPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) != FALSE;
}

bool TryGetFileLastWriteUnix(const std::string& path, long long& lastWriteUnix)
{
    lastWriteUnix = 0;

    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attributes))
    {
        return false;
    }

    if ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        return false;
    }

    ULARGE_INTEGER timestamp{};
    timestamp.LowPart = attributes.ftLastWriteTime.dwLowDateTime;
    timestamp.HighPart = attributes.ftLastWriteTime.dwHighDateTime;

    constexpr unsigned long long kUnixEpochInFiletimeTicks = 116444736000000000ull;
    if (timestamp.QuadPart <= kUnixEpochInFiletimeTicks)
    {
        return true;
    }

    lastWriteUnix = static_cast<long long>((timestamp.QuadPart - kUnixEpochInFiletimeTicks) / 10000000ull);
    return true;
}

void TouchJsonPayload(const std::string& path, json payload)
{
    if (path.empty() || !payload.is_object())
    {
        return;
    }

    payload["lastUsedAt"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    WriteTextFile(path, payload.dump());
}

void Log(ELogLevel level, const std::string& message)
{
    if (gState.Api != nullptr)
    {
        gState.Api->Log(level, kAddonChannel, message.c_str());
    }
}

void Notify(const std::string& message)
{
    if (gState.Api != nullptr)
    {
        gState.Api->UI.SendAlert(message.c_str());
    }
}

void OnAsyncTextureLoaded(const char*, Texture*)
{
}

std::string Normalize(std::string_view value)
{
    std::string normalized;
    normalized.reserve(value.size());

    bool previousSpace = true;
    for (unsigned char ch : value)
    {
        if (ch < 128 && std::isalnum(ch) != 0)
        {
            normalized.push_back(static_cast<char>(std::tolower(ch)));
            previousSpace = false;
        }
        else if (!previousSpace)
        {
            normalized.push_back(' ');
            previousSpace = true;
        }
    }

    if (!normalized.empty() && normalized.back() == ' ')
    {
        normalized.pop_back();
    }

    return normalized;
}

bool IsDigitsOnly(std::string_view value)
{
    return !value.empty() && std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

bool IsQuantityToken(std::string_view value)
{
    if (value.empty())
    {
        return false;
    }

    if (IsDigitsOnly(value))
    {
        return true;
    }

    if (value.size() >= 2 && (value.front() == 'x' || value.front() == 'X') && IsDigitsOnly(value.substr(1)))
    {
        return true;
    }

    if (value.size() >= 2 && (value.back() == 'x' || value.back() == 'X') && IsDigitsOnly(value.substr(0, value.size() - 1)))
    {
        return true;
    }

    return false;
}

std::string JoinTokens(const std::vector<std::string>& tokens, size_t startIndex = 0)
{
    if (startIndex >= tokens.size())
    {
        return {};
    }

    std::string joined;
    for (size_t index = startIndex; index < tokens.size(); ++index)
    {
        if (!joined.empty())
        {
            joined.push_back(' ');
        }
        joined += tokens[index];
    }

    return joined;
}

std::string NormalizeSearchInput(std::string value)
{
    value = TrimCopy(value);
    // Pasted GW2-style text often arrives as "[ 25 Item Name ]". Strip wrapping
    // brackets and leading quantity noise so search sees the actual subject.
    while (value.size() >= 2 && value.front() == '[' && value.back() == ']')
    {
        value = TrimCopy(value.substr(1, value.size() - 2));
    }

    if (value.empty())
    {
        return {};
    }

    std::istringstream stream(value);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token)
    {
        if (!token.empty())
        {
            tokens.push_back(std::move(token));
        }
    }

    while (tokens.size() > 1 && IsQuantityToken(tokens.front()))
    {
        tokens.erase(tokens.begin());
    }

    return TrimCopy(JoinTokens(tokens));
}

std::vector<std::string> SplitWords(std::string_view value)
{
    std::vector<std::string> words;
    std::istringstream stream{std::string(value)};
    std::string word;
    while (stream >> word)
    {
        if (!word.empty())
        {
            words.push_back(std::move(word));
        }
    }

    return words;
}

std::string JoinWords(const std::vector<std::string>& words, size_t count)
{
    if (words.empty() || count == 0)
    {
        return {};
    }

    count = std::min(count, words.size());
    std::string joined;
    for (size_t index = 0; index < count; ++index)
    {
        if (!joined.empty())
        {
            joined.push_back(' ');
        }
        joined += words[index];
    }
    return joined;
}

void AddUniqueSearchCandidate(std::vector<std::string>& candidates, std::string candidate)
{
    candidate = Normalize(candidate);
    if (candidate.size() < 2)
    {
        return;
    }

    if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end())
    {
        candidates.push_back(std::move(candidate));
    }
}

std::vector<std::string> BuildPrefixSearchCandidates(const std::string& queryRaw)
{
    std::vector<std::string> candidates;
    AddUniqueSearchCandidate(candidates, queryRaw);

    const std::string normalized = Normalize(queryRaw);
    const std::vector<std::string> words = SplitWords(normalized);
    if (words.empty())
    {
        return candidates;
    }

    if (words.size() >= 2)
    {
        AddUniqueSearchCandidate(candidates, JoinWords(words, words.size() - 1));
        AddUniqueSearchCandidate(candidates, JoinWords(words, std::min<size_t>(2, words.size())));
    }

    const std::string& firstWord = words.front();
    if (firstWord.size() >= 5)
    {
        AddUniqueSearchCandidate(candidates, firstWord.substr(0, 5));
    }
    else if (firstWord.size() >= 4)
    {
        AddUniqueSearchCandidate(candidates, firstWord.substr(0, 4));
    }

    return candidates;
}

int EditDistance(std::string_view left, std::string_view right)
{
    if (left.empty())
    {
        return static_cast<int>(right.size());
    }
    if (right.empty())
    {
        return static_cast<int>(left.size());
    }

    std::vector<int> previous(right.size() + 1, 0);
    std::vector<int> current(right.size() + 1, 0);
    for (size_t column = 0; column <= right.size(); ++column)
    {
        previous[column] = static_cast<int>(column);
    }

    for (size_t row = 1; row <= left.size(); ++row)
    {
        current[0] = static_cast<int>(row);
        for (size_t column = 1; column <= right.size(); ++column)
        {
            const int substitutionCost = left[row - 1] == right[column - 1] ? 0 : 1;
            current[column] = std::min({
                previous[column] + 1,
                current[column - 1] + 1,
                previous[column - 1] + substitutionCost
            });
        }

        std::swap(previous, current);
    }

    return previous.back();
}

size_t CommonPrefixLength(std::string_view left, std::string_view right)
{
    const size_t limit = std::min(left.size(), right.size());
    size_t index = 0;
    while (index < limit && left[index] == right[index])
    {
        index += 1;
    }
    return index;
}

long long UnixNow()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

bool IsCacheStale(const CacheMetadata& metadata, long long ttlSeconds)
{
    if (metadata.FetchedAt <= 0)
    {
        return true;
    }

    return UnixNow() - metadata.FetchedAt >= ttlSeconds;
}

std::string DescribeCacheAge(long long fetchedAt)
{
    if (fetchedAt <= 0)
    {
        return "unknown age";
    }

    long long seconds = UnixNow() - fetchedAt;
    if (seconds < 0)
    {
        seconds = 0;
    }

    if (seconds < 60)
    {
        return std::to_string(seconds) + "s old";
    }

    const long long minutes = seconds / 60;
    if (minutes < 60)
    {
        return std::to_string(minutes) + "m old";
    }

    const long long hours = minutes / 60;
    if (hours < 48)
    {
        return std::to_string(hours) + "h old";
    }

    const long long days = hours / 24;
    return std::to_string(days) + "d old";
}

std::string ReplaceAllCopy(std::string value, const std::string& needle, const std::string& replacement)
{
    size_t cursor = 0;
    while ((cursor = value.find(needle, cursor)) != std::string::npos)
    {
        value.replace(cursor, needle.size(), replacement);
        cursor += replacement.size();
    }

    return value;
}

std::string StripTags(std::string_view value)
{
    std::string output;
    output.reserve(value.size());

    bool inTag = false;
    for (char ch : value)
    {
        if (ch == '<')
        {
            inTag = true;
            continue;
        }
        if (ch == '>')
        {
            inTag = false;
            continue;
        }

        if (!inTag)
        {
            output.push_back(ch);
        }
    }

    output = ReplaceAllCopy(output, "&quot;", "\"");
    output = ReplaceAllCopy(output, "&#039;", "'");
    output = ReplaceAllCopy(output, "&amp;", "&");
    output = ReplaceAllCopy(output, "&lt;", "<");
    output = ReplaceAllCopy(output, "&gt;", ">");
    output = ReplaceAllCopy(output, "&nbsp;", " ");
    return TrimCopy(output);
}

std::string LowerCopy(std::string_view value)
{
    std::string output;
    output.reserve(value.size());
    for (unsigned char ch : value)
    {
        output.push_back(static_cast<char>(std::tolower(ch)));
    }
    return output;
}

int ParseIntOrFallback(const std::string& value, int fallback = 0)
{
    if (value.empty())
    {
        return fallback;
    }

    try
    {
        return std::stoi(value);
    }
    catch (...)
    {
        return fallback;
    }
}

std::string DecodeHtmlEntities(std::string_view value)
{
    std::string output;
    output.reserve(value.size());

    for (size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] != '&')
        {
            output.push_back(value[index]);
            continue;
        }

        const size_t semicolon = value.find(';', index + 1);
        if (semicolon == std::string_view::npos)
        {
            output.push_back(value[index]);
            continue;
        }

        const std::string entity(value.substr(index + 1, semicolon - index - 1));
        if (entity == "quot")
        {
            output.push_back('"');
        }
        else if (entity == "amp")
        {
            output.push_back('&');
        }
        else if (entity == "apos" || entity == "#039" || entity == "#39")
        {
            output.push_back('\'');
        }
        else if (entity == "lt")
        {
            output.push_back('<');
        }
        else if (entity == "gt")
        {
            output.push_back('>');
        }
        else if (entity == "nbsp")
        {
            output.push_back(' ');
        }
        else if (!entity.empty() && entity[0] == '#')
        {
            int codePoint = 0;
            try
            {
                if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X'))
                {
                    codePoint = std::stoi(entity.substr(2), nullptr, 16);
                }
                else
                {
                    codePoint = std::stoi(entity.substr(1), nullptr, 10);
                }
            }
            catch (...)
            {
                codePoint = 0;
            }

            if (codePoint > 0 && codePoint < 128)
            {
                output.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint == 160)
            {
                output.push_back(' ');
            }
        }
        else
        {
            output.push_back('&');
            output += entity;
            output.push_back(';');
        }

        index = semicolon;
    }

    return output;
}

std::string CollapseHtmlWhitespace(std::string_view value)
{
    std::string output;
    output.reserve(value.size());

    bool previousSpace = true;
    for (unsigned char ch : value)
    {
        if (std::isspace(ch) != 0)
        {
            if (!previousSpace)
            {
                output.push_back(' ');
                previousSpace = true;
            }
        }
        else
        {
            output.push_back(static_cast<char>(ch));
            previousSpace = false;
        }
    }

    if (!output.empty() && output.back() == ' ')
    {
        output.pop_back();
    }

    return output;
}

std::string DecodeAndCollapseHtmlText(std::string_view value)
{
    return TrimCopy(CollapseHtmlWhitespace(DecodeHtmlEntities(value)));
}

std::string MapClassValue(const std::unordered_map<std::string, std::string>& attributes)
{
    const auto iterator = attributes.find("class");
    if (iterator == attributes.end())
    {
        return {};
    }

    return LowerCopy(iterator->second);
}

bool ClassContains(const std::unordered_map<std::string, std::string>& attributes, const char* needle)
{
    const std::string haystack = MapClassValue(attributes);
    if (haystack.empty())
    {
        return false;
    }

    return haystack.find(LowerCopy(needle)) != std::string::npos;
}

std::string AttributeOrEmpty(const std::unordered_map<std::string, std::string>& attributes, const char* key)
{
    const auto iterator = attributes.find(key);
    if (iterator == attributes.end())
    {
        return {};
    }

    return iterator->second;
}

bool AttributeContainsInsensitive(const std::unordered_map<std::string, std::string>& attributes, const char* key, const char* needle)
{
    const std::string value = LowerCopy(AttributeOrEmpty(attributes, key));
    if (value.empty())
    {
        return false;
    }

    return value.find(LowerCopy(needle)) != std::string::npos;
}

bool IsVoidHtmlTag(const std::string& name)
{
    return name == "br" || name == "img" || name == "hr" || name == "meta" || name == "link" || name == "input";
}

bool IgnoreTagCompletely(const std::string& name, const std::unordered_map<std::string, std::string>& attributes)
{
    if (name == "script" || name == "style")
    {
        return true;
    }

    if (name == "span" && ClassContains(attributes, "mw-editsection"))
    {
        return true;
    }

    if (name == "sup" && (ClassContains(attributes, "reference") || ClassContains(attributes, "mw-ref")))
    {
        return true;
    }

    if (AttributeContainsInsensitive(attributes, "style", "display:none"))
    {
        return true;
    }

    if ((name == "div" || name == "table" || name == "ul") &&
        (ClassContains(attributes, "navbox") || ClassContains(attributes, "toc") || ClassContains(attributes, "metadata") || ClassContains(attributes, "mw-collapsible")))
    {
        return true;
    }

    return false;
}

struct HtmlToken
{
    enum class Type
    {
        Text,
        StartTag,
        EndTag
    };

    Type TokenType = Type::Text;
    std::string Name;
    std::unordered_map<std::string, std::string> Attributes;
    std::string Text;
    bool SelfClosing = false;
};

std::vector<HtmlToken> TokenizeHtml(const std::string& html)
{
    std::vector<HtmlToken> tokens;
    size_t cursor = 0;

    while (cursor < html.size())
    {
        const size_t tagStart = html.find('<', cursor);
        if (tagStart == std::string::npos)
        {
            if (cursor < html.size())
            {
                tokens.push_back(HtmlToken{ HtmlToken::Type::Text, {}, {}, html.substr(cursor), false });
            }
            break;
        }

        if (tagStart > cursor)
        {
            tokens.push_back(HtmlToken{ HtmlToken::Type::Text, {}, {}, html.substr(cursor, tagStart - cursor), false });
        }

        if (html.compare(tagStart, 4, "<!--") == 0)
        {
            const size_t commentEnd = html.find("-->", tagStart + 4);
            cursor = commentEnd == std::string::npos ? html.size() : commentEnd + 3;
            continue;
        }

        const size_t tagEnd = html.find('>', tagStart + 1);
        if (tagEnd == std::string::npos)
        {
            break;
        }

        std::string rawTag = html.substr(tagStart + 1, tagEnd - tagStart - 1);
        cursor = tagEnd + 1;
        rawTag = TrimCopy(rawTag);
        if (rawTag.empty())
        {
            continue;
        }

        if (rawTag[0] == '!')
        {
            continue;
        }

        HtmlToken token;
        if (rawTag[0] == '/')
        {
            token.TokenType = HtmlToken::Type::EndTag;
            token.Name = LowerCopy(TrimCopy(rawTag.substr(1)));
            tokens.push_back(std::move(token));
            continue;
        }

        token.TokenType = HtmlToken::Type::StartTag;
        if (!rawTag.empty() && rawTag.back() == '/')
        {
            token.SelfClosing = true;
            rawTag.pop_back();
            rawTag = TrimCopy(rawTag);
        }

        size_t position = 0;
        while (position < rawTag.size() && std::isspace(static_cast<unsigned char>(rawTag[position])) == 0 && rawTag[position] != '=')
        {
            token.Name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(rawTag[position]))));
            position += 1;
        }

        while (position < rawTag.size())
        {
            while (position < rawTag.size() && std::isspace(static_cast<unsigned char>(rawTag[position])) != 0)
            {
                position += 1;
            }

            if (position >= rawTag.size())
            {
                break;
            }

            std::string key;
            while (position < rawTag.size() &&
                std::isspace(static_cast<unsigned char>(rawTag[position])) == 0 &&
                rawTag[position] != '=')
            {
                key.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(rawTag[position]))));
                position += 1;
            }

            while (position < rawTag.size() && std::isspace(static_cast<unsigned char>(rawTag[position])) != 0)
            {
                position += 1;
            }

            std::string value;
            if (position < rawTag.size() && rawTag[position] == '=')
            {
                position += 1;
                while (position < rawTag.size() && std::isspace(static_cast<unsigned char>(rawTag[position])) != 0)
                {
                    position += 1;
                }

                if (position < rawTag.size() && (rawTag[position] == '"' || rawTag[position] == '\''))
                {
                    const char quote = rawTag[position];
                    position += 1;
                    while (position < rawTag.size() && rawTag[position] != quote)
                    {
                        value.push_back(rawTag[position]);
                        position += 1;
                    }
                    if (position < rawTag.size() && rawTag[position] == quote)
                    {
                        position += 1;
                    }
                }
                else
                {
                    while (position < rawTag.size() && std::isspace(static_cast<unsigned char>(rawTag[position])) == 0)
                    {
                        value.push_back(rawTag[position]);
                        position += 1;
                    }
                }
            }

            if (!key.empty())
            {
                token.Attributes[key] = DecodeHtmlEntities(value);
            }
        }

        if (IsVoidHtmlTag(token.Name))
        {
            token.SelfClosing = true;
        }

        const bool isRawTextTag = token.Name == "script" || token.Name == "style";
        const std::string rawTextCloseTag = token.Name == "script" ? "</script>" : "</style>";
        tokens.push_back(std::move(token));
        if (isRawTextTag)
        {
            const size_t closeTagStart = html.find(rawTextCloseTag, cursor);
            cursor = closeTagStart == std::string::npos ? html.size() : closeTagStart;
        }
    }

    return tokens;
}

std::string NormalizeWikiHref(std::string href)
{
    href = DecodeHtmlEntities(TrimCopy(href));
    if (href.empty())
    {
        return {};
    }

    if (href.rfind("//", 0) == 0)
    {
        return "https:" + href;
    }

    if (href[0] == '/')
    {
        return "https://wiki.guildwars2.com" + href;
    }

    return href;
}

struct HtmlParseState
{
    std::vector<HtmlBlock> Blocks;
    HtmlBlock Paragraph;
    bool HasParagraph = false;
    HtmlBlock Heading;
    bool HasHeading = false;
    HtmlBlock List;
    bool HasList = false;
    std::vector<HtmlInline> ListItem;
    bool HasListItem = false;
    HtmlBlock Table;
    bool HasTable = false;
    std::vector<HtmlTableCell> TableRow;
    bool HasTableRow = false;
    HtmlTableCell TableCell;
    bool HasTableCell = false;
    std::vector<std::string> LinkStack;
    int IgnoreDepth = 0;
};

std::string PlainTextFromInlines(const std::vector<HtmlInline>& inlines);
ParsedPage ParseRenderedTokens(const std::vector<HtmlToken>& tokens, size_t beginIndex, size_t endIndex);

void FlushParagraph(HtmlParseState& state)
{
    if (!state.HasParagraph || state.Paragraph.Inlines.empty())
    {
        state.Paragraph = HtmlBlock{};
        state.HasParagraph = false;
        return;
    }

    state.Paragraph.Type = HtmlBlockType::Paragraph;
    state.Blocks.push_back(std::move(state.Paragraph));
    state.Paragraph = HtmlBlock{};
    state.HasParagraph = false;
}

void FlushHeading(HtmlParseState& state)
{
    if (!state.HasHeading || state.Heading.Inlines.empty())
    {
        state.Heading = HtmlBlock{};
        state.HasHeading = false;
        return;
    }

    state.Heading.Type = HtmlBlockType::Heading;
    state.Blocks.push_back(std::move(state.Heading));
    state.Heading = HtmlBlock{};
    state.HasHeading = false;
}

void FlushListItem(HtmlParseState& state)
{
    if (!state.HasListItem || state.ListItem.empty())
    {
        state.ListItem.clear();
        state.HasListItem = false;
        return;
    }

    if (!state.HasList)
    {
        state.List = HtmlBlock{};
        state.List.Type = HtmlBlockType::List;
        state.HasList = true;
    }

    state.List.ListItems.push_back(std::move(state.ListItem));
    state.ListItem = {};
    state.HasListItem = false;
}

void FlushList(HtmlParseState& state)
{
    FlushListItem(state);
    if (!state.HasList || state.List.ListItems.empty())
    {
        state.List = HtmlBlock{};
        state.HasList = false;
        return;
    }

    state.List.Type = HtmlBlockType::List;
    state.Blocks.push_back(std::move(state.List));
    state.List = HtmlBlock{};
    state.HasList = false;
}

void FlushTableCell(HtmlParseState& state)
{
    if (!state.HasTableCell)
    {
        return;
    }

    state.TableRow.push_back(std::move(state.TableCell));
    state.TableCell = HtmlTableCell{};
    state.HasTableCell = false;
}

void FlushTableRow(HtmlParseState& state)
{
    FlushTableCell(state);
    if (!state.HasTableRow || state.TableRow.empty())
    {
        state.TableRow.clear();
        state.HasTableRow = false;
        return;
    }

    if (!state.HasTable)
    {
        state.Table = HtmlBlock{};
        state.Table.Type = HtmlBlockType::Table;
        state.HasTable = true;
    }

    state.Table.TableRows.push_back(std::move(state.TableRow));
    state.TableRow = {};
    state.HasTableRow = false;
}

void FlushTable(HtmlParseState& state)
{
    FlushTableRow(state);
    if (!state.HasTable || state.Table.TableRows.empty())
    {
        state.Table = HtmlBlock{};
        state.HasTable = false;
        return;
    }

    state.Table.Type = HtmlBlockType::Table;
    state.Blocks.push_back(std::move(state.Table));
    state.Table = HtmlBlock{};
    state.HasTable = false;
}

std::vector<HtmlInline>& ActiveInlineTarget(HtmlParseState& state)
{
    if (state.HasTableCell)
    {
        return state.TableCell.Inlines;
    }
    if (state.HasHeading)
    {
        return state.Heading.Inlines;
    }
    if (state.HasListItem)
    {
        return state.ListItem;
    }
    if (!state.HasParagraph)
    {
        state.Paragraph = HtmlBlock{};
        state.Paragraph.Type = HtmlBlockType::Paragraph;
        state.HasParagraph = true;
    }
    return state.Paragraph.Inlines;
}

void AppendLiteralInline(std::vector<HtmlInline>& target, std::string text, const std::string& href = {})
{
    if (text.empty())
    {
        return;
    }

    HtmlInline item;
    item.Type = href.empty() ? HtmlInlineType::Text : HtmlInlineType::Link;
    item.Text = std::move(text);
    item.Href = href;
    target.push_back(std::move(item));
}

void AppendTextInline(std::vector<HtmlInline>& target, const std::string& text, const std::string& href)
{
    const std::string normalized = DecodeAndCollapseHtmlText(text);
    if (normalized.empty())
    {
        return;
    }

    if (!target.empty() &&
        target.back().Type == (href.empty() ? HtmlInlineType::Text : HtmlInlineType::Link) &&
        target.back().Href == href)
    {
        target.back().Text += " " + normalized;
        return;
    }

    HtmlInline item;
    item.Type = href.empty() ? HtmlInlineType::Text : HtmlInlineType::Link;
    item.Text = normalized;
    item.Href = href;
    target.push_back(std::move(item));
}

void AppendImageInline(std::vector<HtmlInline>& target, const std::unordered_map<std::string, std::string>& attributes)
{
    const std::string source = NormalizeWikiHref(AttributeOrEmpty(attributes, "src"));
    if (source.empty())
    {
        return;
    }

    HtmlInline item;
    item.Type = HtmlInlineType::Image;
    item.ImageSource = source;
    item.ImageAlt = TrimCopy(DecodeHtmlEntities(AttributeOrEmpty(attributes, "alt")));
    item.ImageWidth = ParseIntOrFallback(AttributeOrEmpty(attributes, "width"), 20);
    item.ImageHeight = ParseIntOrFallback(AttributeOrEmpty(attributes, "height"), 20);
    target.push_back(std::move(item));
}

bool BlockHasRenderableContent(const HtmlBlock& block)
{
    if (!block.Inlines.empty() || !block.ExtraInlines.empty() || !block.TableRows.empty() || !block.ChildBlocks.empty())
    {
        return true;
    }

    for (const auto& item : block.ListItems)
    {
        if (!item.empty())
        {
            return true;
        }
    }

    return false;
}

size_t FindMatchingEndTag(const std::vector<HtmlToken>& tokens, size_t startIndex, size_t endIndex)
{
    if (startIndex >= endIndex || startIndex >= tokens.size())
    {
        return endIndex;
    }

    const HtmlToken& start = tokens[startIndex];
    if (start.TokenType != HtmlToken::Type::StartTag || start.SelfClosing)
    {
        return startIndex;
    }

    int depth = 0;
    for (size_t index = startIndex; index < endIndex && index < tokens.size(); ++index)
    {
        const HtmlToken& token = tokens[index];
        if (token.Name != start.Name)
        {
            continue;
        }

        if (token.TokenType == HtmlToken::Type::StartTag && !token.SelfClosing)
        {
            depth += 1;
        }
        else if (token.TokenType == HtmlToken::Type::EndTag)
        {
            depth -= 1;
            if (depth == 0)
            {
                return index;
            }
        }
    }

    return endIndex;
}

void AppendInlineVector(std::vector<HtmlInline>& target, const std::vector<HtmlInline>& source)
{
    for (const auto& item : source)
    {
        if (item.Type == HtmlInlineType::Image)
        {
            target.push_back(item);
        }
        else
        {
            AppendLiteralInline(target, item.Text, item.Href);
        }
    }
}

std::vector<HtmlInline> FlattenBlocksToInlines(const std::vector<HtmlBlock>& blocks)
{
    std::vector<HtmlInline> flattened;
    bool needsBreak = false;

    for (const auto& block : blocks)
    {
        if (!BlockHasRenderableContent(block))
        {
            continue;
        }

        if (needsBreak)
        {
            AppendLiteralInline(flattened, "\n");
        }

        if (block.Type == HtmlBlockType::Paragraph || block.Type == HtmlBlockType::Heading)
        {
            AppendInlineVector(flattened, block.Inlines);
        }
        else if (block.Type == HtmlBlockType::List)
        {
            bool firstItem = true;
            for (const auto& item : block.ListItems)
            {
                if (!firstItem)
                {
                    AppendLiteralInline(flattened, "\n");
                }
                AppendLiteralInline(flattened, "• ");
                AppendInlineVector(flattened, item);
                firstItem = false;
            }
        }
        else if (block.Type == HtmlBlockType::Quote)
        {
            if (!block.Inlines.empty())
            {
                AppendInlineVector(flattened, block.Inlines);
            }
            else
            {
                AppendInlineVector(flattened, FlattenBlocksToInlines(block.ChildBlocks));
            }
        }

        needsBreak = true;
    }

    return flattened;
}

bool IsIgnorableQuoteParagraph(const HtmlBlock& block)
{
    if (block.Type != HtmlBlockType::Paragraph)
    {
        return false;
    }

    const std::string plain = TrimCopy(PlainTextFromInlines(block.Inlines));
    return plain.empty() || plain == "\"" || plain == "'" || plain == "“" || plain == "”";
}

bool LooksLikeQuoteAttribution(const HtmlBlock& block)
{
    if (block.Type != HtmlBlockType::Paragraph)
    {
        return false;
    }

    const std::string plain = TrimCopy(PlainTextFromInlines(block.Inlines));
    if (plain.empty())
    {
        return false;
    }

    if (plain.rfind("—", 0) == 0 || plain.rfind("-", 0) == 0)
    {
        return true;
    }

    const std::string normalized = Normalize(plain);
    return normalized.find("in game description") == 0;
}

std::vector<std::vector<HtmlTableCell>> ParseDefinitionListRows(const std::vector<HtmlToken>& tokens, size_t beginIndex, size_t endIndex)
{
    std::vector<std::vector<HtmlTableCell>> rows;
    std::vector<HtmlInline> pendingLabel;

    for (size_t index = beginIndex; index < endIndex && index < tokens.size(); ++index)
    {
        const HtmlToken& token = tokens[index];
        if (token.TokenType != HtmlToken::Type::StartTag || token.SelfClosing || (token.Name != "dt" && token.Name != "dd"))
        {
            continue;
        }

        const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
        ParsedPage inner = ParseRenderedTokens(tokens, index + 1, closeIndex);
        std::vector<HtmlInline> inlines = FlattenBlocksToInlines(inner.Blocks);

        if (token.Name == "dt")
        {
            pendingLabel = std::move(inlines);
        }
        else
        {
            HtmlTableCell label;
            label.Inlines = std::move(pendingLabel);

            HtmlTableCell value;
            value.Inlines = std::move(inlines);

            rows.push_back({ std::move(label), std::move(value) });
            pendingLabel.clear();
        }

        index = closeIndex;
    }

    return rows;
}

HtmlBlock ParseBlockquoteBlock(const std::vector<HtmlToken>& tokens, size_t startIndex, size_t endIndex)
{
    HtmlBlock block;
    block.Type = HtmlBlockType::Quote;

    ParsedPage inner = ParseRenderedTokens(tokens, startIndex + 1, endIndex);
    std::vector<HtmlBlock> contentBlocks;
    contentBlocks.reserve(inner.Blocks.size());

    for (auto& child : inner.Blocks)
    {
        if (IsIgnorableQuoteParagraph(child))
        {
            continue;
        }
        contentBlocks.push_back(std::move(child));
    }

    if (!contentBlocks.empty() && LooksLikeQuoteAttribution(contentBlocks.back()))
    {
        block.ExtraInlines = std::move(contentBlocks.back().Inlines);
        contentBlocks.pop_back();
    }

    if (contentBlocks.size() == 1 && contentBlocks.front().Type == HtmlBlockType::Paragraph)
    {
        block.Inlines = std::move(contentBlocks.front().Inlines);
    }
    else
    {
        block.ChildBlocks = std::move(contentBlocks);
    }

    return block;
}

HtmlBlock ParseInfoboxBlock(const std::vector<HtmlToken>& tokens, size_t startIndex, size_t endIndex)
{
    HtmlBlock block;
    block.Type = HtmlBlockType::Infobox;

    for (size_t index = startIndex + 1; index < endIndex && index < tokens.size(); ++index)
    {
        const HtmlToken& token = tokens[index];
        if (token.TokenType != HtmlToken::Type::StartTag || token.SelfClosing)
        {
            continue;
        }

        if (token.Name == "div" && ClassContains(token.Attributes, "infobox-icon"))
        {
            const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
            ParsedPage inner = ParseRenderedTokens(tokens, index + 1, closeIndex);
            block.ExtraInlines = FlattenBlocksToInlines(inner.Blocks);
            index = closeIndex;
            continue;
        }

        if (token.Name == "p" && ClassContains(token.Attributes, "heading"))
        {
            const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
            ParsedPage inner = ParseRenderedTokens(tokens, index + 1, closeIndex);
            block.Inlines = FlattenBlocksToInlines(inner.Blocks);
            index = closeIndex;
            continue;
        }

        if (token.Name == "dl")
        {
            const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
            std::vector<std::vector<HtmlTableCell>> rows = ParseDefinitionListRows(tokens, index + 1, closeIndex);
            for (auto& row : rows)
            {
                block.TableRows.push_back(std::move(row));
            }
            index = closeIndex;
            continue;
        }

        if (token.Name == "table" || token.Name == "p" || token.Name == "ul" || token.Name == "ol")
        {
            const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
            ParsedPage inner = ParseRenderedTokens(tokens, index, closeIndex + 1);
            for (auto& child : inner.Blocks)
            {
                if (BlockHasRenderableContent(child))
                {
                    block.ChildBlocks.push_back(std::move(child));
                }
            }
            index = closeIndex;
        }
    }

    return block;
}

ParsedPage ParseRenderedTokens(const std::vector<HtmlToken>& tokens, size_t beginIndex, size_t endIndex)
{
    HtmlParseState state;

    for (size_t index = beginIndex; index < endIndex && index < tokens.size(); ++index)
    {
        const auto& token = tokens[index];

        if (token.TokenType == HtmlToken::Type::Text)
        {
            if (state.IgnoreDepth > 0)
            {
                continue;
            }

            const std::string href = state.LinkStack.empty() ? "" : state.LinkStack.back();
            AppendTextInline(ActiveInlineTarget(state), token.Text, href);
            continue;
        }

        if (token.TokenType == HtmlToken::Type::StartTag)
        {
            if (state.IgnoreDepth > 0)
            {
                if (!token.SelfClosing)
                {
                    state.IgnoreDepth += 1;
                }
                continue;
            }

            if (IgnoreTagCompletely(token.Name, token.Attributes))
            {
                if (!token.SelfClosing)
                {
                    state.IgnoreDepth = 1;
                }
                continue;
            }

            if (token.Name == "a")
            {
                state.LinkStack.push_back(NormalizeWikiHref(AttributeOrEmpty(token.Attributes, "href")));
            }
            else if (token.Name == "blockquote")
            {
                FlushParagraph(state);
                FlushHeading(state);
                FlushList(state);
                FlushTable(state);

                const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
                HtmlBlock quote = ParseBlockquoteBlock(tokens, index, closeIndex);
                if (BlockHasRenderableContent(quote))
                {
                    state.Blocks.push_back(std::move(quote));
                }
                index = closeIndex;
            }
            else if (token.Name == "div" && ClassContains(token.Attributes, "infobox"))
            {
                FlushParagraph(state);
                FlushHeading(state);
                FlushList(state);
                FlushTable(state);

                const size_t closeIndex = FindMatchingEndTag(tokens, index, endIndex);
                HtmlBlock infobox = ParseInfoboxBlock(tokens, index, closeIndex);
                if (BlockHasRenderableContent(infobox))
                {
                    state.Blocks.push_back(std::move(infobox));
                }
                index = closeIndex;
            }
            else if (token.Name == "img")
            {
                AppendImageInline(ActiveInlineTarget(state), token.Attributes);
            }
            else if (token.Name == "br")
            {
                AppendLiteralInline(ActiveInlineTarget(state), "\n", state.LinkStack.empty() ? "" : state.LinkStack.back());
            }
            else if (token.Name == "hr")
            {
                FlushParagraph(state);
                FlushHeading(state);
                FlushList(state);
                FlushTable(state);
                HtmlBlock rule;
                rule.Type = HtmlBlockType::Rule;
                state.Blocks.push_back(std::move(rule));
            }
            else if (token.Name == "p")
            {
                FlushParagraph(state);
            }
            else if (token.Name == "h2" || token.Name == "h3" || token.Name == "h4" || token.Name == "h5" || token.Name == "h6")
            {
                FlushParagraph(state);
                FlushList(state);
                FlushTable(state);
                FlushHeading(state);
                state.Heading = HtmlBlock{};
                state.Heading.Type = HtmlBlockType::Heading;
                state.Heading.Level = ParseIntOrFallback(token.Name.substr(1), 2);
                state.HasHeading = true;
            }
            else if (token.Name == "ul" || token.Name == "ol")
            {
                FlushParagraph(state);
                if (!state.HasTableCell)
                {
                    FlushList(state);
                    state.List = HtmlBlock{};
                    state.List.Type = HtmlBlockType::List;
                    state.HasList = true;
                }
            }
            else if (token.Name == "li")
            {
                if (state.HasTableCell)
                {
                    AppendTextInline(state.TableCell.Inlines, "•", "");
                }
                else
                {
                    FlushListItem(state);
                    state.HasListItem = true;
                    state.ListItem = {};
                }
            }
            else if (token.Name == "table")
            {
                FlushParagraph(state);
                FlushList(state);
                FlushHeading(state);
                FlushTable(state);
                state.Table = HtmlBlock{};
                state.Table.Type = HtmlBlockType::Table;
                state.HasTable = true;
            }
            else if (token.Name == "tr")
            {
                if (state.HasTable)
                {
                    FlushTableRow(state);
                    state.HasTableRow = true;
                }
            }
            else if (token.Name == "th" || token.Name == "td")
            {
                if (state.HasTable)
                {
                    FlushTableCell(state);
                    state.TableCell = HtmlTableCell{};
                    state.TableCell.Header = (token.Name == "th");
                    state.HasTableCell = true;
                }
            }
            else if ((token.Name == "div" || token.Name == "dl") && !state.HasTableCell)
            {
                FlushParagraph(state);
            }

            continue;
        }

        if (state.IgnoreDepth > 0)
        {
            state.IgnoreDepth -= 1;
            continue;
        }

        if (token.Name == "a")
        {
            if (!state.LinkStack.empty())
            {
                state.LinkStack.pop_back();
            }
        }
        else if (token.Name == "p" || token.Name == "div" || token.Name == "dl")
        {
            FlushParagraph(state);
        }
        else if (token.Name == "h2" || token.Name == "h3" || token.Name == "h4" || token.Name == "h5" || token.Name == "h6")
        {
            FlushHeading(state);
        }
        else if (token.Name == "li")
        {
            if (!state.HasTableCell)
            {
                FlushListItem(state);
            }
        }
        else if (token.Name == "ul" || token.Name == "ol")
        {
            if (!state.HasTableCell)
            {
                FlushList(state);
            }
        }
        else if (token.Name == "th" || token.Name == "td")
        {
            FlushTableCell(state);
        }
        else if (token.Name == "tr")
        {
            FlushTableRow(state);
        }
        else if (token.Name == "table")
        {
            FlushTable(state);
        }
    }

    FlushParagraph(state);
    FlushHeading(state);
    FlushList(state);
    FlushTable(state);

    ParsedPage page;
    page.Blocks = std::move(state.Blocks);
    return page;
}

ParsedPage ParseRenderedHtml(const std::string& html)
{
    const std::vector<HtmlToken> tokens = TokenizeHtml(html);
    return ParseRenderedTokens(tokens, 0, tokens.size());
}

void EnsureRenderableBlocks(PageDocument& document)
{
    document.Parsed.Blocks = ParseRenderedHtml(document.Html).Blocks;
    if (!document.Parsed.Blocks.empty())
    {
        return;
    }

    const std::string fallbackText = TrimCopy(CollapseHtmlWhitespace(StripTags(document.Html)));
    if (fallbackText.empty())
    {
        return;
    }

    HtmlBlock paragraph;
    paragraph.Type = HtmlBlockType::Paragraph;

    HtmlInline text;
    text.Type = HtmlInlineType::Text;
    text.Text = fallbackText;
    paragraph.Inlines.push_back(std::move(text));

    document.Parsed.Blocks.push_back(std::move(paragraph));
}

std::wstring Utf8ToWide(const std::string& input)
{
    if (input.empty())
    {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (required <= 0)
    {
        return {};
    }

    std::wstring output(static_cast<size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()), output.data(), required);
    return output;
}

std::string UrlEncode(std::string_view value)
{
    std::ostringstream encoded;
    encoded.fill('0');

    for (unsigned char ch : value)
    {
        if (std::isalnum(ch) != 0 || ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            encoded << static_cast<char>(ch);
        }
        else
        {
            encoded << '%' << std::uppercase << std::hex << std::setw(2) << static_cast<int>(ch) << std::dec;
        }
    }

    return encoded.str();
}

std::string UrlDecode(std::string_view value)
{
    std::string output;
    output.reserve(value.size());

    for (size_t index = 0; index < value.size(); ++index)
    {
        if (value[index] == '%' && index + 2 < value.size())
        {
            const char hex[3] = { static_cast<char>(value[index + 1]), static_cast<char>(value[index + 2]), '\0' };
            char* end = nullptr;
            const long decoded = std::strtol(hex, &end, 16);
            if (end != nullptr && *end == '\0')
            {
                output.push_back(static_cast<char>(decoded));
                index += 2;
                continue;
            }
        }

        output.push_back(value[index] == '+' ? ' ' : value[index]);
    }

    return output;
}

unsigned long long Hash64(std::string_view value)
{
    unsigned long long hash = 1469598103934665603ull;
    for (unsigned char ch : value)
    {
        hash ^= static_cast<unsigned long long>(ch);
        hash *= 1099511628211ull;
    }

    return hash;
}

std::string ToHex(unsigned long long value)
{
    char buffer[17] = {};
    std::snprintf(buffer, sizeof(buffer), "%016llx", value);
    return buffer;
}

std::string SearchCachePath(const std::string& queryNormalized)
{
    return JoinPath(gState.SearchCacheDirectory, ToHex(Hash64(queryNormalized)) + ".json");
}

std::string CanonicalImageSource(std::string source)
{
    source = NormalizeWikiHref(std::move(source));
    const size_t fragment = source.find('#');
    if (fragment != std::string::npos)
    {
        source = source.substr(0, fragment);
    }

    return source;
}

std::string ImageFileExtension(std::string source)
{
    const size_t query = source.find('?');
    if (query != std::string::npos)
    {
        source = source.substr(0, query);
    }

    const size_t slash = source.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        source = source.substr(slash + 1);
    }

    const size_t dot = source.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= source.size())
    {
        return ".img";
    }

    std::string extension = LowerCopy(source.substr(dot));
    if (extension.size() > 10)
    {
        return ".img";
    }

    for (size_t index = 1; index < extension.size(); ++index)
    {
        if (std::isalnum(static_cast<unsigned char>(extension[index])) == 0)
        {
            return ".img";
        }
    }

    return extension;
}

std::string ImageCachePath(const std::string& source)
{
    const std::string canonical = CanonicalImageSource(source);
    return JoinPath(gState.ImageCacheDirectory, ToHex(Hash64(canonical)) + ImageFileExtension(canonical));
}

std::string ImageTempCachePath(const std::string& source, unsigned long long generation)
{
    const std::string canonical = CanonicalImageSource(source);
    return JoinPath(
        gState.ImageCacheDirectory,
        ToHex(Hash64(canonical)) + "." + ToHex(generation) + ImageFileExtension(canonical) + ".tmp");
}

std::string ImageTextureId(const std::string& canonicalSource, long long fileWriteUnix)
{
    const unsigned long long timestamp = fileWriteUnix > 0 ? static_cast<unsigned long long>(fileWriteUnix) : 0ull;
    return std::string(kAddonName) + ".Remote." + ToHex(Hash64(canonicalSource)) + "." + ToHex(timestamp);
}

std::string PendingImageTextureId(const std::string& canonicalSource, unsigned long long generation)
{
    return std::string(kAddonName) + ".Remote.Pending." + ToHex(Hash64(canonicalSource)) + "." + ToHex(generation);
}

std::string PageAliasPath(const std::string& title)
{
    return JoinPath(gState.PageCacheDirectory, ToHex(Hash64(Normalize(title))) + ".json");
}

std::string PageCachePath(int pageId, const std::string& title)
{
    if (pageId > 0)
    {
        return JoinPath(gState.PageCacheDirectory, std::to_string(pageId) + ".json");
    }

    return JoinPath(gState.PageCacheDirectory, ToHex(Hash64(Normalize(title))) + ".json");
}

const char* KeyLabel(unsigned short keyCode)
{
    for (const auto& option : kKeyOptions)
    {
        if (option.Vk == keyCode)
        {
            return option.Label;
        }
    }

    return "G";
}

int KeyOptionIndex(unsigned short keyCode)
{
    for (int index = 0; index < static_cast<int>(std::size(kKeyOptions)); ++index)
    {
        if (kKeyOptions[index].Vk == keyCode)
        {
            return index;
        }
    }

    return 6;
}

std::string FormatHotkey(const HotkeySettings& hotkey)
{
    std::string value;
    if (hotkey.Ctrl)
    {
        value += "CTRL+";
    }
    if (hotkey.Alt)
    {
        value += "ALT+";
    }
    if (hotkey.Shift)
    {
        value += "SHIFT+";
    }

    value += KeyLabel(hotkey.Key);
    return value;
}

bool ParseBoolSetting(std::string value, bool fallback = false)
{
    value = Normalize(value);
    if (value == "1" || value == "true" || value == "yes" || value == "on")
    {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" || value == "off")
    {
        return false;
    }

    return fallback;
}

void ResetArticleSectionStates()
{
    for (auto& tab : gState.Tabs)
    {
        tab.CollapsedSections.clear();
    }
}

HotkeySettings ParseHotkey(std::string value)
{
    HotkeySettings hotkey{};
    hotkey.Ctrl = false;
    hotkey.Alt = false;
    hotkey.Shift = false;
    hotkey.Key = 'G';

    value = Normalize(value);
    std::istringstream stream(value);
    std::string token;
    while (stream >> token)
    {
        if (token == "ctrl")
        {
            hotkey.Ctrl = true;
            continue;
        }
        if (token == "alt")
        {
            hotkey.Alt = true;
            continue;
        }
        if (token == "shift")
        {
            hotkey.Shift = true;
            continue;
        }

        std::string upper = token;
        std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });

        for (const auto& option : kKeyOptions)
        {
            if (upper == option.Label)
            {
                hotkey.Key = option.Vk;
                break;
            }
        }
    }

    return hotkey;
}

bool CopyToClipboard(const std::string& value)
{
    if (value.empty())
    {
        return false;
    }

    if (!OpenClipboard(nullptr))
    {
        return false;
    }

    EmptyClipboard();

    const size_t size = value.size() + 1;
    HGLOBAL memoryHandle = GlobalAlloc(GMEM_MOVEABLE, size);
    if (memoryHandle == nullptr)
    {
        CloseClipboard();
        return false;
    }

    void* target = GlobalLock(memoryHandle);
    if (target == nullptr)
    {
        GlobalFree(memoryHandle);
        CloseClipboard();
        return false;
    }

    std::memcpy(target, value.c_str(), size);
    GlobalUnlock(memoryHandle);

    if (SetClipboardData(CF_TEXT, memoryHandle) == nullptr)
    {
        GlobalFree(memoryHandle);
        CloseClipboard();
        return false;
    }

    CloseClipboard();
    return true;
}

bool OpenExternalUrl(const std::string& url)
{
    if (url.empty())
    {
        return false;
    }

    const HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(result) > 32;
}

void SaveSettings()
{
    if (gState.SettingsPath.empty())
    {
        return;
    }

    std::ostringstream out;
    out << "window_visible=" << (gState.WindowVisible ? "1" : "0") << "\n";
    out << "hotkey=" << FormatHotkey(gState.UserSettings.Hotkey) << "\n";
    out << "collapse_contents_by_default=" << (gState.UserSettings.CollapseContentsByDefault ? "1" : "0") << "\n";
    out << "collapse_notes_by_default=" << (gState.UserSettings.CollapseNotesByDefault ? "1" : "0") << "\n";
    out << "collapse_trivia_by_default=" << (gState.UserSettings.CollapseTriviaByDefault ? "1" : "0") << "\n";
    out << "collapse_gallery_by_default=" << (gState.UserSettings.CollapseGalleryByDefault ? "1" : "0") << "\n";
    out << "collapse_history_by_default=" << (gState.UserSettings.CollapseHistoryByDefault ? "1" : "0") << "\n";
    out << "collapse_see_also_by_default=" << (gState.UserSettings.CollapseSeeAlsoByDefault ? "1" : "0") << "\n";
    out << "collapse_references_by_default=" << (gState.UserSettings.CollapseReferencesByDefault ? "1" : "0") << "\n";
    out << "collapse_external_links_by_default=" << (gState.UserSettings.CollapseExternalLinksByDefault ? "1" : "0") << "\n";
    WriteTextFile(gState.SettingsPath, out.str());
}

void LoadSettings()
{
    gState.UserSettings = Settings{};

    const std::string raw = ReadTextFile(gState.SettingsPath);
    if (raw.empty())
    {
        gState.WindowVisible = gState.UserSettings.WindowVisible;
        return;
    }

    std::istringstream stream(raw);
    std::string line;
    while (std::getline(stream, line))
    {
        const auto separator = line.find('=');
        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string key = TrimCopy(line.substr(0, separator));
        const std::string value = TrimCopy(line.substr(separator + 1));

        if (key == "window_visible")
        {
            gState.UserSettings.WindowVisible = (value == "1" || value == "true");
        }
        else if (key == "hotkey")
        {
            gState.UserSettings.Hotkey = ParseHotkey(value);
        }
        else if (key == "collapse_contents_by_default")
        {
            gState.UserSettings.CollapseContentsByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_notes_by_default")
        {
            gState.UserSettings.CollapseNotesByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_trivia_by_default")
        {
            gState.UserSettings.CollapseTriviaByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_gallery_by_default")
        {
            gState.UserSettings.CollapseGalleryByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_history_by_default")
        {
            gState.UserSettings.CollapseHistoryByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_see_also_by_default")
        {
            gState.UserSettings.CollapseSeeAlsoByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_references_by_default")
        {
            gState.UserSettings.CollapseReferencesByDefault = ParseBoolSetting(value);
        }
        else if (key == "collapse_external_links_by_default")
        {
            gState.UserSettings.CollapseExternalLinksByDefault = ParseBoolSetting(value);
        }
    }

    gState.WindowVisible = gState.UserSettings.WindowVisible;
}

json SerializeSavedPageEntry(const SavedPageEntry& entry)
{
    json payload;
    payload["pageId"] = entry.PageId;
    payload["title"] = entry.Title;
    payload["savedAt"] = entry.SavedAt;
    return payload;
}

SavedPageEntry ParseSavedPageEntry(const json& payload)
{
    SavedPageEntry entry;
    if (payload.contains("pageId"))
    {
        entry.PageId = payload.at("pageId").get<int>();
    }
    if (payload.contains("title"))
    {
        entry.Title = payload.at("title").get<std::string>();
    }
    if (payload.contains("savedAt"))
    {
        entry.SavedAt = payload.at("savedAt").get<long long>();
    }
    return entry;
}

void SaveLibrary()
{
    if (gState.LibraryPath.empty())
    {
        return;
    }

    // Recent/favorites stay deliberately small and human-readable so a broken entry can
    // be recovered without introducing a heavier persistence layer.
    json payload;
    payload["recent"] = json::array();
    payload["favorites"] = json::array();

    for (const auto& entry : gState.RecentPages)
    {
        payload["recent"].push_back(SerializeSavedPageEntry(entry));
    }

    for (const auto& entry : gState.FavoritePages)
    {
        payload["favorites"].push_back(SerializeSavedPageEntry(entry));
    }

    WriteTextFile(gState.LibraryPath, payload.dump(2));
}

void LoadLibrary()
{
    gState.RecentPages.clear();
    gState.FavoritePages.clear();

    const std::string raw = ReadTextFile(gState.LibraryPath);
    if (raw.empty())
    {
        return;
    }

    try
    {
        const json payload = json::parse(raw);
        if (payload.contains("recent") && payload.at("recent").is_array())
        {
            for (const auto& item : payload.at("recent"))
            {
                SavedPageEntry entry = ParseSavedPageEntry(item);
                if (!TrimCopy(entry.Title).empty())
                {
                    gState.RecentPages.push_back(std::move(entry));
                }
            }
        }

        if (payload.contains("favorites") && payload.at("favorites").is_array())
        {
            for (const auto& item : payload.at("favorites"))
            {
                SavedPageEntry entry = ParseSavedPageEntry(item);
                if (!TrimCopy(entry.Title).empty())
                {
                    gState.FavoritePages.push_back(std::move(entry));
                }
            }
        }
    }
    catch (...)
    {
        gState.RecentPages.clear();
        gState.FavoritePages.clear();
    }
}

void SetWindowVisible(bool visible)
{
    const bool changed = gState.WindowVisible != visible;
    gState.WindowVisible = visible;
    gState.UserSettings.WindowVisible = visible;

    if (changed && visible)
    {
        gState.FocusSearchOnOpen = true;
    }
}

void ToggleWindow()
{
    SetWindowVisible(!gState.WindowVisible);
}

void OnToggleHotkey(const char*, bool isRelease)
{
    if (!isRelease)
    {
        ToggleWindow();
    }
}

void RegisterHotkey()
{
    if (gState.Api == nullptr)
    {
        return;
    }

    gState.Api->InputBinds.Deregister(kToggleKeybindId);
    const std::string hotkey = FormatHotkey(gState.UserSettings.Hotkey);
    gState.Api->InputBinds.RegisterWithString(kToggleKeybindId, OnToggleHotkey, hotkey.c_str());
}

void PreparePaths()
{
    if (gState.Api == nullptr)
    {
        return;
    }

    const char* addonDir = gState.Api->Paths.GetAddonDirectory(kAddonName);
    gState.AddonDirectory = addonDir != nullptr ? addonDir : "";
    EnsureDirectory(gState.AddonDirectory);

    // The addon bootstraps its own support/cache tree so end users only need the DLL.
    gState.SettingsPath = JoinPath(gState.AddonDirectory, "settings.ini");
    gState.LibraryPath = JoinPath(gState.AddonDirectory, "library.json");
    gState.CacheRoot = JoinPath(gState.AddonDirectory, "cache");
    gState.SearchCacheDirectory = JoinPath(gState.CacheRoot, "search");
    gState.PageCacheDirectory = JoinPath(gState.CacheRoot, "pages");
    gState.ImageCacheDirectory = JoinPath(gState.CacheRoot, "images");

    EnsureDirectory(gState.CacheRoot);
    EnsureDirectory(gState.SearchCacheDirectory);
    EnsureDirectory(gState.PageCacheDirectory);
    EnsureDirectory(gState.ImageCacheDirectory);
}

bool DeleteFilesInDirectory(const std::string& directory)
{
    const std::string pattern = JoinPath(directory, "*");
    WIN32_FIND_DATAA findData{};
    HANDLE handle = FindFirstFileA(pattern.c_str(), &findData);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return true;
    }

    bool success = true;
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
            continue;
        }

        const std::string filePath = JoinPath(directory, findData.cFileName);
        if (!DeleteFileA(filePath.c_str()))
        {
            success = false;
        }
    } while (FindNextFileA(handle, &findData));

    FindClose(handle);
    return success;
}

bool HttpGetJson(const std::wstring& path, std::string& body, std::string& error)
{
    body.clear();

    HINTERNET session = WinHttpOpen(L"NexusGameWiki/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr)
    {
        error = "WinHTTP session could not be created.";
        return false;
    }

    WinHttpSetTimeouts(session, 4000, 4000, 8000, 10000);

    HINTERNET connection = WinHttpConnect(session, L"wiki.guildwars2.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (connection == nullptr)
    {
        error = "Could not connect to the GW2 wiki.";
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* acceptTypes[] = { L"application/json", nullptr };
    HINTERNET request = WinHttpOpenRequest(connection,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        acceptTypes,
        WINHTTP_FLAG_SECURE);

    if (request == nullptr)
    {
        error = "Could not create the wiki request.";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    bool ok = WinHttpSendRequest(request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0) != FALSE;

    if (ok)
    {
        ok = WinHttpReceiveResponse(request, nullptr) != FALSE;
    }

    if (!ok)
    {
        error = "The wiki request failed before a response was received.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        error = "The wiki response did not include a valid status code.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    if (statusCode != 200)
    {
        error = "The wiki returned HTTP " + std::to_string(statusCode) + ".";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD available = 0;
    do
    {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            error = "Could not read the wiki response stream.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        if (available == 0)
        {
            break;
        }

        std::string chunk(static_cast<size_t>(available), '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &downloaded))
        {
            error = "Could not download the wiki response body.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        chunk.resize(static_cast<size_t>(downloaded));
        body += chunk;
    } while (available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return true;
}

bool HttpGetBytesFromUrl(const std::string& url, std::string& body, std::string& error)
{
    body.clear();

    const std::wstring wideUrl = Utf8ToWide(url);
    if (wideUrl.empty())
    {
        error = "The image URL was empty or invalid.";
        return false;
    }

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), static_cast<DWORD>(wideUrl.size()), 0, &components))
    {
        error = "The image URL could not be parsed.";
        return false;
    }

    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path = components.dwUrlPathLength > 0
        ? std::wstring(components.lpszUrlPath, components.dwUrlPathLength)
        : L"/";
    if (components.dwExtraInfoLength > 0)
    {
        path.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }

    HINTERNET session = WinHttpOpen(L"NexusGameWiki/0.1",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (session == nullptr)
    {
        error = "WinHTTP session could not be created.";
        return false;
    }

    WinHttpSetTimeouts(session, 4000, 4000, 8000, 10000);

    HINTERNET connection = WinHttpConnect(session, host.c_str(), components.nPort, 0);
    if (connection == nullptr)
    {
        error = "Could not connect to the image host.";
        WinHttpCloseHandle(session);
        return false;
    }

    const wchar_t* acceptTypes[] = { L"*/*", nullptr };
    const DWORD flags = components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connection,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        acceptTypes,
        flags);

    if (request == nullptr)
    {
        error = "Could not create the image request.";
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    bool ok = WinHttpSendRequest(request,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0) != FALSE;

    if (ok)
    {
        ok = WinHttpReceiveResponse(request, nullptr) != FALSE;
    }

    if (!ok)
    {
        error = "The image request failed before a response was received.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    if (!WinHttpQueryHeaders(request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX))
    {
        error = "The image response did not include a valid status code.";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    if (statusCode != 200)
    {
        error = "The image host returned HTTP " + std::to_string(statusCode) + ".";
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        return false;
    }

    DWORD available = 0;
    do
    {
        available = 0;
        if (!WinHttpQueryDataAvailable(request, &available))
        {
            error = "Could not read the image response stream.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        if (available == 0)
        {
            break;
        }

        std::string chunk(static_cast<size_t>(available), '\0');
        DWORD downloaded = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &downloaded))
        {
            error = "Could not download the image response body.";
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return false;
        }

        chunk.resize(static_cast<size_t>(downloaded));
        body += chunk;
    } while (available > 0);

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return true;
}

bool LoadSearchCache(const std::string& queryNormalized, std::vector<SearchHit>& results, CacheMetadata* metadata = nullptr)
{
    results.clear();
    const std::string cachePath = SearchCachePath(queryNormalized);
    const std::string raw = ReadTextFile(cachePath);
    if (raw.empty())
    {
        return false;
    }

    try
    {
        const json payload = json::parse(raw);
        const int format = payload.value("format", 0);
        if (format < 2)
        {
            return false;
        }

        if (!payload.contains("queryNormalized") || payload.at("queryNormalized").get<std::string>() != queryNormalized)
        {
            return false;
        }

        if (!payload.contains("results") || !payload.at("results").is_array())
        {
            return false;
        }

        TouchJsonPayload(cachePath, payload);

        if (metadata != nullptr)
        {
            metadata->FetchedAt = payload.value("fetchedAt", 0ll);
            metadata->LastUsedAt = payload.value("lastUsedAt", 0ll);
        }

        for (const auto& node : payload.at("results"))
        {
            SearchHit hit;
            hit.PageId = node.value("pageId", -1);
            hit.Title = TrimCopy(node.value("title", ""));
            hit.Snippet = TrimCopy(node.value("snippet", ""));
            if (hit.PageId > 0 && !hit.Title.empty())
            {
                results.push_back(std::move(hit));
            }
        }

        return true;
    }
    catch (...)
    {
        return false;
    }
}

void SaveSearchCache(const std::string& queryNormalized, const std::vector<SearchHit>& results)
{
    json payload = json::object();
    payload["format"] = 3;
    payload["fetchedAt"] = UnixNow();
    payload["lastUsedAt"] = UnixNow();
    payload["queryNormalized"] = queryNormalized;
    payload["results"] = json::array();

    for (const auto& hit : results)
    {
        payload["results"].push_back({
            { "pageId", hit.PageId },
            { "title", hit.Title },
            { "snippet", hit.Snippet }
        });
    }

    WriteTextFile(SearchCachePath(queryNormalized), payload.dump());
}

bool LoadPageDocumentPayloadFromJson(const json& payload, PageDocument& document, CacheMetadata* metadata = nullptr)
{
    document = PageDocument{};

    const int format = payload.value("format", 0);
    if (format < 2)
    {
        return false;
    }

    if (metadata != nullptr)
    {
        metadata->FetchedAt = payload.value("fetchedAt", 0ll);
        metadata->LastUsedAt = payload.value("lastUsedAt", 0ll);
    }

    document.PageId = payload.value("pageId", -1);
    document.Title = TrimCopy(payload.value("title", ""));
    document.DisplayTitle = TrimCopy(StripTags(payload.value("displayTitle", document.Title)));
    document.Url = TrimCopy(payload.value("url", ""));
    document.Html = payload.value("html", "");

    if (payload.contains("sections") && payload.at("sections").is_array())
    {
        for (const auto& sectionNode : payload.at("sections"))
        {
            WikiSectionInfo section;
            section.Number = TrimCopy(sectionNode.value("number", ""));
            section.Index = TrimCopy(sectionNode.value("index", ""));
            section.Title = TrimCopy(StripTags(sectionNode.value("title", "")));
            section.Level = ParseIntOrFallback(sectionNode.value("level", "0"), 0);
            section.TocLevel = sectionNode.value("tocLevel", 0);
            if (!section.Title.empty())
            {
                document.Parsed.Sections.push_back(std::move(section));
            }
        }
    }

    EnsureRenderableBlocks(document);
    return document.PageId > 0 && !document.Title.empty() && !document.Html.empty();
}

bool LoadPageCache(int pageId, const std::string& title, PageDocument& document, CacheMetadata* metadata = nullptr)
{
    document = PageDocument{};
    const std::string requestedPath = pageId > 0 ? PageCachePath(pageId, title) : PageAliasPath(title);
    const std::string raw = ReadTextFile(requestedPath);
    if (raw.empty())
    {
        return false;
    }

    try
    {
        const json payload = json::parse(raw);
        const bool isAlias = payload.value("kind", "") == "alias";
        if (isAlias)
        {
            TouchJsonPayload(requestedPath, payload);

            const int aliasPageId = payload.value("pageId", -1);
            const std::string aliasTitle = TrimCopy(payload.value("title", title));
            if (aliasPageId <= 0)
            {
                return false;
            }

            const std::string canonicalPath = PageCachePath(aliasPageId, aliasTitle);
            const std::string canonicalRaw = ReadTextFile(canonicalPath);
            if (canonicalRaw.empty())
            {
                return false;
            }

            const json canonicalPayload = json::parse(canonicalRaw);
            TouchJsonPayload(canonicalPath, canonicalPayload);
            return LoadPageDocumentPayloadFromJson(canonicalPayload, document, metadata);
        }

        TouchJsonPayload(requestedPath, payload);
        return LoadPageDocumentPayloadFromJson(payload, document, metadata);
    }
    catch (...)
    {
        return false;
    }
}

void SavePageCache(const PageDocument& document)
{
    json payload = json::object();
    payload["format"] = 3;
    payload["fetchedAt"] = UnixNow();
    payload["lastUsedAt"] = UnixNow();
    payload["pageId"] = document.PageId;
    payload["title"] = document.Title;
    payload["displayTitle"] = document.DisplayTitle;
    payload["url"] = document.Url;
    payload["html"] = document.Html;
    payload["sections"] = json::array();

    for (const auto& section : document.Parsed.Sections)
    {
        payload["sections"].push_back({
            { "number", section.Number },
            { "index", section.Index },
            { "title", section.Title },
            { "level", std::to_string(section.Level) },
            { "tocLevel", section.TocLevel }
        });
    }

    const std::string serialized = payload.dump();
    WriteTextFile(PageCachePath(document.PageId, document.Title), serialized);
    if (!document.Title.empty())
    {
        json aliasPayload = json::object();
        aliasPayload["format"] = 4;
        aliasPayload["kind"] = "alias";
        aliasPayload["fetchedAt"] = payload["fetchedAt"];
        aliasPayload["lastUsedAt"] = payload["lastUsedAt"];
        aliasPayload["pageId"] = document.PageId;
        aliasPayload["title"] = document.Title;
        WriteTextFile(PageAliasPath(document.Title), aliasPayload.dump());
    }
}

bool FetchSearchOnline(const std::string& queryRaw, std::vector<SearchHit>& results, std::string& error)
{
    results.clear();
    const std::string pathUtf8 =
        "/api.php?action=query&list=search&srsearch=" + UrlEncode(queryRaw) +
        "&srlimit=" + std::to_string(kMaxSearchResults) +
        "&utf8=1&formatversion=2&format=json";

    std::string body;
    if (!HttpGetJson(Utf8ToWide(pathUtf8), body, error))
    {
        return false;
    }

    try
    {
        const json payload = json::parse(body);
        if (!payload.contains("query") || !payload.at("query").contains("search"))
        {
            error = "The wiki search response was missing search results.";
            return false;
        }

        for (const auto& node : payload.at("query").at("search"))
        {
            SearchHit hit;
            hit.PageId = node.value("pageid", -1);
            hit.Title = TrimCopy(node.value("title", ""));
            hit.Snippet = StripTags(node.value("snippet", ""));
            if (hit.PageId > 0 && !hit.Title.empty())
            {
                results.push_back(std::move(hit));
            }
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        error = std::string("The wiki search response could not be parsed: ") + exception.what();
        return false;
    }
}

bool FetchPrefixSearchOnline(const std::string& queryRaw, std::vector<SearchHit>& results, std::string& error)
{
    results.clear();
    const std::string pathUtf8 =
        "/api.php?action=query&list=prefixsearch&pssearch=" + UrlEncode(queryRaw) +
        "&pslimit=" + std::to_string(kMaxSearchResults) +
        "&formatversion=2&format=json";

    std::string body;
    if (!HttpGetJson(Utf8ToWide(pathUtf8), body, error))
    {
        return false;
    }

    try
    {
        const json payload = json::parse(body);
        if (!payload.contains("query") || !payload.at("query").contains("prefixsearch"))
        {
            error = "The wiki prefix search response was missing results.";
            return false;
        }

        for (const auto& node : payload.at("query").at("prefixsearch"))
        {
            SearchHit hit;
            hit.PageId = node.value("pageid", -1);
            hit.Title = TrimCopy(node.value("title", ""));
            hit.Snippet = "Title prefix match";
            if (hit.PageId > 0 && !hit.Title.empty())
            {
                results.push_back(std::move(hit));
            }
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        error = std::string("The wiki prefix search response could not be parsed: ") + exception.what();
        return false;
    }
}

bool FetchPageOnline(int pageId, const std::string& title, PageDocument& document, std::string& error)
{
    document = PageDocument{};

    std::string pathUtf8 =
        "/api.php?action=parse&prop=text|sections|displaytitle&formatversion=2&format=json";
    if (pageId > 0)
    {
        pathUtf8 += "&pageid=" + std::to_string(pageId);
    }
    else
    {
        pathUtf8 += "&page=" + UrlEncode(title);
    }

    std::string body;
    if (!HttpGetJson(Utf8ToWide(pathUtf8), body, error))
    {
        return false;
    }

    try
    {
        const json payload = json::parse(body);
        if (!payload.contains("parse"))
        {
            error = "The wiki page response did not include parsed content.";
            return false;
        }

        const auto& node = payload.at("parse");
        document.PageId = node.value("pageid", pageId);
        document.Title = TrimCopy(node.value("title", title));
        document.DisplayTitle = TrimCopy(StripTags(node.value("displaytitle", document.Title)));
        document.Url = "https://wiki.guildwars2.com/wiki/" + UrlEncode(document.Title);
        document.Html = node.value("text", "");

        if (node.contains("sections") && node.at("sections").is_array())
        {
            for (const auto& sectionNode : node.at("sections"))
            {
                WikiSectionInfo section;
                section.Number = TrimCopy(sectionNode.value("number", ""));
                section.Index = TrimCopy(sectionNode.value("index", ""));
                section.Title = TrimCopy(StripTags(sectionNode.value("line", "")));
                section.Level = ParseIntOrFallback(sectionNode.value("level", "0"), 0);
                section.TocLevel = sectionNode.value("toclevel", 0);
                if (!section.Title.empty())
                {
                    document.Parsed.Sections.push_back(std::move(section));
                }
            }
        }

        EnsureRenderableBlocks(document);

        if (document.PageId <= 0 || document.Title.empty() || document.Html.empty())
        {
            error = "The wiki page response did not include rendered article HTML.";
            return false;
        }

        return true;
    }
    catch (const std::exception& exception)
    {
        error = std::string("The wiki page response could not be parsed: ") + exception.what();
        return false;
    }
}

void SearchWorkerLoop()
{
    for (;;)
    {
        SearchJob job;
        {
            std::unique_lock<std::mutex> lock(gWorker.Mutex);
            gWorker.Cv.wait(lock, [] {
                return gWorker.Stop || gWorker.Search.Pending;
            });

            if (gWorker.Stop)
            {
                return;
            }

            job = gWorker.Search;
            gWorker.Search.Pending = false;
        }

        SearchFetchResult result;
        result.Ready = true;
        result.Token = job.Token;
        result.QueryNormalized = job.QueryNormalized;
        result.ForceRefresh = job.ForceRefresh;

        // Search is disk-first: cached results can be shown immediately while the main
        // thread decides whether a stale hit should be refreshed in the background.
        CacheMetadata searchCacheMetadata;
        if (!job.ForceRefresh && LoadSearchCache(job.QueryNormalized, result.Results, &searchCacheMetadata))
        {
            result.Success = true;
            result.FromCache = true;
            result.CacheFetchedAt = searchCacheMetadata.FetchedAt;
            result.CacheStale = IsCacheStale(searchCacheMetadata, kSearchCacheTtlSeconds);
        }
        else
        {
            std::vector<SearchHit> combinedResults;
            std::string searchError;
            const bool searchOk = FetchSearchOnline(job.QueryRaw, combinedResults, searchError);

            std::vector<SearchHit> prefixResults;
            std::string prefixError;
            bool prefixOk = false;
            for (const auto& candidate : BuildPrefixSearchCandidates(job.QueryRaw))
            {
                std::vector<SearchHit> candidateResults;
                std::string candidateError;
                const bool candidateOk = FetchPrefixSearchOnline(candidate, candidateResults, candidateError);
                prefixOk = prefixOk || candidateOk;
                if (!candidateOk)
                {
                    if (prefixError.empty())
                    {
                        prefixError = std::move(candidateError);
                    }
                    continue;
                }

                for (const auto& hit : candidateResults)
                {
                    prefixResults.push_back(hit);
                }
            }

            std::unordered_map<int, SearchHit> deduped;
            for (const auto& hit : combinedResults)
            {
                deduped[hit.PageId] = hit;
            }
            for (const auto& hit : prefixResults)
            {
                if (deduped.find(hit.PageId) == deduped.end())
                {
                    deduped[hit.PageId] = hit;
                }
            }

            result.Results.clear();
            result.Results.reserve(deduped.size());
            for (const auto& [_, hit] : deduped)
            {
                result.Results.push_back(hit);
            }

            const std::string query = Normalize(job.QueryRaw);
            const std::vector<std::string> queryWords = SplitWords(query);
            std::sort(result.Results.begin(), result.Results.end(), [&](const SearchHit& left, const SearchHit& right) {
                const std::string leftTitle = Normalize(left.Title);
                const std::string rightTitle = Normalize(right.Title);

                // Keep ranking explainable: exact match first, then prefix/contains, then
                // softer fuzzy tie-breakers so typo tolerance does not bury obvious results.
                const auto score = [&](const std::string& normalizedTitle) {
                    const std::vector<std::string> titleWords = SplitWords(normalizedTitle);
                    const bool allWordsPrefixMatch = !queryWords.empty() &&
                        std::all_of(queryWords.begin(), queryWords.end(), [&](const std::string& queryWord) {
                            return std::any_of(titleWords.begin(), titleWords.end(), [&](const std::string& titleWord) {
                                return titleWord.rfind(queryWord, 0) == 0;
                            });
                        });

                    if (normalizedTitle == query)
                    {
                        return 0;
                    }
                    if (normalizedTitle.rfind(query, 0) == 0)
                    {
                        return 1;
                    }
                    if (normalizedTitle.find(query) != std::string::npos)
                    {
                        return 2;
                    }
                    if (allWordsPrefixMatch)
                    {
                        return 3;
                    }
                    return 4;
                };

                const int leftScore = score(leftTitle);
                const int rightScore = score(rightTitle);
                if (leftScore != rightScore)
                {
                    return leftScore < rightScore;
                }

                const size_t leftPrefix = CommonPrefixLength(query, leftTitle);
                const size_t rightPrefix = CommonPrefixLength(query, rightTitle);
                if (leftPrefix != rightPrefix)
                {
                    return leftPrefix > rightPrefix;
                }

                const int leftDistance = EditDistance(query, leftTitle);
                const int rightDistance = EditDistance(query, rightTitle);
                if (leftDistance != rightDistance)
                {
                    return leftDistance < rightDistance;
                }

                const int leftLengthGap = std::abs(static_cast<int>(leftTitle.size()) - static_cast<int>(query.size()));
                const int rightLengthGap = std::abs(static_cast<int>(rightTitle.size()) - static_cast<int>(query.size()));
                if (leftLengthGap != rightLengthGap)
                {
                    return leftLengthGap < rightLengthGap;
                }

                if (left.Title != right.Title)
                {
                    return left.Title < right.Title;
                }

                return left.PageId < right.PageId;
            });

            if (result.Results.size() > static_cast<size_t>(kMaxSearchResults))
            {
                result.Results.resize(kMaxSearchResults);
            }

            result.Success = searchOk || prefixOk;
            result.FromCache = false;
            if (result.Success)
            {
                SaveSearchCache(job.QueryNormalized, result.Results);
            }
            else
            {
                result.Error = !searchError.empty() ? searchError : prefixError;
            }
        }

        {
            std::lock_guard<std::mutex> lock(gWorker.Mutex);
            gWorker.SearchResult = std::move(result);
        }
    }
}

void PageWorkerLoop()
{
    for (;;)
    {
        PageJob job;
        {
            std::unique_lock<std::mutex> lock(gWorker.Mutex);
            gWorker.Cv.wait(lock, [] {
                return gWorker.Stop || gWorker.Page.Pending;
            });

            if (gWorker.Stop)
            {
                return;
            }

            job = gWorker.Page;
            gWorker.Page.Pending = false;
        }

        PageFetchResult result;
        result.Ready = true;
        result.Token = job.Token;
        result.TabId = job.TabId;
        result.PageId = job.PageId;
        result.ForceRefresh = job.ForceRefresh;

        CacheMetadata pageCacheMetadata;
        if (!job.ForceRefresh && LoadPageCache(job.PageId, job.Title, result.Document, &pageCacheMetadata))
        {
            result.Success = true;
            result.FromCache = true;
            result.CacheFetchedAt = pageCacheMetadata.FetchedAt;
            result.CacheStale = IsCacheStale(pageCacheMetadata, kPageCacheTtlSeconds);
        }
        else
        {
            result.Success = FetchPageOnline(job.PageId, job.Title, result.Document, result.Error);
            result.FromCache = false;
            if (result.Success)
            {
                SavePageCache(result.Document);
            }
        }

        {
            std::lock_guard<std::mutex> lock(gWorker.Mutex);
            gWorker.PageResult = std::move(result);
        }
    }
}

void ImageWorkerLoop()
{
    for (;;)
    {
        ImageJob job;
        {
            std::unique_lock<std::mutex> lock(gWorker.Mutex);
            gWorker.Cv.wait(lock, [] {
                return gWorker.Stop || !gWorker.ImageJobs.empty();
            });

            if (gWorker.Stop)
            {
                return;
            }

            job = std::move(gWorker.ImageJobs.front());
            gWorker.ImageJobs.pop_front();
        }

        ImageFetchResult result;
        result.Generation = job.Generation;
        result.Source = job.Source;
        result.CachePath = job.CachePath;
        result.TempPath = job.TempPath;

        std::string body;
        result.Success = HttpGetBytesFromUrl(job.Source, body, result.Error);
        if (result.Success)
        {
            unsigned long long currentGeneration = 0;
            {
                std::lock_guard<std::mutex> lock(gWorker.Mutex);
                currentGeneration = gWorker.ImageGeneration;
            }

            if (job.Generation != currentGeneration)
            {
                DeleteFileIfExists(job.TempPath);
                continue;
            }
        }

        if (result.Success && !WriteBinaryFile(job.TempPath, body))
        {
            result.Success = false;
            result.Error = "The downloaded image could not be written to its temp file.";
        }

        {
            std::lock_guard<std::mutex> lock(gWorker.Mutex);
            gWorker.ImageResults.push_back(std::move(result));
        }
    }
}

void QueueImageDownload(const std::string& source, const std::string& cachePath)
{
    if (source.empty() || cachePath.empty())
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gWorker.Mutex);
        gWorker.ImageJobs.push_back(ImageJob{
            gWorker.ImageGeneration,
            source,
            cachePath,
            ImageTempCachePath(source, gWorker.ImageGeneration)
        });
    }

    gWorker.Cv.notify_all();
}

void StartWorkers()
{
    gWorker.Stop = false;
    gWorker.Search = SearchJob{};
    gWorker.SearchResult = SearchFetchResult{};
    gWorker.Page = PageJob{};
    gWorker.PageResult = PageFetchResult{};
    gWorker.ImageGeneration = 1;
    gWorker.ImageJobs.clear();
    gWorker.ImageResults.clear();
    gWorker.ImageThreads.clear();
    gWorker.SearchThread = std::thread(SearchWorkerLoop);
    gWorker.PageThread = std::thread(PageWorkerLoop);
    gWorker.ImageThreads.reserve(kImageWorkerCount);
    for (int index = 0; index < kImageWorkerCount; ++index)
    {
        gWorker.ImageThreads.emplace_back(ImageWorkerLoop);
    }
}

void StopWorkers()
{
    {
        std::lock_guard<std::mutex> lock(gWorker.Mutex);
        gWorker.Stop = true;
        gWorker.Search.Pending = false;
        gWorker.Page.Pending = false;
        gWorker.ImageJobs.clear();
    }

    gWorker.Cv.notify_all();

    if (gWorker.SearchThread.joinable())
    {
        gWorker.SearchThread.join();
    }
    if (gWorker.PageThread.joinable())
    {
        gWorker.PageThread.join();
    }
    for (auto& thread : gWorker.ImageThreads)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }
    gWorker.ImageThreads.clear();
}

bool SavedPageMatches(const SavedPageEntry& entry, int pageId, const std::string& title)
{
    if (pageId > 0 && entry.PageId > 0)
    {
        return entry.PageId == pageId;
    }

    return Normalize(entry.Title) == Normalize(title);
}

SavedPageEntry MakeSavedPageEntry(int pageId, const std::string& title)
{
    SavedPageEntry entry;
    entry.PageId = pageId;
    entry.Title = TrimCopy(title);
    entry.SavedAt = UnixNow();
    return entry;
}

bool ContainsFavoritePage(int pageId, const std::string& title)
{
    return std::any_of(gState.FavoritePages.begin(), gState.FavoritePages.end(), [&](const SavedPageEntry& entry) {
        return SavedPageMatches(entry, pageId, title);
    });
}

void RememberRecentPage(int pageId, const std::string& title)
{
    const std::string cleanedTitle = TrimCopy(title);
    if (cleanedTitle.empty())
    {
        return;
    }

    auto& recent = gState.RecentPages;
    recent.erase(std::remove_if(recent.begin(), recent.end(), [&](const SavedPageEntry& entry) {
        return SavedPageMatches(entry, pageId, cleanedTitle);
    }), recent.end());
    recent.insert(recent.begin(), MakeSavedPageEntry(pageId, cleanedTitle));
    if (recent.size() > kMaxRecentEntries)
    {
        recent.resize(kMaxRecentEntries);
    }

    SaveLibrary();
}

void ToggleFavoritePage(int pageId, const std::string& title)
{
    const std::string cleanedTitle = TrimCopy(title);
    if (cleanedTitle.empty())
    {
        return;
    }

    auto& favorites = gState.FavoritePages;
    auto iterator = std::find_if(favorites.begin(), favorites.end(), [&](const SavedPageEntry& entry) {
        return SavedPageMatches(entry, pageId, cleanedTitle);
    });
    if (iterator != favorites.end())
    {
        favorites.erase(iterator);
    }
    else
    {
        favorites.insert(favorites.begin(), MakeSavedPageEntry(pageId, cleanedTitle));
    }

    SaveLibrary();
}

ArticleTab* FindTabById(int tabId)
{
    for (auto& tab : gState.Tabs)
    {
        if (tab.Id == tabId)
        {
            return &tab;
        }
    }

    return nullptr;
}

ArticleTab* FindTabByPageId(int pageId)
{
    for (auto& tab : gState.Tabs)
    {
        if (tab.PageId == pageId && pageId > 0)
        {
            return &tab;
        }
    }

    return nullptr;
}

ArticleTab* FindTabByTitle(const std::string& title)
{
    const std::string normalized = Normalize(title);
    if (normalized.empty())
    {
        return nullptr;
    }

    for (auto& tab : gState.Tabs)
    {
        const std::string tabTitle = !tab.Document.Title.empty() ? tab.Document.Title : tab.Title;
        if (Normalize(tabTitle) == normalized)
        {
            return &tab;
        }
    }

    return nullptr;
}

ArticleTab* ActiveTab()
{
    if (gState.ActiveTabId != 0)
    {
        if (ArticleTab* tab = FindTabById(gState.ActiveTabId))
        {
            return tab;
        }
    }

    if (gState.Tabs.empty())
    {
        return nullptr;
    }

    gState.ActiveTabId = gState.Tabs.front().Id;
    return &gState.Tabs.front();
}

ArticleTab& CreateArticleTab(int pageId, const std::string& title)
{
    ArticleTab tab;
    tab.Id = ++gState.NextTabId;
    tab.PageId = pageId;
    tab.Title = title;
    tab.Status = "Waiting to load article...";
    gState.Tabs.push_back(std::move(tab));
    gState.ActiveTabId = gState.Tabs.back().Id;
    return gState.Tabs.back();
}

void RemoveTab(int tabId)
{
    if (tabId == 0)
    {
        return;
    }

    int fallbackTabId = 0;
    for (size_t index = 0; index < gState.Tabs.size(); ++index)
    {
        if (gState.Tabs[index].Id == tabId)
        {
            if (index > 0)
            {
                fallbackTabId = gState.Tabs[index - 1].Id;
            }
            else if (index + 1 < gState.Tabs.size())
            {
                fallbackTabId = gState.Tabs[index + 1].Id;
            }

            gState.Tabs.erase(gState.Tabs.begin() + static_cast<long long>(index));
            break;
        }
    }

    if (gState.ActiveTabId == tabId)
    {
        gState.ActiveTabId = fallbackTabId;
    }
}

void QueueSearch(bool forceRefresh)
{
    if (gState.Search.QueryNormalized.size() < 2)
    {
        return;
    }

    gState.Search.Loading = true;
    gState.Search.HadFailure = false;
    gState.Search.FromCache = false;
    gState.Search.CacheStale = false;
    gState.Search.Status = forceRefresh ? "Refreshing live wiki results..." : "Searching the GW2 wiki...";
    gState.Search.DispatchedToken = ++gState.NextSearchToken;

    {
        std::lock_guard<std::mutex> lock(gWorker.Mutex);
        gWorker.Search.Pending = true;
        gWorker.Search.ForceRefresh = forceRefresh;
        gWorker.Search.Token = gState.Search.DispatchedToken;
        gWorker.Search.QueryRaw = gState.Search.QueryRaw;
        gWorker.Search.QueryNormalized = gState.Search.QueryNormalized;
    }

    gWorker.Cv.notify_all();
}

void QueuePageLoad(int tabId, int pageId, const std::string& title, bool forceRefresh)
{
    ArticleTab* tab = FindTabById(tabId);
    if (tab == nullptr || title.empty())
    {
        return;
    }

    tab->PageId = pageId;
    tab->Title = title;
    tab->Loading = true;
    tab->HadFailure = false;
    tab->FromCache = false;
    tab->CacheStale = false;
    tab->Status = forceRefresh ? "Refreshing article from the wiki..." : "Loading article...";
    tab->DispatchedToken = ++gState.NextPageToken;

    {
        std::lock_guard<std::mutex> lock(gWorker.Mutex);
        gWorker.Page.Pending = true;
        gWorker.Page.ForceRefresh = forceRefresh;
        gWorker.Page.Token = tab->DispatchedToken;
        gWorker.Page.TabId = tabId;
        gWorker.Page.PageId = pageId;
        gWorker.Page.Title = title;
    }

    gWorker.Cv.notify_all();
}

void LoadPageIntoActiveTab(int pageId, const std::string& title, bool forceRefresh)
{
    RememberRecentPage(pageId, title);

    ArticleTab* tab = ActiveTab();
    if (tab == nullptr)
    {
        tab = &CreateArticleTab(pageId, title);
    }
    else
    {
        const bool samePage = (tab->PageId == pageId && Normalize(tab->Title) == Normalize(title));
        tab->PageId = pageId;
        tab->Title = title;
        tab->Document = PageDocument{};
        if (!samePage)
        {
            tab->CollapsedSections.clear();
            tab->SelectedSectionIndex.clear();
            tab->PendingScrollSectionIndex.clear();
        }
    }

    gState.ActiveTabId = tab->Id;
    QueuePageLoad(tab->Id, pageId, title, forceRefresh);
}

void OpenPageInNewTab(int pageId, const std::string& title, bool forceRefresh)
{
    RememberRecentPage(pageId, title);

    if (pageId > 0)
    {
        if (ArticleTab* existing = FindTabByPageId(pageId))
        {
            gState.ActiveTabId = existing->Id;
            if (forceRefresh || existing->Document.PageId <= 0)
            {
                QueuePageLoad(existing->Id, pageId, title.empty() ? existing->Title : title, forceRefresh);
            }
            return;
        }
    }
    else if (ArticleTab* existingByTitle = FindTabByTitle(title))
    {
        gState.ActiveTabId = existingByTitle->Id;
        if (forceRefresh || existingByTitle->Document.PageId <= 0)
        {
            QueuePageLoad(existingByTitle->Id, pageId, title.empty() ? existingByTitle->Title : title, forceRefresh);
        }
        return;
    }

    ArticleTab& tab = CreateArticleTab(pageId, title);
    QueuePageLoad(tab.Id, pageId, title, forceRefresh);
}

void PumpWorkerResults()
{
    SearchFetchResult searchResult;
    PageFetchResult pageResult;
    std::vector<ImageFetchResult> imageResults;
    unsigned long long imageGeneration = 0;

    {
        std::lock_guard<std::mutex> lock(gWorker.Mutex);
        imageGeneration = gWorker.ImageGeneration;
        if (gWorker.SearchResult.Ready)
        {
            searchResult = std::move(gWorker.SearchResult);
            gWorker.SearchResult = SearchFetchResult{};
        }

        if (gWorker.PageResult.Ready)
        {
            pageResult = std::move(gWorker.PageResult);
            gWorker.PageResult = PageFetchResult{};
        }

        while (!gWorker.ImageResults.empty())
        {
            imageResults.push_back(std::move(gWorker.ImageResults.front()));
            gWorker.ImageResults.pop_front();
        }
    }

    for (const auto& imageResult : imageResults)
    {
        if (imageResult.Generation != imageGeneration)
        {
            DeleteFileIfExists(imageResult.TempPath);
            continue;
        }

        const std::string canonicalSource = CanonicalImageSource(imageResult.Source);
        auto iterator = gState.RemoteImages.find(canonicalSource);
        if (iterator == gState.RemoteImages.end())
        {
            DeleteFileIfExists(imageResult.TempPath);
            continue;
        }

        RemoteImage& entry = iterator->second;
        entry.DownloadPending = false;

        if (imageResult.Success)
        {
            entry.PendingTempPath = imageResult.TempPath;
            entry.PendingTextureId = PendingImageTextureId(canonicalSource, imageResult.Generation);
            entry.PendingTexture = nullptr;
            entry.PendingValidationRequested = false;
            entry.PendingValidationStartedAt = std::chrono::steady_clock::time_point{};
        }
        else
        {
            DeleteFileIfExists(imageResult.TempPath);
            if (!imageResult.Error.empty())
            {
                Log(ELogLevel_WARNING, "Image cache download failed for " + canonicalSource + ": " + imageResult.Error);
            }
        }
    }

    if (searchResult.Ready && searchResult.Token == gState.Search.DispatchedToken && searchResult.QueryNormalized == gState.Search.QueryNormalized)
    {
        gState.Search.Loading = false;
        gState.Search.HadFailure = !searchResult.Success;

        if (!searchResult.Success)
        {
            if (!gState.Search.Results.empty())
            {
                gState.Search.FromCache = true;
                gState.Search.CacheStale = true;
                gState.Search.Status = searchResult.Error.empty()
                    ? "Live refresh failed. Showing cached search results."
                    : "Live refresh failed. Showing cached search results.";
            }
            else
            {
                gState.Search.FromCache = false;
                gState.Search.CacheStale = false;
                gState.Search.Status = searchResult.Error.empty() ? "The search request failed." : searchResult.Error;
                gState.Search.Results.clear();
                gState.Search.SelectedIndex = -1;
            }
        }
        else
        {
            gState.Search.LastLoadedQueryNormalized = searchResult.QueryNormalized;
            gState.Search.FromCache = searchResult.FromCache;
            gState.Search.CacheStale = searchResult.CacheStale;
            gState.Search.CacheFetchedAt = searchResult.CacheFetchedAt;
            gState.Search.Results = std::move(searchResult.Results);

            const std::string cacheAge = DescribeCacheAge(searchResult.CacheFetchedAt);

            if (gState.Search.Results.empty())
            {
                gState.Search.SelectedIndex = -1;
                if (searchResult.FromCache && searchResult.CacheStale && !searchResult.ForceRefresh)
                {
                    QueueSearch(true);
                    gState.Search.FromCache = true;
                    gState.Search.CacheStale = true;
                    gState.Search.CacheFetchedAt = searchResult.CacheFetchedAt;
                    gState.Search.Status = "No cached matches (" + cacheAge + "). Refreshing in background...";
                }
                else
                {
                    gState.Search.Status = searchResult.FromCache
                        ? "No cached wiki pages matched this query."
                        : "No wiki pages matched this query.";
                }
            }
            else
            {
                if (searchResult.FromCache && searchResult.CacheStale && !searchResult.ForceRefresh)
                {
                    QueueSearch(true);
                    gState.Search.FromCache = true;
                    gState.Search.CacheStale = true;
                    gState.Search.CacheFetchedAt = searchResult.CacheFetchedAt;
                    gState.Search.Status = "Showing cached search results (" + cacheAge + "). Refreshing in background...";
                }
                else if (searchResult.FromCache)
                {
                    gState.Search.Status = "Showing cached search results (" + cacheAge + ").";
                }
                else
                {
                    gState.Search.Status = "Showing live wiki results.";
                }

                int selectedIndex = -1;
                const int activePageId = ActiveTab() != nullptr ? ActiveTab()->PageId : -1;
                for (int index = 0; index < static_cast<int>(gState.Search.Results.size()); ++index)
                {
                    if (gState.Search.Results[index].PageId == activePageId)
                    {
                        selectedIndex = index;
                        break;
                    }
                }

                if (selectedIndex < 0)
                {
                    selectedIndex = 0;
                }

                gState.Search.SelectedIndex = selectedIndex;
            }
        }
    }

    if (pageResult.Ready)
    {
        if (ArticleTab* tab = FindTabById(pageResult.TabId))
        {
            if (pageResult.Token == tab->DispatchedToken)
            {
                tab->Loading = false;
                tab->HadFailure = !pageResult.Success;

                if (!pageResult.Success)
                {
                    if (tab->Document.PageId > 0 && !tab->Document.Html.empty())
                    {
                        tab->FromCache = true;
                        tab->CacheStale = true;
                        tab->Status = pageResult.Error.empty()
                            ? "Live refresh failed. Showing cached article."
                            : "Live refresh failed. Showing cached article.";
                    }
                    else
                    {
                        tab->FromCache = false;
                        tab->CacheStale = false;
                        tab->Status = pageResult.Error.empty() ? "The article request failed." : pageResult.Error;
                        tab->Document = PageDocument{};
                    }
                }
                else
                {
                    tab->FromCache = pageResult.FromCache;
                    tab->CacheStale = pageResult.CacheStale;
                    tab->CacheFetchedAt = pageResult.CacheFetchedAt;
                    tab->Document = std::move(pageResult.Document);
                    tab->Title = tab->Document.Title;
                    const std::string cacheAge = DescribeCacheAge(pageResult.CacheFetchedAt);
                    if (pageResult.FromCache && pageResult.CacheStale && !pageResult.ForceRefresh)
                    {
                        QueuePageLoad(tab->Id, tab->Document.PageId > 0 ? tab->Document.PageId : tab->PageId, tab->Title, true);
                        tab->FromCache = true;
                        tab->CacheStale = true;
                        tab->CacheFetchedAt = pageResult.CacheFetchedAt;
                        tab->Status = "Loaded cached article (" + cacheAge + "). Refreshing in background...";
                    }
                    else if (tab->FromCache)
                    {
                        tab->Status = "Loaded from local cache (" + cacheAge + ").";
                    }
                    else
                    {
                        tab->Status = "Loaded from the live wiki.";
                    }
                }
            }
        }
    }
}

void UpdateSearchFlow()
{
    PumpWorkerResults();

    const std::string raw = NormalizeSearchInput(std::string(gState.SearchBuffer));
    const std::string normalized = Normalize(raw);
    const auto now = std::chrono::steady_clock::now();

    const bool normalizedChanged = normalized != gState.Search.QueryNormalized;
    if (raw != gState.Search.QueryRaw || normalizedChanged)
    {
        if (!raw.empty())
        {
            gState.CurrentLeftPaneMode = LeftPaneMode::Search;
        }

        gState.Search.QueryRaw = raw;
        gState.Search.QueryNormalized = normalized;
        gState.Search.LastEditedAt = now;
        gState.Search.Status.clear();
        gState.Search.HadFailure = false;

        if (normalizedChanged)
        {
            gState.Search.Results.clear();
            gState.Search.SelectedIndex = -1;
            gState.Search.Loading = false;
            gState.Search.LastLoadedQueryNormalized.clear();
        }

        if (normalized.size() < 2)
        {
            gState.Search.Results.clear();
            gState.Search.SelectedIndex = -1;
            gState.Search.Loading = false;
        }
    }

    if (normalized.size() < 2)
    {
        return;
    }

    if (gState.Search.Loading)
    {
        return;
    }

    if (gState.Search.LastLoadedQueryNormalized == normalized)
    {
        return;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - gState.Search.LastEditedAt).count();
    if (elapsed >= kSearchDebounceMs)
    {
        QueueSearch(false);
    }
}

void EnsureIconShortcut()
{
    if (gState.Api == nullptr || gState.IconRegistered)
    {
        return;
    }

    // Register the embedded icon up front so Nexus never depends on a loose PNG beside
    // the DLL. The same texture id is reused for hover to keep the shortcut simple.
    gState.Api->Textures.GetOrCreateFromMemory(
        kIconTextureId,
        const_cast<unsigned char*>(kEmbeddedIconPng),
        sizeof(kEmbeddedIconPng));

    gState.Api->QuickAccess.Add(kQuickAccessId, kIconTextureId, kIconTextureId, kToggleKeybindId, "NexusGameWiki");
    gState.IconRegistered = true;
}

std::string PlainTextFromInlines(const std::vector<HtmlInline>& inlines)
{
    std::string output;
    for (const auto& inlineNode : inlines)
    {
        if (inlineNode.Type == HtmlInlineType::Image)
        {
            if (inlineNode.ImageAlt.empty())
            {
                continue;
            }

            if (!output.empty() && output.back() != ' ')
            {
                output.push_back(' ');
            }
            output += "[" + inlineNode.ImageAlt + "]";
            continue;
        }

        if (!inlineNode.Text.empty())
        {
            if (!output.empty() && output.back() != ' ')
            {
                output.push_back(' ');
            }
            output += inlineNode.Text;
        }
    }

    return TrimCopy(CollapseHtmlWhitespace(output));
}

std::string FileNameFromPath(std::string value)
{
    const size_t slash = value.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        value = value.substr(slash + 1);
    }

    return DecodeHtmlEntities(value);
}

Texture* GetRemoteTexture(const std::string& source)
{
    if (gState.Api == nullptr || source.empty())
    {
        return nullptr;
    }

    const std::string canonicalSource = CanonicalImageSource(source);
    if (canonicalSource.empty())
    {
        return nullptr;
    }

    auto& entry = gState.RemoteImages[canonicalSource];
    entry.CanonicalSource = canonicalSource;
    if (entry.CachePath.empty())
    {
        entry.CachePath = ImageCachePath(canonicalSource);
    }

    auto queueFreshDownload = [&](bool resetCooldown) {
        const auto now = std::chrono::steady_clock::now();
        const bool shouldRequest = entry.PendingTempPath.empty() &&
            !entry.DownloadPending &&
            (resetCooldown ||
                !entry.Requested ||
                std::chrono::duration_cast<std::chrono::seconds>(now - entry.LastRequestedAt).count() >= kImageRetryCooldownSeconds);
        if (!shouldRequest)
        {
            return false;
        }

        entry.DownloadPending = true;
        entry.Requested = true;
        entry.LastRequestedAt = now;
        entry.RequestAttempts += 1;
        QueueImageDownload(canonicalSource, entry.CachePath);
        return true;
    };

    auto tryStartTextureLoad = [&](const std::string& textureId, const std::string& path) {
        if (gState.ImageTextureLoadRequestsThisFrame >= kMaxImageTextureLoadRequestsPerFrame)
        {
            return false;
        }

        gState.Api->Textures.LoadFromFile(textureId.c_str(), path.c_str(), OnAsyncTextureLoaded);
        gState.ImageTextureLoadRequestsThisFrame += 1;
        return true;
    };

    if (!entry.PendingTempPath.empty())
    {
        const auto now = std::chrono::steady_clock::now();
        if (!entry.PendingValidationRequested)
        {
            if (tryStartTextureLoad(entry.PendingTextureId, entry.PendingTempPath))
            {
                entry.PendingValidationRequested = true;
                entry.PendingValidationStartedAt = now;
            }
            return nullptr;
        }

        if (Texture* existingPending = gState.Api->Textures.Get(entry.PendingTextureId.c_str()))
        {
            entry.PendingTexture = existingPending;
        }

        if (entry.PendingTexture != nullptr && entry.PendingTexture->Resource != nullptr)
        {
            if (ReplaceFileFromTemp(entry.PendingTempPath, entry.CachePath))
            {
                long long promotedWriteUnix = 0;
                TryGetFileLastWriteUnix(entry.CachePath, promotedWriteUnix);
                entry.LoadedTexture = entry.PendingTexture;
                entry.TextureId = entry.PendingTextureId;
                entry.LoadedFileWriteAt = promotedWriteUnix;
                entry.FileLoadRequested = false;
                entry.FileLoadStartedAt = std::chrono::steady_clock::time_point{};
                entry.PendingTempPath.clear();
                entry.PendingTextureId.clear();
                entry.PendingTexture = nullptr;
                entry.PendingValidationRequested = false;
                entry.PendingValidationStartedAt = std::chrono::steady_clock::time_point{};
                return entry.LoadedTexture;
            }
            else
            {
                Log(ELogLevel_WARNING, "Image cache promotion failed for " + canonicalSource + ".");
                DeleteFileIfExists(entry.PendingTempPath);
                entry.PendingTempPath.clear();
                entry.PendingTextureId.clear();
                entry.PendingTexture = nullptr;
                entry.PendingValidationRequested = false;
                entry.PendingValidationStartedAt = std::chrono::steady_clock::time_point{};
                queueFreshDownload(false);
            }
        }
        else if (entry.PendingValidationRequested &&
            std::chrono::duration_cast<std::chrono::seconds>(now - entry.PendingValidationStartedAt).count() >= kImageFileLoadTimeoutSeconds)
        {
            Log(ELogLevel_WARNING, "Discarding invalid staged image cache for " + canonicalSource + ".");
            DeleteFileIfExists(entry.PendingTempPath);
            entry.PendingTempPath.clear();
            entry.PendingTextureId.clear();
            entry.PendingTexture = nullptr;
            entry.PendingValidationRequested = false;
            entry.PendingValidationStartedAt = std::chrono::steady_clock::time_point{};
            entry.Requested = false;
            queueFreshDownload(true);
        }
    }

    if (entry.LoadedTexture != nullptr && entry.LoadedTexture->Resource != nullptr)
    {
        if (entry.LoadedFileWriteAt > 0 &&
            IsCacheStale(CacheMetadata{ entry.LoadedFileWriteAt, entry.LoadedFileWriteAt }, kImageCacheTtlSeconds))
        {
            queueFreshDownload(false);
        }

        return entry.LoadedTexture;
    }

    if (entry.DownloadPending && entry.LoadedFileWriteAt <= 0 && entry.PendingTempPath.empty())
    {
        return nullptr;
    }

    if (!entry.TextureId.empty())
    {
        if (Texture* existing = gState.Api->Textures.Get(entry.TextureId.c_str()))
        {
            entry.LoadedTexture = existing;
            if (existing->Resource != nullptr)
            {
                entry.FileLoadRequested = false;
                if (entry.LoadedFileWriteAt > 0 &&
                    IsCacheStale(CacheMetadata{ entry.LoadedFileWriteAt, entry.LoadedFileWriteAt }, kImageCacheTtlSeconds))
                {
                    queueFreshDownload(false);
                }

                return existing;
            }
        }
    }

    if (entry.LoadedFileWriteAt <= 0)
    {
        long long fileWriteUnix = 0;
        if (!TryGetFileLastWriteUnix(entry.CachePath, fileWriteUnix))
        {
            entry.LoadedTexture = nullptr;
            entry.TextureId.clear();
            entry.FileLoadRequested = false;
            entry.FileLoadStartedAt = std::chrono::steady_clock::time_point{};
            queueFreshDownload(false);
            return nullptr;
        }

        entry.LoadedFileWriteAt = fileWriteUnix;
        entry.TextureId = ImageTextureId(canonicalSource, fileWriteUnix);
        entry.LoadedTexture = nullptr;
        entry.FileLoadRequested = false;
        entry.FileLoadStartedAt = std::chrono::steady_clock::time_point{};
    }

    if (entry.TextureId.empty())
    {
        entry.TextureId = ImageTextureId(canonicalSource, entry.LoadedFileWriteAt);
    }

    if (!entry.FileLoadRequested)
    {
        if (tryStartTextureLoad(entry.TextureId, entry.CachePath))
        {
            entry.FileLoadRequested = true;
            entry.FileLoadStartedAt = std::chrono::steady_clock::now();
        }
        return nullptr;
    }

    if (entry.FileLoadRequested &&
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - entry.FileLoadStartedAt).count() >= kImageFileLoadTimeoutSeconds)
    {
        Log(ELogLevel_WARNING, "Discarding invalid cached image for " + canonicalSource + ".");
        DeleteFileIfExists(entry.CachePath);
        entry.LoadedTexture = nullptr;
        entry.LoadedFileWriteAt = 0;
        entry.TextureId.clear();
        entry.FileLoadRequested = false;
        entry.FileLoadStartedAt = std::chrono::steady_clock::time_point{};
        entry.Requested = false;
        queueFreshDownload(true);
        return nullptr;
    }

    return nullptr;
}

std::string ResolveWikiTitleFromHref(std::string href)
{
    href = NormalizeWikiHref(href);
    if (href.empty())
    {
        return {};
    }

    const size_t hash = href.find('#');
    if (hash != std::string::npos)
    {
        href = href.substr(0, hash);
    }

    const std::string prefix = "https://wiki.guildwars2.com/wiki/";
    if (href.rfind(prefix, 0) == 0)
    {
        std::string title = href.substr(prefix.size());
        title = ReplaceAllCopy(title, "_", " ");
        return DecodeHtmlEntities(UrlDecode(title));
    }

    const std::string indexPrefix = "https://wiki.guildwars2.com/index.php?title=";
    if (href.rfind(indexPrefix, 0) == 0)
    {
        std::string title = href.substr(indexPrefix.size());
        const size_t ampersand = title.find('&');
        if (ampersand != std::string::npos)
        {
            title = title.substr(0, ampersand);
        }
        title = ReplaceAllCopy(title, "_", " ");
        return DecodeHtmlEntities(UrlDecode(title));
    }

    return {};
}

bool RenderTextLink(const std::string& label)
{
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.29f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.30f, 0.38f, 0.65f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.52f, 0.77f, 0.97f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    const bool clicked = ImGui::SmallButton(label.c_str());
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(4);
    return clicked;
}

void HandleInlineLinkClick(const std::string& href)
{
    const std::string wikiTitle = ResolveWikiTitleFromHref(href);
    if (!wikiTitle.empty())
    {
        gState.PendingOpenPageId = -1;
        gState.PendingOpenTitle = wikiTitle;
        return;
    }

    if (!href.empty())
    {
        OpenExternalUrl(href);
    }
}

bool TryGetRarityColor(std::string_view piece, ImVec4& color)
{
    const std::string normalized = Normalize(piece);
    if (normalized == "junk")
    {
        color = ImVec4(0.62f, 0.62f, 0.62f, 1.0f);
        return true;
    }
    if (normalized == "basic")
    {
        color = ImVec4(0.90f, 0.90f, 0.90f, 1.0f);
        return true;
    }
    if (normalized == "fine")
    {
        color = ImVec4(0.40f, 0.68f, 0.96f, 1.0f);
        return true;
    }
    if (normalized == "masterwork")
    {
        color = ImVec4(0.47f, 0.82f, 0.45f, 1.0f);
        return true;
    }
    if (normalized == "rare")
    {
        color = ImVec4(0.96f, 0.84f, 0.36f, 1.0f);
        return true;
    }
    if (normalized == "exotic")
    {
        color = ImVec4(0.96f, 0.64f, 0.28f, 1.0f);
        return true;
    }
    if (normalized == "ascended")
    {
        color = ImVec4(0.97f, 0.36f, 0.60f, 1.0f);
        return true;
    }
    if (normalized == "legendary")
    {
        color = ImVec4(0.74f, 0.47f, 0.95f, 1.0f);
        return true;
    }

    return false;
}

void RenderWrappedTextPiece(const std::string& piece, bool* firstPiece, bool allowRarityColor)
{
    if (piece.empty())
    {
        return;
    }

    const ImVec2 pieceSize = ImGui::CalcTextSize(piece.c_str());
    const float availableX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (!*firstPiece)
    {
        const float nextWidth = ImGui::GetCursorScreenPos().x + pieceSize.x;
        if (nextWidth > availableX)
        {
            ImGui::NewLine();
        }
        else
        {
            ImGui::SameLine(0.0f, 3.0f);
        }
    }

    ImVec4 rarityColor{};
    if (allowRarityColor && TryGetRarityColor(piece, rarityColor))
    {
        ImGui::TextColored(rarityColor, "%s", piece.c_str());
    }
    else
    {
        ImGui::TextUnformatted(piece.c_str());
    }

    *firstPiece = false;
}

void RenderWrappedLinkPiece(const std::string& piece, const std::string& href, bool* firstPiece, int uniqueId)
{
    if (piece.empty())
    {
        return;
    }

    const ImVec2 pieceSize = ImGui::CalcTextSize(piece.c_str());
    const float availableX = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
    if (!*firstPiece)
    {
        const float nextWidth = ImGui::GetCursorScreenPos().x + pieceSize.x;
        if (nextWidth > availableX)
        {
            ImGui::NewLine();
        }
        else
        {
            ImGui::SameLine(0.0f, 3.0f);
        }
    }

    ImGui::PushID(uniqueId);
    if (RenderTextLink(piece))
    {
        HandleInlineLinkClick(href);
    }
    ImGui::PopID();

    *firstPiece = false;
}

std::vector<std::string> SplitTextPieces(const std::string& text)
{
    std::vector<std::string> pieces;
    std::string current;
    for (char ch : text)
    {
        current.push_back(ch);
        if (ch == ' ' || ch == '\n')
        {
            pieces.push_back(current);
            current.clear();
        }
    }

    if (!current.empty())
    {
        pieces.push_back(current);
    }

    return pieces;
}

void RenderInlineFlow(const std::vector<HtmlInline>& inlines, bool allowRarityColor = false)
{
    bool firstPiece = true;
    int uniqueId = 0;

    for (const auto& inlineNode : inlines)
    {
        if (inlineNode.Type == HtmlInlineType::Image)
        {
            if (!firstPiece)
            {
                ImGui::SameLine(0.0f, 4.0f);
            }

            Texture* texture = GetRemoteTexture(inlineNode.ImageSource);
            if (texture != nullptr && texture->Resource != nullptr)
            {
                const float width = static_cast<float>(std::max(12, inlineNode.ImageWidth));
                const float height = static_cast<float>(std::max(12, inlineNode.ImageHeight));
                ImGui::Image(texture->Resource, ImVec2(width, height));
            }
            else
            {
                const std::string fallback = inlineNode.ImageAlt.empty()
                    ? "[" + FileNameFromPath(inlineNode.ImageSource) + "]"
                    : "[" + inlineNode.ImageAlt + "]";
                ImGui::TextDisabled("%s", fallback.c_str());
            }

            firstPiece = false;
            continue;
        }

        const std::string text = inlineNode.Text;
        if (text.empty())
        {
            continue;
        }

        if (inlineNode.Type == HtmlInlineType::Link && !inlineNode.Href.empty())
        {
            RenderWrappedLinkPiece(text, inlineNode.Href, &firstPiece, uniqueId++);
        }
        else
        {
            const std::vector<std::string> pieces = SplitTextPieces(text);
            for (const auto& piece : pieces)
            {
                if (piece == "\n")
                {
                    ImGui::NewLine();
                    firstPiece = true;
                    continue;
                }

                RenderWrappedTextPiece(piece, &firstPiece, allowRarityColor);
            }
        }
    }
}

void RenderInlineSummary(const std::vector<HtmlInline>& inlines, bool wrapText, bool allowRarityColor = false)
{
    const std::string text = PlainTextFromInlines(inlines);
    if (text.empty() && std::none_of(inlines.begin(), inlines.end(), [](const HtmlInline& item) { return item.Type == HtmlInlineType::Image; }))
    {
        ImGui::TextDisabled("-");
        return;
    }

    if (wrapText)
    {
        RenderInlineFlow(inlines, allowRarityColor);
    }
    else
    {
        ImGui::TextUnformatted(text.c_str());
    }
}

bool EndsWithNormalizedWord(const std::string& normalizedTitle, const char* word)
{
    const std::string needle = Normalize(word);
    if (normalizedTitle.size() < needle.size())
    {
        return false;
    }

    if (normalizedTitle.compare(normalizedTitle.size() - needle.size(), needle.size(), needle) != 0)
    {
        return false;
    }

    return normalizedTitle.size() == needle.size() || normalizedTitle[normalizedTitle.size() - needle.size() - 1] == ' ';
}

bool ShouldCollapseSectionByDefault(const std::string& title)
{
    const std::string normalized = Normalize(title);
    if (normalized.empty())
    {
        return false;
    }

    if (normalized == "contents")
    {
        return gState.UserSettings.CollapseContentsByDefault;
    }
    if (normalized == "notes")
    {
        return gState.UserSettings.CollapseNotesByDefault;
    }
    if (normalized == "trivia")
    {
        return gState.UserSettings.CollapseTriviaByDefault;
    }
    if (normalized == "gallery")
    {
        return gState.UserSettings.CollapseGalleryByDefault;
    }
    if (normalized == "see also")
    {
        return gState.UserSettings.CollapseSeeAlsoByDefault;
    }
    if (normalized == "references")
    {
        return gState.UserSettings.CollapseReferencesByDefault;
    }
    if (normalized == "external links")
    {
        return gState.UserSettings.CollapseExternalLinksByDefault;
    }
    if (normalized == "history" || EndsWithNormalizedWord(normalized, "history"))
    {
        return gState.UserSettings.CollapseHistoryByDefault;
    }

    return false;
}

bool GetOrInitCollapsedState(ArticleTab& tab, const std::string& key, bool defaultCollapsed)
{
    const auto [iterator, inserted] = tab.CollapsedSections.emplace(key, defaultCollapsed);
    return iterator->second;
}

bool* GetCollapsedStatePtr(ArticleTab& tab, const std::string& key, bool defaultCollapsed)
{
    auto [iterator, inserted] = tab.CollapsedSections.emplace(key, defaultCollapsed);
    return &iterator->second;
}

std::string SectionStateKey(size_t sectionPosition, const std::string& title)
{
    return "section:" + std::to_string(sectionPosition) + ":" + Normalize(title);
}

void ExpandSectionPathForNavigation(ArticleTab& tab, const ParsedPage& parsed, const std::string& targetIndex)
{
    if (targetIndex.empty())
    {
        return;
    }

    int targetPosition = -1;
    for (int index = 0; index < static_cast<int>(parsed.Sections.size()); ++index)
    {
        if (parsed.Sections[index].Index == targetIndex)
        {
            targetPosition = index;
            break;
        }
    }

    if (targetPosition < 0)
    {
        return;
    }

    const WikiSectionInfo& target = parsed.Sections[static_cast<size_t>(targetPosition)];
    tab.CollapsedSections[SectionStateKey(static_cast<size_t>(targetPosition), target.Title)] = false;

    int currentTocLevel = target.TocLevel;
    for (int index = targetPosition - 1; index >= 0 && currentTocLevel > 1; --index)
    {
        const WikiSectionInfo& candidate = parsed.Sections[static_cast<size_t>(index)];
        if (candidate.TocLevel < currentTocLevel)
        {
            tab.CollapsedSections[SectionStateKey(static_cast<size_t>(index), candidate.Title)] = false;
            currentTocLevel = candidate.TocLevel;
        }
    }
}

struct GalleryItem
{
    HtmlInline Image;
    std::vector<HtmlInline> Caption;
};

bool TryGetFirstImageInline(const std::vector<HtmlInline>& inlines, HtmlInline& image)
{
    for (const auto& inlineNode : inlines)
    {
        if (inlineNode.Type == HtmlInlineType::Image)
        {
            image = inlineNode;
            return true;
        }
    }

    return false;
}

bool IsGalleryTableBlock(const HtmlBlock& block)
{
    if (block.Type != HtmlBlockType::Table || block.TableRows.size() < 2 || block.TableRows.front().size() != 1)
    {
        return false;
    }

    return block.TableRows.front().front().Header &&
        Normalize(PlainTextFromInlines(block.TableRows.front().front().Inlines)) == "gallery";
}

std::vector<GalleryItem> ExtractGalleryItems(const HtmlBlock& block)
{
    std::vector<GalleryItem> items;
    if (!IsGalleryTableBlock(block))
    {
        return items;
    }

    for (size_t rowIndex = 1; rowIndex < block.TableRows.size(); ++rowIndex)
    {
        for (const auto& cell : block.TableRows[rowIndex])
        {
            GalleryItem current;
            bool hasImage = false;

            for (const auto& inlineNode : cell.Inlines)
            {
                if (inlineNode.Type == HtmlInlineType::Image)
                {
                    if (hasImage)
                    {
                        items.push_back(std::move(current));
                        current = GalleryItem{};
                    }

                    current.Image = inlineNode;
                    hasImage = true;
                }
                else if (hasImage)
                {
                    current.Caption.push_back(inlineNode);
                }
            }

            if (hasImage)
            {
                items.push_back(std::move(current));
            }
        }
    }

    return items;
}

void RenderWikiImageInline(const HtmlInline& inlineNode, float maxWidth = 0.0f)
{
    const float sourceWidth = static_cast<float>(std::max(12, inlineNode.ImageWidth));
    const float sourceHeight = static_cast<float>(std::max(12, inlineNode.ImageHeight));

    float drawWidth = sourceWidth;
    float drawHeight = sourceHeight;
    if (maxWidth > 0.0f && drawWidth > maxWidth)
    {
        const float scale = maxWidth / drawWidth;
        drawWidth *= scale;
        drawHeight *= scale;
    }

    Texture* texture = GetRemoteTexture(inlineNode.ImageSource);
    if (texture != nullptr && texture->Resource != nullptr)
    {
        ImGui::Image(texture->Resource, ImVec2(drawWidth, drawHeight));
        return;
    }

    const std::string fallback = inlineNode.ImageAlt.empty()
        ? "[" + FileNameFromPath(inlineNode.ImageSource) + "]"
        : "[" + inlineNode.ImageAlt + "]";
    ImGui::TextDisabled("%s", fallback.c_str());
}

void RenderCenteredCaption(const std::vector<HtmlInline>& caption)
{
    const std::string text = PlainTextFromInlines(caption);
    if (text.empty())
    {
        return;
    }

    const float width = ImGui::CalcTextSize(text.c_str()).x;
    const float avail = ImGui::GetContentRegionAvail().x;
    if (width < avail)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - width) * 0.5f);
    }

    ImGui::TextDisabled("%s", text.c_str());
}

void RenderHtmlTableBlock(const HtmlBlock& block, int blockIndex)
{
    size_t columnCount = 0;
    for (const auto& row : block.TableRows)
    {
        columnCount = std::max(columnCount, row.size());
    }

    if (columnCount == 0)
    {
        return;
    }

    const bool hasHeaderRow = !block.TableRows.empty() &&
        !block.TableRows.front().empty() &&
        std::all_of(block.TableRows.front().begin(), block.TableRows.front().end(), [](const HtmlTableCell& cell) {
            return cell.Header;
        });

    ImGuiTableFlags flags = ImGuiTableFlags_Borders
        | ImGuiTableFlags_RowBg
        | ImGuiTableFlags_Resizable
        | ImGuiTableFlags_SizingFixedFit
        | ImGuiTableFlags_NoHostExtendX;

    const std::string tableId = "##WikiHtmlTable" + std::to_string(blockIndex);
    if (!ImGui::BeginTable(tableId.c_str(), static_cast<int>(columnCount), flags))
    {
        return;
    }

    if (hasHeaderRow)
    {
        const auto& headerRow = block.TableRows.front();
        for (size_t column = 0; column < columnCount; ++column)
        {
            const std::string title = column < headerRow.size()
                ? PlainTextFromInlines(headerRow[column].Inlines)
                : ("Column " + std::to_string(column + 1));
            ImGui::TableSetupColumn(title.empty() ? " " : title.c_str(), ImGuiTableColumnFlags_WidthFixed);
        }
        ImGui::TableHeadersRow();
    }
    else
    {
        for (size_t column = 0; column < columnCount; ++column)
        {
            ImGui::TableSetupColumn(("##WikiColumn" + std::to_string(column)).c_str(), ImGuiTableColumnFlags_WidthFixed);
        }
    }

    const size_t rowStart = hasHeaderRow ? 1 : 0;
    std::vector<bool> allowRarityColorByColumn(columnCount, false);
    if (hasHeaderRow)
    {
        const auto& headerRow = block.TableRows.front();
        for (size_t column = 0; column < columnCount; ++column)
        {
            const std::string title = column < headerRow.size()
                ? Normalize(PlainTextFromInlines(headerRow[column].Inlines))
                : "";
            allowRarityColorByColumn[column] = title == "rarity";
        }
    }

    for (size_t rowIndex = rowStart; rowIndex < block.TableRows.size(); ++rowIndex)
    {
        const auto& row = block.TableRows[rowIndex];
        ImGui::TableNextRow();

        for (size_t column = 0; column < columnCount; ++column)
        {
            ImGui::TableSetColumnIndex(static_cast<int>(column));
            if (column < row.size())
            {
                RenderInlineSummary(row[column].Inlines, true, allowRarityColorByColumn[column]);
            }
            else
            {
                ImGui::TextDisabled("-");
            }
        }
    }

    ImGui::EndTable();
}

void RenderHtmlBlock(const HtmlBlock& block, int& blockIndexCounter);
void RenderFlatBlocks(const std::vector<HtmlBlock>& blocks, int& blockIndexCounter);

void RenderQuoteBlock(const HtmlBlock& block, int uniqueId)
{
    ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    const std::string tableId = "##WikiQuoteBlock" + std::to_string(uniqueId);
    if (!ImGui::BeginTable(tableId.c_str(), 2, flags))
    {
        return;
    }

    ImGui::TableSetupColumn("##QuoteMark", ImGuiTableColumnFlags_WidthFixed, 26.0f);
    ImGui::TableSetupColumn("##QuoteBody", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 quotePos = ImGui::GetCursorScreenPos();
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize() * 2.35f,
            quotePos,
            ImGui::ColorConvertFloat4ToU32(ImVec4(0.72f, 0.72f, 0.74f, 0.95f)),
            "“");
        ImGui::Dummy(ImVec2(22.0f, ImGui::GetFontSize() * 1.85f));
    }

    ImGui::TableSetColumnIndex(1);
    if (!block.Inlines.empty())
    {
        RenderInlineSummary(block.Inlines, true);
    }
    else
    {
        int nestedCounter = 0;
        RenderFlatBlocks(block.ChildBlocks, nestedCounter);
    }

    if (!block.ExtraInlines.empty())
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.78f, 0.81f, 0.95f));
        RenderInlineSummary(block.ExtraInlines, true);
        ImGui::PopStyleColor();
    }

    ImGui::EndTable();
}

void RenderInfoboxBlock(const HtmlBlock& block, int& blockIndexCounter)
{
    const float cardWidth = std::min(380.0f, ImGui::GetContentRegionAvail().x);
    ImGuiTableFlags outerFlags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX;
    const std::string outerId = "##WikiInfoboxCard" + std::to_string(blockIndexCounter++);

    if (!ImGui::BeginTable(outerId.c_str(), 1, outerFlags, ImVec2(cardWidth, 0.0f)))
    {
        return;
    }

    ImGui::TableSetupColumn("##WikiInfoboxCardColumn", ImGuiTableColumnFlags_WidthFixed, cardWidth);

    HtmlInline iconInline;
    const bool hasIcon = TryGetFirstImageInline(block.ExtraInlines, iconInline);

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    {
        const std::string headerId = "##WikiInfoboxHeader" + std::to_string(blockIndexCounter++);
        if (ImGui::BeginTable(headerId.c_str(), hasIcon ? 2 : 1, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoHostExtendX))
        {
            ImGui::TableSetupColumn("##WikiInfoboxHeaderTitle", ImGuiTableColumnFlags_WidthStretch);
            if (hasIcon)
            {
                ImGui::TableSetupColumn("##WikiInfoboxHeaderIcon", ImGuiTableColumnFlags_WidthFixed, static_cast<float>(std::max(44, iconInline.ImageWidth + 4)));
            }

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.82f, 0.57f, 1.0f));
            if (!block.Inlines.empty())
            {
                RenderInlineSummary(block.Inlines, true);
            }
            ImGui::PopStyleColor();

            if (hasIcon)
            {
                ImGui::TableSetColumnIndex(1);
                RenderWikiImageInline(iconInline);
            }

            ImGui::EndTable();
        }
    }

    if (!block.TableRows.empty())
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        const std::string fieldsId = "##WikiInfoboxFields" + std::to_string(blockIndexCounter++);
        if (ImGui::BeginTable(fieldsId.c_str(), 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX))
        {
            ImGui::TableSetupColumn("##WikiInfoboxLabel", ImGuiTableColumnFlags_WidthFixed, 112.0f);
            ImGui::TableSetupColumn("##WikiInfoboxValue", ImGuiTableColumnFlags_WidthStretch);

            for (const auto& row : block.TableRows)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.83f, 0.94f, 1.0f));
                if (!row.empty())
                {
                    RenderInlineSummary(row.front().Inlines, true);
                }
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                if (row.size() > 1)
                {
                    RenderInlineSummary(row[1].Inlines, true);
                }
                else
                {
                    ImGui::TextDisabled("-");
                }
            }

            ImGui::EndTable();
        }
    }

    for (const auto& child : block.ChildBlocks)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);

        if (IsGalleryTableBlock(child))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.84f, 0.86f, 0.92f, 1.0f));
            ImGui::TextUnformatted("Gallery");
            ImGui::PopStyleColor();
            ImGui::Separator();

            const std::vector<GalleryItem> items = ExtractGalleryItems(child);
            for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
            {
                const GalleryItem& item = items[itemIndex];
                const float avail = ImGui::GetContentRegionAvail().x;
                const float imageWidth = static_cast<float>(std::max(12, item.Image.ImageWidth));
                const float drawWidth = std::min(imageWidth, std::max(48.0f, avail - 12.0f));
                const float offset = std::max(0.0f, (avail - drawWidth) * 0.5f);
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
                RenderWikiImageInline(item.Image, drawWidth);
                RenderCenteredCaption(item.Caption);

                if (itemIndex + 1 < items.size())
                {
                    ImGui::Spacing();
                }
            }
        }
        else
        {
            RenderHtmlBlock(child, blockIndexCounter);
        }
    }

    ImGui::EndTable();
}

void RenderHtmlBlock(const HtmlBlock& block, int& blockIndexCounter)
{
    if (block.Type == HtmlBlockType::Heading)
    {
        const std::string title = PlainTextFromInlines(block.Inlines);
        if (!title.empty())
        {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.82f, 0.52f, 1.0f));
            ImGui::TextWrapped("%s", title.c_str());
            ImGui::PopStyleColor();
            ImGui::Separator();
        }
    }
    else if (block.Type == HtmlBlockType::Paragraph)
    {
        RenderInlineSummary(block.Inlines, true);
        ImGui::Spacing();
    }
    else if (block.Type == HtmlBlockType::List)
    {
        for (const auto& item : block.ListItems)
        {
            ImGui::Bullet();
            ImGui::SameLine();
            RenderInlineSummary(item, true);
        }
        ImGui::Spacing();
    }
    else if (block.Type == HtmlBlockType::Table)
    {
        RenderHtmlTableBlock(block, blockIndexCounter++);
        ImGui::Spacing();
    }
    else if (block.Type == HtmlBlockType::Quote)
    {
        RenderQuoteBlock(block, blockIndexCounter++);
        ImGui::Spacing();
    }
    else if (block.Type == HtmlBlockType::Infobox)
    {
        RenderInfoboxBlock(block, blockIndexCounter);
        ImGui::Spacing();
    }
    else if (block.Type == HtmlBlockType::Rule)
    {
        ImGui::Separator();
    }
}

void RenderFlatBlocks(const std::vector<HtmlBlock>& blocks, int& blockIndexCounter)
{
    for (const auto& block : blocks)
    {
        RenderHtmlBlock(block, blockIndexCounter);
    }
}

float RenderSectionHeaderRow(const std::string& title, int level, bool* collapsed, const std::string& key)
{
    const float indent = static_cast<float>(std::max(0, level - 2)) * 12.0f;
    if (indent > 0.0f)
    {
        ImGui::Indent(indent);
    }

    ImGui::PushID(key.c_str());
    if (ImGui::SmallButton(*collapsed ? ">" : "v"))
    {
        *collapsed = !*collapsed;
    }
    ImGui::PopID();

    ImGui::SameLine(0.0f, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.93f, 0.82f, 0.52f, 1.0f));
    ImGui::TextWrapped("%s", title.c_str());
    const float headerAnchorY = std::max(0.0f, ImGui::GetItemRectMin().y - ImGui::GetWindowPos().y + ImGui::GetScrollY());
    ImGui::PopStyleColor();
    ImGui::Separator();

    if (indent > 0.0f)
    {
        ImGui::Unindent(indent);
    }

    return headerAnchorY;
}

bool RenderContentsEntryRow(const WikiSectionInfo& section, bool selected)
{
    const ImVec2 itemSize(0.0f, ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f);
    const bool clicked = ImGui::Selectable("##WikiContentsEntry", selected, ImGuiSelectableFlags_SpanAllColumns, itemSize);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float textY = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f;
    const float indent = static_cast<float>(std::max(0, section.TocLevel - 1)) * 14.0f;
    const ImVec2 textPos(min.x + ImGui::GetStyle().FramePadding.x + indent, textY);

    std::string line;
    if (!section.Number.empty())
    {
        line += section.Number;
        line += "  ";
    }
    line += section.Title;

    const ImVec4 textColor = selected
        ? ImVec4(0.98f, 0.97f, 0.92f, 1.0f)
        : ImVec4(0.90f, 0.90f, 0.92f, 1.0f);
    ImGui::GetWindowDrawList()->AddText(textPos, ImGui::ColorConvertFloat4ToU32(textColor), line.c_str());
    return clicked;
}

void RenderContentsSidebar(ArticleTab* tab)
{
    ImGui::BeginChild("##WikiContentsSidebar", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Contents");
    ImGui::Separator();

    if (tab == nullptr)
    {
        ImGui::TextDisabled("Pick a wiki result to load its article here.");
        ImGui::EndChild();
        return;
    }

    if (tab->Loading && tab->Document.Parsed.Sections.empty())
    {
        ImGui::TextDisabled("%s", tab->Status.c_str());
        ImGui::EndChild();
        return;
    }

    if (tab->Document.Parsed.Sections.empty())
    {
        ImGui::TextDisabled("No section outline for this page.");
        ImGui::EndChild();
        return;
    }

    for (size_t index = 0; index < tab->Document.Parsed.Sections.size(); ++index)
    {
        const WikiSectionInfo& section = tab->Document.Parsed.Sections[index];
        const bool selected = section.Index == tab->SelectedSectionIndex;
        ImGui::PushID(static_cast<int>(index));
        if (RenderContentsEntryRow(section, selected))
        {
            // The sidebar only retargets the active article tab and expands the ancestor
            // section path required to make the destination visible.
            tab->PendingScrollSectionIndex = section.Index;
            tab->SelectedSectionIndex = section.Index;
            ExpandSectionPathForNavigation(*tab, tab->Document.Parsed, section.Index);
        }
        if (index + 1 < tab->Document.Parsed.Sections.size())
        {
            ImGui::Separator();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

size_t RenderSectionBlocks(const ParsedPage& parsed, const std::vector<HtmlBlock>& blocks, size_t headingIndex, size_t& sectionCursor, ArticleTab& tab, int& blockIndexCounter)
{
    const HtmlBlock& heading = blocks[headingIndex];
    const size_t sectionPosition = sectionCursor;
    const WikiSectionInfo* sectionInfo = sectionPosition < parsed.Sections.size() ? &parsed.Sections[sectionPosition] : nullptr;

    const std::string title = sectionInfo != nullptr ? sectionInfo->Title : PlainTextFromInlines(heading.Inlines);
    const std::string sectionIndex = sectionInfo != nullptr ? sectionInfo->Index : "";
    const std::string key = SectionStateKey(sectionPosition, title);
    bool* collapsed = GetCollapsedStatePtr(tab, key, ShouldCollapseSectionByDefault(title));

    if (!tab.PendingScrollSectionIndex.empty() && sectionIndex == tab.PendingScrollSectionIndex)
    {
        *collapsed = false;
    }

    const float headerAnchorY = RenderSectionHeaderRow(title, heading.Level, collapsed, key + ":header");
    // Contents clicks queue a section id. When that exact header renders, resolve the
    // jump there and clear the request so repeated clicks stay idempotent.
    if (!tab.PendingScrollSectionIndex.empty() && sectionIndex == tab.PendingScrollSectionIndex)
    {
        ImGui::SetScrollY(headerAnchorY);
        tab.SelectedSectionIndex = sectionIndex;
        tab.PendingScrollSectionIndex.clear();
    }
    sectionCursor += 1;

    size_t index = headingIndex + 1;
    if (*collapsed)
    {
        while (index < blocks.size())
        {
            if (blocks[index].Type == HtmlBlockType::Heading && blocks[index].Level <= heading.Level)
            {
                break;
            }

            if (blocks[index].Type == HtmlBlockType::Heading)
            {
                sectionCursor += 1;
            }
            ++index;
        }

        ImGui::Spacing();
        return index;
    }

    const float contentIndent = static_cast<float>(std::max(0, heading.Level - 2)) * 12.0f + 10.0f;
    ImGui::Indent(contentIndent);
    while (index < blocks.size())
    {
        if (blocks[index].Type == HtmlBlockType::Heading)
        {
            if (blocks[index].Level <= heading.Level)
            {
                break;
            }

            index = RenderSectionBlocks(parsed, blocks, index, sectionCursor, tab, blockIndexCounter);
            continue;
        }

        RenderHtmlBlock(blocks[index], blockIndexCounter);
        ++index;
    }
    ImGui::Unindent(contentIndent);
    ImGui::Spacing();
    return index;
}

void RenderParsedBlocks(const ParsedPage& parsed, ArticleTab& tab)
{
    if (parsed.Blocks.empty())
    {
        ImGui::TextDisabled("This page did not produce any renderable wiki blocks.");
        return;
    }

    int blockIndexCounter = 0;
    size_t index = 0;
    size_t sectionCursor = 0;
    while (index < parsed.Blocks.size())
    {
        if (parsed.Blocks[index].Type == HtmlBlockType::Heading)
        {
            index = RenderSectionBlocks(parsed, parsed.Blocks, index, sectionCursor, tab, blockIndexCounter);
            continue;
        }

        RenderHtmlBlock(parsed.Blocks[index], blockIndexCounter);
        ++index;
    }
}

bool RenderSearchResultTitle(const SearchHit& hit, bool selected)
{
    const ImVec2 itemSize(0.0f, ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f);
    const bool clicked = ImGui::Selectable("##WikiSearchTitle", selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns, itemSize);

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float textY = min.y + (max.y - min.y - ImGui::GetTextLineHeight()) * 0.5f;
    const ImVec2 textPos(min.x + ImGui::GetStyle().FramePadding.x, textY);

    const ImVec4 titleColor = selected
        ? ImVec4(0.98f, 0.97f, 0.92f, 1.0f)
        : ImVec4(0.94f, 0.83f, 0.54f, 1.0f);
    const ImU32 color = ImGui::ColorConvertFloat4ToU32(titleColor);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddText(ImVec2(textPos.x + 0.9f, textPos.y), color, hit.Title.c_str());
    drawList->AddText(textPos, color, hit.Title.c_str());

    return clicked;
}

const char* LeftPaneModeLabel(LeftPaneMode mode)
{
    switch (mode)
    {
    case LeftPaneMode::Search:
        return "Search";
    case LeftPaneMode::Recent:
        return "Recent";
    case LeftPaneMode::Favorites:
        return "Favorites";
    }

    return "Search";
}

void RenderModeStrip()
{
    constexpr std::array<LeftPaneMode, 3> kModes = {
        LeftPaneMode::Search,
        LeftPaneMode::Recent,
        LeftPaneMode::Favorites
    };

    for (size_t index = 0; index < kModes.size(); ++index)
    {
        if (index > 0)
        {
            ImGui::SameLine();
        }

        const LeftPaneMode mode = kModes[index];
        const bool active = gState.CurrentLeftPaneMode == mode;
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.42f, 0.31f, 0.11f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.49f, 0.37f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.52f, 0.39f, 0.15f, 1.0f));
        }

        if (ImGui::Button(LeftPaneModeLabel(mode)))
        {
            gState.CurrentLeftPaneMode = mode;
        }

        if (active)
        {
            ImGui::PopStyleColor(3);
        }
    }
}

void RenderSearchResultsPane()
{
    ImGui::BeginChild("##WikiSearchResults", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Search Results");
    ImGui::Separator();

    const std::string normalized = Normalize(NormalizeSearchInput(std::string(gState.SearchBuffer)));
    if (normalized.size() < 2)
    {
        ImGui::TextDisabled("Type at least 2 characters to search the GW2 wiki.");
        ImGui::EndChild();
        return;
    }

    if (gState.Search.Loading)
    {
        ImGui::TextDisabled("%s", gState.Search.Status.c_str());
        ImGui::Spacing();
    }
    else if (!gState.Search.Status.empty())
    {
        if (gState.Search.HadFailure)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.45f, 0.34f, 1.0f));
            ImGui::TextWrapped("%s", gState.Search.Status.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::TextDisabled("%s", gState.Search.Status.c_str());
        }
        ImGui::Spacing();
    }

    if (gState.Search.Results.empty())
    {
        ImGui::TextDisabled("No cached or live results yet for this query.");
        ImGui::EndChild();
        return;
    }

    for (int index = 0; index < static_cast<int>(gState.Search.Results.size()); ++index)
    {
        const SearchHit& hit = gState.Search.Results[index];
        const bool selected = index == gState.Search.SelectedIndex;

        ImGui::PushID(index);
        if (RenderSearchResultTitle(hit, selected))
        {
            gState.Search.SelectedIndex = index;
            LoadPageIntoActiveTab(hit.PageId, hit.Title, false);
        }

        ImGui::TextDisabled("Page %d", hit.PageId);
        if (!hit.Snippet.empty())
        {
            ImGui::PushTextWrapPos();
            ImGui::TextUnformatted(hit.Snippet.c_str());
            ImGui::PopTextWrapPos();
        }

        if (index + 1 < static_cast<int>(gState.Search.Results.size()))
        {
            ImGui::Separator();
        }
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void RenderSavedPagesPane(const char* childId, const char* title, std::vector<SavedPageEntry>& entries, const char* emptyText, const char* removeLabel)
{
    ImGui::BeginChild(childId, ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted(title);
    ImGui::Separator();

    if (entries.empty())
    {
        ImGui::TextDisabled("%s", emptyText);
        ImGui::EndChild();
        return;
    }

    ArticleTab* activeTab = ActiveTab();
    const int activePageId = activeTab != nullptr ? activeTab->PageId : -1;
    const std::string activeTitle = activeTab != nullptr ? activeTab->Title : "";
    size_t removeIndex = static_cast<size_t>(-1);
    int pendingOpenPageId = -1;
    std::string pendingOpenTitle;

    for (size_t index = 0; index < entries.size(); ++index)
    {
        const SavedPageEntry& entry = entries[index];
        const bool selected = SavedPageMatches(entry, activePageId, activeTitle);

        ImGui::PushID(static_cast<int>(index));
        if (ImGui::Selectable(entry.Title.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns))
        {
            pendingOpenPageId = entry.PageId;
            pendingOpenTitle = entry.Title;
        }

        if (entry.PageId > 0)
        {
            ImGui::TextDisabled("Page %d", entry.PageId);
            ImGui::SameLine();
        }

        if (ImGui::SmallButton(removeLabel))
        {
            removeIndex = index;
        }

        if (index + 1 < entries.size())
        {
            ImGui::Separator();
        }
        ImGui::PopID();
    }

    if (removeIndex != static_cast<size_t>(-1))
    {
        entries.erase(entries.begin() + static_cast<long long>(removeIndex));
        SaveLibrary();
    }

    if (!pendingOpenTitle.empty())
    {
        LoadPageIntoActiveTab(pendingOpenPageId, pendingOpenTitle, false);
    }

    ImGui::EndChild();
}

void RenderLeftPane()
{
    switch (gState.CurrentLeftPaneMode)
    {
    case LeftPaneMode::Search:
        RenderSearchResultsPane();
        return;
    case LeftPaneMode::Recent:
        RenderSavedPagesPane("##WikiRecentPages", "Recent", gState.RecentPages, "Pages you open will stay here for quick return access.", "Remove");
        return;
    case LeftPaneMode::Favorites:
        RenderSavedPagesPane("##WikiFavoritePages", "Favorites", gState.FavoritePages, "Use the Favorite button on an article to pin it here.", "Remove");
        return;
    }
}

void RenderArticleTabContent(ArticleTab& tab)
{
    const std::string heading = tab.Document.DisplayTitle.empty()
        ? (tab.Title.empty() ? "Article" : tab.Title)
        : tab.Document.DisplayTitle;
    const int currentPageId = tab.Document.PageId > 0 ? tab.Document.PageId : tab.PageId;
    const std::string currentTitle = !tab.Document.Title.empty() ? tab.Document.Title : tab.Title;
    const bool favorite = ContainsFavoritePage(currentPageId, currentTitle);

    ImGui::TextWrapped("%s", heading.c_str());
    ImGui::TextDisabled("%s", tab.Status.c_str());

    if (!tab.Document.Url.empty())
    {
        if (ImGui::Button("Open in Browser"))
        {
            if (!OpenExternalUrl(tab.Document.Url))
            {
                Notify("NexusGameWiki could not open the browser.");
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy URL"))
        {
            if (CopyToClipboard(tab.Document.Url))
            {
                Notify("NexusGameWiki copied the article URL.");
            }
        }
        ImGui::SameLine();
    }

    if (ImGui::Button(favorite ? "Unfavorite" : "Favorite"))
    {
        ToggleFavoritePage(currentPageId, currentTitle);
    }
    ImGui::SameLine();

    if (ImGui::Button("Refresh Article"))
    {
        QueuePageLoad(tab.Id, tab.PageId, tab.Title, true);
    }

    if (!tab.Document.Parsed.Sections.empty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("%d sections", static_cast<int>(tab.Document.Parsed.Sections.size()));
    }

    ImGui::Separator();
    const std::string bodyId = "##WikiArticleBody" + std::to_string(tab.Id);
    ImGui::BeginChild(bodyId.c_str(), ImVec2(0.0f, 0.0f), false, ImGuiWindowFlags_HorizontalScrollbar);
    if (tab.Loading)
    {
        ImGui::TextDisabled("%s", tab.Status.c_str());
    }
    else if (tab.Document.Parsed.Blocks.empty())
    {
        ImGui::TextDisabled("This page did not produce renderable HTML blocks.");
    }
    else
    {
        RenderParsedBlocks(tab.Document.Parsed, tab);
    }
    ImGui::EndChild();
}

void RenderPagePane()
{
    ImGui::BeginChild("##WikiArticle", ImVec2(0.0f, 0.0f), true);
    ImGui::TextUnformatted("Article");
    ImGui::Separator();

    if (gState.Tabs.empty())
    {
        ImGui::TextDisabled("Pick a wiki result to load its article here.");
        ImGui::EndChild();
        return;
    }

    int tabToClose = 0;
    if (ImGui::BeginTabBar("##WikiArticleTabs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs))
    {
        for (auto& tab : gState.Tabs)
        {
            const std::string visibleLabel = tab.Document.DisplayTitle.empty()
                ? (tab.Title.empty() ? "Article" : tab.Title)
                : tab.Document.DisplayTitle;
            const std::string label = visibleLabel + "##WikiTab" + std::to_string(tab.Id);

            bool open = true;
            ImGuiTabItemFlags tabFlags = tab.Loading ? ImGuiTabItemFlags_UnsavedDocument : ImGuiTabItemFlags_None;
            if (ImGui::BeginTabItem(label.c_str(), &open, tabFlags))
            {
                gState.ActiveTabId = tab.Id;
                RenderArticleTabContent(tab);
                ImGui::EndTabItem();
            }

            if (!open)
            {
                tabToClose = tab.Id;
            }
        }

        ImGui::EndTabBar();
    }

    if (tabToClose != 0)
    {
        RemoveTab(tabToClose);
    }

    ImGui::EndChild();
}

void RenderWindow()
{
    if (!gState.WindowVisible)
    {
        return;
    }

    gState.ImageTextureLoadRequestsThisFrame = 0;
    UpdateSearchFlow();

    ImGui::SetNextWindowSize(ImVec2(1220.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(kWindowTitle, &gState.WindowVisible, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        gState.UserSettings.WindowVisible = gState.WindowVisible;
        return;
    }

    gState.UserSettings.WindowVisible = gState.WindowVisible;

    ImGui::TextUnformatted("Guild Wars 2 in-game wiki viewer.");
    ImGui::SameLine();
    ImGui::TextDisabled("Search online once, then reopen from cache.");

    if (gState.FocusSearchOnOpen)
    {
        ImGui::SetKeyboardFocusHere();
        gState.FocusSearchOnOpen = false;
    }

    ImGui::PushItemWidth(-250.0f);
    ImGui::InputTextWithHint("##WikiSearch", "Type any GW2 wiki page title, item, NPC, map, event, or achievement", gState.SearchBuffer, IM_ARRAYSIZE(gState.SearchBuffer), ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Clear", ImVec2(70.0f, 0.0f)))
    {
        gState.SearchBuffer[0] = '\0';
        gState.Search = SearchState{};
        gState.Tabs.clear();
        gState.ActiveTabId = 0;
            gState.CurrentLeftPaneMode = LeftPaneMode::Search;
        gState.FocusSearchOnOpen = true;
    }

    ImGui::SameLine();
    const bool canRefreshSearch = Normalize(NormalizeSearchInput(std::string(gState.SearchBuffer))).size() >= 2;
    if (!canRefreshSearch)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.55f);
    }
    if (ImGui::Button("Refresh Search", ImVec2(110.0f, 0.0f)) && canRefreshSearch)
    {
        gState.CurrentLeftPaneMode = LeftPaneMode::Search;
        gState.Search.LastLoadedQueryNormalized.clear();
        QueueSearch(true);
    }
    if (!canRefreshSearch)
    {
        ImGui::PopStyleVar();
    }

    RenderModeStrip();
    ImGui::Separator();
    if (ImGui::BeginTable("##WikiLayout", 3, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("Results", ImGuiTableColumnFlags_WidthStretch, 0.23f);
        ImGui::TableSetupColumn("Article", ImGuiTableColumnFlags_WidthStretch, 0.54f);
        ImGui::TableSetupColumn("Contents", ImGuiTableColumnFlags_WidthStretch, 0.23f);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        RenderLeftPane();

        ImGui::TableSetColumnIndex(1);
        RenderPagePane();

        ImGui::TableSetColumnIndex(2);
        RenderContentsSidebar(ActiveTab());

        ImGui::EndTable();
    }

    ImGui::End();

    if (!gState.PendingOpenTitle.empty())
    {
        const int pendingPageId = gState.PendingOpenPageId;
        const std::string pendingTitle = gState.PendingOpenTitle;
        gState.PendingOpenPageId = 0;
        gState.PendingOpenTitle.clear();
        OpenPageInNewTab(pendingPageId, pendingTitle, false);
    }
}

void RenderOptions()
{
    ImGui::Separator();
    ImGui::TextUnformatted("NexusGameWiki");
    ImGui::TextDisabled("Guild Wars 2 in-game wiki viewer.");

    bool visible = gState.WindowVisible;
    if (ImGui::Checkbox("Open window", &visible))
    {
        SetWindowVisible(visible);
        SaveSettings();
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Hotkey");

    bool ctrl = gState.UserSettings.Hotkey.Ctrl;
    bool alt = gState.UserSettings.Hotkey.Alt;
    bool shift = gState.UserSettings.Hotkey.Shift;
    int selectedKey = KeyOptionIndex(gState.UserSettings.Hotkey.Key);

    if (ImGui::Checkbox("Ctrl", &ctrl))
    {
        gState.UserSettings.Hotkey.Ctrl = ctrl;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Alt", &alt))
    {
        gState.UserSettings.Hotkey.Alt = alt;
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Shift", &shift))
    {
        gState.UserSettings.Hotkey.Shift = shift;
    }

    std::vector<const char*> labels;
    labels.reserve(std::size(kKeyOptions));
    for (const auto& option : kKeyOptions)
    {
        labels.push_back(option.Label);
    }

    if (ImGui::Combo("Key", &selectedKey, labels.data(), static_cast<int>(labels.size())))
    {
        gState.UserSettings.Hotkey.Key = kKeyOptions[selectedKey].Vk;
    }

    ImGui::SameLine();
    if (ImGui::Button("Apply Hotkey"))
    {
        RegisterHotkey();
        SaveSettings();
        Notify("NexusGameWiki hotkey set to " + FormatHotkey(gState.UserSettings.Hotkey));
    }

    ImGui::TextDisabled("Current bind: %s", FormatHotkey(gState.UserSettings.Hotkey).c_str());

    ImGui::Spacing();
    ImGui::TextUnformatted("Sections");
    ImGui::TextWrapped("All wiki sections stay expanded by default. Turn on any of these if you want those section types to start collapsed.");

    auto updateSectionOption = [](const char* label, bool* value) {
        if (ImGui::Checkbox(label, value))
        {
            SaveSettings();
            ResetArticleSectionStates();
        }
    };

    updateSectionOption("Collapse Notes by default", &gState.UserSettings.CollapseNotesByDefault);
    updateSectionOption("Collapse Trivia by default", &gState.UserSettings.CollapseTriviaByDefault);
    updateSectionOption("Collapse Gallery by default", &gState.UserSettings.CollapseGalleryByDefault);
    updateSectionOption("Collapse History by default", &gState.UserSettings.CollapseHistoryByDefault);
    updateSectionOption("Collapse See also by default", &gState.UserSettings.CollapseSeeAlsoByDefault);
    updateSectionOption("Collapse References by default", &gState.UserSettings.CollapseReferencesByDefault);
    updateSectionOption("Collapse External links by default", &gState.UserSettings.CollapseExternalLinksByDefault);

    ImGui::Spacing();
    ImGui::TextUnformatted("Cache");
    ImGui::TextWrapped("Searches, article HTML, and downloaded wiki images are cached locally so the addon can reopen pages from disk before it goes back online.");
    ImGui::TextDisabled("Search cache: %s", gState.SearchCacheDirectory.c_str());
    ImGui::TextDisabled("Page cache: %s", gState.PageCacheDirectory.c_str());
    ImGui::TextDisabled("Image cache: %s", gState.ImageCacheDirectory.c_str());

    if (ImGui::Button("Clear All Cache"))
    {
        {
            std::lock_guard<std::mutex> lock(gWorker.Mutex);
            gWorker.ImageGeneration += 1;
            gWorker.ImageJobs.clear();
            gWorker.ImageResults.clear();
        }

        const bool searchCleared = DeleteFilesInDirectory(gState.SearchCacheDirectory);
        const bool pageCleared = DeleteFilesInDirectory(gState.PageCacheDirectory);
        const bool imageCleared = DeleteFilesInDirectory(gState.ImageCacheDirectory);
        if (searchCleared && pageCleared && imageCleared)
        {
            Notify("NexusGameWiki cache cleared.");
            gState.RemoteImages.clear();
            gState.Search.LastLoadedQueryNormalized.clear();
            gState.Search.Status = "Cache cleared. Search again to refill it.";
            for (auto& tab : gState.Tabs)
            {
                tab.Status = "Cache cleared.";
            }
        }
        else
        {
            Notify("NexusGameWiki could not remove every cached file.");
        }
    }
}

void AddonLoad(AddonAPI* api)
{
    gState = AppState{};
    gState.Api = api;

    ImGui::SetCurrentContext(static_cast<ImGuiContext*>(gState.Api->ImguiContext));
    ImGui::SetAllocatorFunctions(
        reinterpret_cast<void* (*)(size_t, void*)>(gState.Api->ImguiMalloc),
        reinterpret_cast<void (*)(void*, void*)>(gState.Api->ImguiFree)
    );

    PreparePaths();
    LoadSettings();
    LoadLibrary();
    RegisterHotkey();
    EnsureIconShortcut();
    StartWorkers();

    gState.Api->Renderer.Register(ERenderType_Render, RenderWindow);
    gState.Api->Renderer.Register(ERenderType_OptionsRender, RenderOptions);
    gState.Api->UI.RegisterCloseOnEscape(kWindowTitle, &gState.WindowVisible);

    Log(ELogLevel_INFO, "NexusGameWiki loaded.");
}

void AddonUnload()
{
    SaveSettings();
    SaveLibrary();
    StopWorkers();

    if (gState.Api != nullptr)
    {
        gState.Api->InputBinds.Deregister(kToggleKeybindId);
        gState.Api->Renderer.Deregister(RenderWindow);
        gState.Api->Renderer.Deregister(RenderOptions);
        gState.Api->UI.DeregisterCloseOnEscape(kWindowTitle);

        if (gState.IconRegistered)
        {
            gState.Api->QuickAccess.Remove(kQuickAccessId);
        }

        Log(ELogLevel_INFO, "NexusGameWiki unloaded.");
    }
}

} // namespace

extern "C" __declspec(dllexport) AddonDefinition* GetAddonDef()
{
    static AddonDefinition addonDef{};
    addonDef.Signature = kDefaultSignature;
    addonDef.APIVersion = NEXUS_API_VERSION;
    addonDef.Name = kAddonName;
    addonDef.Version.Major = 0;
    addonDef.Version.Minor = 1;
    addonDef.Version.Build = 0;
    addonDef.Version.Revision = 1;
    addonDef.Author = "mestyq.3204";
    addonDef.Description = "Guild Wars 2 in-game wiki viewer.";
    addonDef.Load = AddonLoad;
    addonDef.Unload = AddonUnload;
    addonDef.Flags = EAddonFlags_None;
    addonDef.Provider = EUpdateProvider_GitHub;
    addonDef.UpdateLink = "https://github.com/mestyq/NexusGameWiki";
    return &addonDef;
}
