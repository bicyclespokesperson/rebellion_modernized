#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

class DibBitmap;

// MovieWindow ("MOVIE_WINDOW", ctor 0x4213e0, VT 0x658bb0, 0xb8 bytes): the full-screen black window the game plays its MDATA
// Smacker movies in (cut scenes and the intro). It starts the movie through the VideoManager when it is created, optionally
// shows a caption in a text field over a backdrop, and ends the movie when the user presses a key or when the movie
// reports that it is over (the VideoManager's private window messages 0x402-0x404). The parent is told with 0x405 (and the
// callback, when there is one, is called with the same arguments).
class MovieWindow : public CoolWindow {
public:
    // `movieId` (0x65-0x74) selects the MDATA file; `callback` may be null; `caption` (copied) may be null.
    MovieWindow(int movieId, void (* callback)(uint32_t, uint32_t, int), HINSTANCE instance, int x, int y, int width,
                int height, CoolWindow* parent, const std::string* caption);
    ~MovieWindow() override;  // 0x421700 (scalar deleting destructor 0x4216e0)
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // 0x421790

    // The MDATA file of the movie id as "MDATA\MDATA.nnn", null for an unknown id.
    const char* GetMovieFile();  // 0x421ba0

    void OnCreate();  // the WM_CREATE part of WindowProc
    void EndMovie(UINT notifyMessage);  // hides, stops the movie and tells the parent

    int m_notifyWParam;                                   // +0x94, 0xf: wParam of the notifications
    int m_movieId;                                        // +0x98
    char* m_path;                                         // +0x9c heap copy of the full path of the movie file
    const char* m_fallbackPath;                                   // +0xa0, never set (a path pointer used when m_path is null)
    std::string m_caption;                                     // +0xa4
    DibBitmap* m_captionBackdrop;                         // +0xb0 owned
    void (* m_callback)(uint32_t, uint32_t, int);  // +0xb4
};


// The VideoManager singleton behind the movie window (0x421300-0x4213c0): all do nothing without a manager.
int PlayMovie(const char* path, int x, int y, CoolWindow* parent, int type, uint32_t scale, uint32_t smackFlags);  // 0x421300
int StopMovie();                           // 0x421340
void NotifyMoviePaletteChanged(HWND window);  // 0x421360
int IsMoviePlaying();                      // 0x421380
void SetMovieVolume(int volume);           // 0x4213a0
void AdvanceMovie();                       // 0x4213c0
