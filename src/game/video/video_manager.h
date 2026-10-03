#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/smacker.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

// Movie playback (0x5f68b0-0x5f7940): a VideoManager owns at most one movie window, a SmackerWindow for the
// game's Smacker movies (MDATA files) or a VideoWindow driving an MCI "avivideo" device (no .avi file ships, so
// that path is dead in practice).
//
// The movie windows report to their parent window with private messages: 0x402 = the last frame was shown,
// 0x403 = the file could not be opened, 0x404 = the movie was skipped (Escape or a mouse click).

enum MovieType : int {
    kMovieSmacker = 1,
    kMovieAvi = 2,
};

constexpr UINT kMovieMessageFinished = 0x402;
constexpr UINT kMovieMessageNotFound = 0x403;
constexpr UINT kMovieMessageSkipped = 0x404;

class SmackerWindow : public CoolWindow {
public:
    SmackerWindow(const char* path, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                  int autoStart, uint32_t smackFlags);
    virtual ~SmackerWindow();
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    // Starts the playback thread (stopping a previous one); `loop` makes the movie restart after its last frame.
    int StartPlayback(int loop);
    // Stops the thread and releases the Smack and its buffer; always returns 1.
    int PrepareDestroy(HWND window);
    void StopPlayback();
    // Opens the movie and its window buffer; false when SmackOpen fails.
    int OpenMovie();
    // Decodes and shows the next frame. Returns 1 when playback is over (end reached without looping, or skipped).
    int AdvanceFrame();
    void SetVolume(int volume);

    static int GetMovieSize(int* width, int* height, HWND unusedWindow, const char* path);
    static uint32_t PlaybackThreadProc(SmackerWindow* window);

    char m_path[0x100];        // +0x094
    uint32_t m_unk194;         // +0x194
    uint32_t m_unk198;         // +0x198
    uint32_t m_scale;          // +0x19c, integer window zoom
    int m_autoStart;           // +0x1a0, WM_CREATE starts playback
    int m_loop;                // +0x1a4
    SmackBuf* m_smackBuffer;   // +0x1a8
    Smack* m_smack;            // +0x1ac
    DWORD m_threadId;          // +0x1b0
    HANDLE m_thread;           // +0x1b4
    uint32_t m_smackFlags;     // +0x1b8, SmackOpen flags
    int m_savedVolume;         // +0x1bc, DirectSound primary volume when the window was created
    int m_volumeTouched;       // +0x1c0, SetVolume was called; the destructor restores m_savedVolume
};

class VideoWindow : public CoolWindow {
public:
    VideoWindow(const char* path, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                int autoStart);
    virtual ~VideoWindow();
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;

    bool OpenMovie(HWND window, const char* path, MCIDEVICEID* device);
    bool Play(int fullScreen);
    bool CloseMovie(HWND window, MCIDEVICEID device);
    bool OpenDriver(const char* path);
    bool CloseDevices(const char* path);

    static int GetMovieSize(int* width, int* height, HWND unusedWindow, const char* path);

    char m_path[0x100];        // +0x094
    MCIDEVICEID m_device;      // +0x194
    uint32_t m_unk198;         // +0x198
    uint32_t m_unk19c;         // +0x19c
    int m_autoStart;           // +0x1a0
};

class VideoManager {
public:
    explicit VideoManager(CoolWindow* parentWindow);
    virtual ~VideoManager();

    // Starts a movie of `type` (MovieType) in a child window of `parent` (the application window when null) at
    // (x, y), centred when both are 0. `scale` zooms Smacker movies. Returns 1 when the movie size is known.
    int Play(const char* path, CoolWindow* parent, int x, int y, int type, uint32_t scale, uint32_t smackFlags);
    // Destroys the movie window; 1 if there was one.
    int Stop();
    void NotifyPaletteChanged(HWND window);
    void SetVolume(int volume);
    void AdvanceMovieFrame();
    bool IsDefaultPlacement(int x, int y);

    static VideoManager* GetInstance();
    static int GetMovieSize(const char* path, int* width, int* height, int type);

    VideoWindow* m_videoWindow;      // +0x04, the MovieType 2 window
    SmackerWindow* m_smackerWindow;  // +0x08, the MovieType 1 window
    int m_movieType;                 // +0x0c
    int m_playing;                   // +0x10
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SmackerWindow, 0x1c4);
REB_ASSERT_OFFSET(SmackerWindow, m_path, 0x94);
REB_ASSERT_OFFSET(SmackerWindow, m_scale, 0x19c);
REB_ASSERT_OFFSET(SmackerWindow, m_loop, 0x1a4);
REB_ASSERT_OFFSET(SmackerWindow, m_smackBuffer, 0x1a8);
REB_ASSERT_OFFSET(SmackerWindow, m_smack, 0x1ac);
REB_ASSERT_OFFSET(SmackerWindow, m_thread, 0x1b4);
REB_ASSERT_OFFSET(SmackerWindow, m_smackFlags, 0x1b8);
REB_ASSERT_OFFSET(SmackerWindow, m_volumeTouched, 0x1c0);
REB_ASSERT_SIZE(VideoWindow, 0x1a4);
REB_ASSERT_OFFSET(VideoWindow, m_device, 0x194);
REB_ASSERT_SIZE(VideoManager, 0x14);
REB_ASSERT_OFFSET(VideoManager, m_movieType, 0x0c);
#pragma clang diagnostic pop
