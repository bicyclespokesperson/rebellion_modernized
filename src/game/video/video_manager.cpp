// VideoManager: owns the movie window and dispatches to the Smacker or AVI implementation.
#include "video/video_manager.h"

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "ui/cool_environment.h"

extern VideoManager* g_videoManager;
extern CoolWindow* g_videoParentWindow;
extern const char *g_pathUnknownCaption;  // "PathName unknown to VideoManager"


namespace {
constexpr UINT kPaletteChangedMessage = WM_PALETTECHANGED;
constexpr int kShowWindowNormal = SW_SHOW;
}  // namespace

VideoManager::VideoManager(CoolWindow* parentWindow) {
    g_videoParentWindow = parentWindow;
    g_videoManager = this;
    m_videoWindow = nullptr;
    m_smackerWindow = nullptr;
    m_movieType = 0;
    m_playing = 0;
}

VideoManager::~VideoManager() {
    REB_KEEP_VPTR_STORE();
    Stop();
    g_videoManager = nullptr;
    g_videoParentWindow = nullptr;
}

VideoManager* VideoManager::GetInstance() { return g_videoManager; }

int VideoManager::IsDefaultPlacement(int x, int y) { return x == 0 && y == 0; }

int VideoManager::GetMovieSize(const char* path, int* width, int* height, int type) {
    if (!g_videoParentWindow) return 0;
    HWND window = g_videoParentWindow->m_hwnd;
    if (type == kMovieAvi) return VideoWindow::GetMovieSize(width, height, window, path);
    if (type == kMovieSmacker) return SmackerWindow::GetMovieSize(width, height, window, path);
    return 0;
}

int VideoManager::Play(const char* path, CoolWindow* parent, int x, int y, int type, uint32_t scale,
                       uint32_t smackFlags) {
    Stop();
    int result = 0;
    if (!parent) parent = g_videoParentWindow;
    if (!parent) return result;

    int width = 0;
    int height = 0;
    m_movieType = type;
    result = GetMovieSize(path, &width, &height, type);
    if (IsDefaultPlacement(x, y) && parent->m_hwnd) {
        RECT client;
        GetClientRect(parent->m_hwnd, &client);
        x = client.right / 2 - static_cast<int>((static_cast<uint32_t>(width) * scale) >> 1);
        y = client.bottom / 2 - static_cast<int>((static_cast<uint32_t>(height) * scale) >> 1);
    }

    HINSTANCE instance = parent->m_instance;
    CoolWindow* movieWindow = nullptr;
    if (!result) {
        ShowErrorMessage(path, g_pathUnknownCaption);
        PostMessageA(parent->m_hwnd, kMovieMessageNotFound, 0, 0);
    } else if (m_movieType == kMovieAvi) {
        m_videoWindow = NewOrNull<VideoWindow>(path, instance, x, y, width, height, parent, 1);
        movieWindow = m_videoWindow;
    } else if (m_movieType == kMovieSmacker) {
        m_smackerWindow = NewOrNull<SmackerWindow>(path, instance, x, y, width, height, parent, 1, smackFlags);
        if (m_smackerWindow && scale > 1) m_smackerWindow->m_scale = scale;
        movieWindow = m_smackerWindow;
    }
    if (movieWindow) {
        movieWindow->Create(0);
        movieWindow->Show(kShowWindowNormal);
        result = 1;
        m_playing = 1;
    }
    return result;
}

int VideoManager::Stop() {
    m_playing = 0;
    if (m_movieType == kMovieAvi) {
        if (!m_videoWindow) return 0;
        DestroyWindow(m_videoWindow->m_hwnd);
        delete m_videoWindow;
        m_videoWindow = nullptr;
        return 1;
    }
    if (m_movieType == kMovieSmacker && m_smackerWindow) {
        HWND window = m_smackerWindow->m_hwnd;
        m_smackerWindow->PrepareDestroy(window);
        DestroyWindow(window);
        delete m_smackerWindow;
        m_smackerWindow = nullptr;
        return 1;
    }
    return 0;
}

void VideoManager::NotifyPaletteChanged(HWND window) {
    CoolWindow* movieWindow = nullptr;
    if (m_movieType == kMovieAvi)
        movieWindow = m_videoWindow;
    else if (m_movieType == kMovieSmacker)
        movieWindow = m_smackerWindow;
    if (movieWindow) movieWindow->WindowProc(kPaletteChangedMessage, reinterpret_cast<WPARAM>(window), 0);
}

void VideoManager::SetVolume(int volume) {
    if (m_movieType != kMovieAvi && m_movieType == kMovieSmacker && m_smackerWindow)
        m_smackerWindow->SetVolume(volume);
}

void VideoManager::AdvanceMovieFrame() {
    if (m_movieType == kMovieSmacker && m_smackerWindow && m_playing) m_smackerWindow->AdvanceFrame();
}

CoolWindow * g_videoParentWindow{};
VideoManager * g_videoManager{};
const char *g_pathUnknownCaption = nullptr;
