#include "video/movie_window.h"

#include <cstring>

#include "app/app_save_strings.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/registry.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_app.h"
#include "ui/text_scroll_field.h"
#include "video/video_manager.h"

extern const char* g_registryKeyPath;    // 0x6a81fc
extern const char* g_musicVolumeName;    // 0x6a8584
extern const char* g_tooBigLocationName; // 0x6a7c74


extern const char* g_movieFile000; // 0x6a8460
extern const char* g_movieFile001; // 0x6a8698
extern const char* g_movieFile002; // 0x6a8688
extern const char* g_movieFile003; // 0x6a8678
extern const char* g_movieFile004; // 0x6a8668
extern const char* g_movieFile106; // 0x6a8658
extern const char* g_movieFile105; // 0x6a8648
extern const char* g_movieFile107; // 0x6a8638
extern const char* g_movieFile108; // 0x6a8628
extern const char* g_movieFile005; // 0x6a8618
extern const char* g_movieFile101; // 0x6a8608
extern const char* g_movieFile202; // 0x6a85f8
extern const char* g_movieFile201; // 0x6a85e8
extern const char* g_movieFile104; // 0x6a85d8
extern const char* g_movieFile102; // 0x6a85c8
extern const char* g_movieFile103; // 0x6a85b8

namespace
{

constexpr int kBlackBrush = BLACK_BRUSH;
constexpr uint32_t kNotifyParentMessage = 0x405;
constexpr UINT kMessageMovieSkipped = 0x404;
constexpr UINT kMessageEraseBackground = 0x14;
constexpr int kCloseCommand = 0xf1;
constexpr int kKeyF1 = 0x70;
constexpr int kKeyF2 = 0x71;
constexpr uint32_t kSmackFlagsStandard = 0xfe400;
constexpr uint32_t kSmackFlagsFallback = 0xff400;
constexpr uint32_t kResourceSlotArt = 0xc;
constexpr uint32_t kCaptionArtId = 0x2b35;
constexpr int kCaptionFieldX = 4;
constexpr int kCaptionFieldY = 0x194;
constexpr int kCaptionFieldWidth = 0x244;
constexpr int kCaptionFieldHeight = 0x46;
constexpr uint32_t kCaptionFieldChildId = 0xf2;
constexpr COLORREF kCaptionTextColor = 0x2ffffff;
constexpr int kCaptionFont = 4;

// The full path of the movie file: "<TooBigLocation>\\" followed by the MDATA file name, as a heap copy.
char* BuildMoviePath(MovieWindow* window)
{
  ScopedStrstream key;
  ScopedStrstream path;
  ostream* keyOut = iostream_access::AsOstream(key.get());
  ostream* pathOut = iostream_access::AsOstream(path.get());
  ostream_operator_ls_char(
      ostream_operator_ls_cstr(
          ostream_operator_ls_cstr(ostream_operator_ls_cstr(keyOut, "SOFTWARE\\LucasArts Entertainment Company LLC"),
                                   "\\"),
          "Star Wars Rebellion"),
      0);
  char* location = reinterpret_cast<char*>(
      ReadRegistryValue(strstreambuf_str(iostream_access::BufferOf(key.get())), g_tooBigLocationName));
  strstreambuf_freeze(iostream_access::BufferOf(key.get()), 0);
  ios* keyState = iostream_access::IosOf(key.get());
  iostream_access::Lock(keyState);
  keyState->m_state = 0;
  iostream_access::Unlock(keyState);
  if (location) {
    ostream_operator_ls_cstr(ostream_operator_ls_cstr(pathOut, location), "\\");
    ::operator delete(location);
  }
  ostream_operator_ls_char(ostream_operator_ls_cstr(pathOut, window->GetMovieFile()), 0);
  char* text = strstreambuf_str(iostream_access::BufferOf(path.get()));
  char* copy = nullptr;
  if (text) {
    const size_t length = std::strlen(text);
    copy = static_cast<char*>(::operator new(length + 1));
    window->m_path = copy;
    if (copy) {
      std::memset(copy, 0, length);
      std::memcpy(copy, text, length + 1);
    }
  }
  strstreambuf_freeze(iostream_access::BufferOf(path.get()), 0);
  ios* pathState = iostream_access::IosOf(path.get());
  iostream_access::Lock(pathState);
  pathState->m_state = 0;
  iostream_access::Unlock(pathState);
  return copy;
}
} // namespace

MovieWindow::MovieWindow(int movieId, void (*callback)(uint32_t, uint32_t, int), HINSTANCE instance, int x, int y,
                         int width, int height, CoolWindow* parent, const std::string* caption)
    : CoolWindow(instance, "MOVIE_WINDOW", WS_CHILD, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(reb::compat::GetStockObject(kBlackBrush)), nullptr, nullptr, nullptr, 0, 0)
{
  m_movieId = movieId;
  m_notifyWParam = 0xf;
  m_callback = callback;
  m_captionBackdrop = nullptr;
  m_path = nullptr;
  m_fallbackPath = nullptr;
  if (caption) {
    (m_caption = *caption);
  }
  BuildMoviePath(this);
}

MovieWindow::~MovieWindow()
{
  if (m_path) {
    ::operator delete(m_path);
  }
  StopMovie();
  if (m_captionBackdrop) {
    m_captionBackdrop->~DibBitmap();
    ::operator delete(m_captionBackdrop);
  }
}

const char* MovieWindow::GetMovieFile()
{
  switch (m_movieId) {
    case 0x65:
      return g_movieFile000;
    case 0x66:
      return g_movieFile001;
    case 0x67:
      return g_movieFile002;
    case 0x68:
      return g_movieFile003;
    case 0x69:
      return g_movieFile004;
    case 0x6a:
      return g_movieFile106;
    case 0x6b:
      return g_movieFile105;
    case 0x6c:
      return g_movieFile107;
    case 0x6d:
      return g_movieFile108;
    case 0x6e:
      return g_movieFile005;
    case 0x6f:
      return g_movieFile101;
    case 0x70:
      return g_movieFile202;
    case 0x71:
      return g_movieFile201;
    case 0x72:
      return g_movieFile104;
    case 0x73:
      return g_movieFile102;
    case 0x74:
      return g_movieFile103;
    default:
      return nullptr;
  }
}

void MovieWindow::EndMovie(UINT notifyMessage)
{
  Hide();
  StopMovie();
  reb::compat::PostMessageA(m_parent->m_hwnd, notifyMessage, m_notifyWParam, m_movieId);
}

void MovieWindow::OnCreate()
{
  m_childId = m_notifyWParam;
  if (!m_caption.empty()) {
    HMODULE art = CoolAppGetResourceModule(kResourceSlotArt);
    auto* picture = NewOrNull<DibBitmap>(art, kCaptionArtId, 2u);
    const int width = m_width;
    const int height = m_height;
    m_captionBackdrop = NewOrNull<DibBitmap>(width, height, picture);
    if (picture) {
      picture->~DibBitmap();
      ::operator delete(picture);
    }
    if (m_captionBackdrop) {
      m_captionBackdrop->FillRect(0, 0, width, height, 0);
    }
    auto* field = NewOrNull<TextScrollField>(art, kCaptionFieldX, kCaptionFieldY, kCaptionFieldWidth,
                                             kCaptionFieldHeight, this, m_captionBackdrop, 0);
    if (field) {
      field->m_childId = kCaptionFieldChildId;
      field->m_textColor = kCaptionTextColor;
      reb::compat::InvalidateRect(field->m_hwnd, nullptr, FALSE);
      field->SetTextFont(kCaptionFont);
      (field->m_text = m_caption);
      field->m_caret.m_index = 0;
      field->Reset();
      field->Create(0);
      m_children.Insert(field);
    }
  }
  const char* source = m_path;
  uint32_t smackFlags = kSmackFlagsStandard;
  if (!source) {
    smackFlags = kSmackFlagsFallback;
    source = m_fallbackPath;
  }
  PlayMovie(source, 0, 0, this, 1, 1, smackFlags);
  const int volume = static_cast<int>(ReadRegistryDword(g_registryKeyPath, g_musicVolumeName));
  SetMovieVolume(-volume);
}

LRESULT MovieWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND window = m_hwnd;
  LRESULT result = 0;
  UINT notify = 0;
  if (message < 0x10) {
    if (message == WM_PAINT) {
      PAINTSTRUCT paint;
      reb::compat::BeginPaint(window, &paint);
      reb::compat::EndPaint(window, &paint);
    } else if (message == WM_CREATE) {
      OnCreate();
    }
    return result;
  }
  if (message < 0x2c) {
    if (message == kMessageEraseBackground) {
      reb::compat::DefWindowProcA(window, kMessageEraseBackground, wParam, lParam);
      result = 1;
    }
    return result;
  }
  if (message < 0x112) {
    if (message == WM_COMMAND) {
      if (static_cast<short>(wParam) != kCloseCommand) {
        return 0;
      }
      EndMovie(kNotifyParentMessage);
      return 0;
    }
    if (message != WM_KEYDOWN) {
      return 0;
    }
    if (wParam != VK_ESCAPE) {
      if (m_movieId == kKeyF2) {
        return 0;
      }
      if (m_movieId == kKeyF1) {
        return 0;
      }
      Hide();
      StopMovie();
      notify = kNotifyParentMessage;
    }
    reb::compat::PostMessageA(m_parent->m_hwnd, notify, m_notifyWParam, m_movieId);
  } else if (message < 0x404) {
    if (message < 0x402) {
      if (message != WM_MOUSEMOVE) {
        return 0;
      }
      reb::compat::SetCursor(nullptr);
      return 0;
    }
    EndMovie(kNotifyParentMessage);
    notify = 0;
  } else {
    if (message != kMessageMovieSkipped) {
      return 0;
    }
    EndMovie(kNotifyParentMessage);
    notify = kNotifyParentMessage;
  }
  if (m_callback == nullptr) {
    return 0;
  }
  m_callback(notify, static_cast<uint16_t>(m_notifyWParam), m_movieId);
  return result;
}

int PlayMovie(const char* path, int x, int y, CoolWindow* parent, int type, uint32_t scale, uint32_t smackFlags)
{
  int result = 0;
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    result = manager->Play(path, parent, x, y, type, scale, smackFlags);
  }
  return result;
}

int StopMovie()
{
  int result = 0;
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    result = manager->Stop();
  }
  return result;
}

void NotifyMoviePaletteChanged(HWND window)
{
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    manager->NotifyPaletteChanged(window);
  }
}

int IsMoviePlaying()
{
  int result = 0;
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    result = manager->m_playing;
  }
  return result;
}

void SetMovieVolume(int volume)
{
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    manager->SetVolume(volume);
  }
}

void AdvanceMovie()
{
  VideoManager* manager = VideoManager::GetInstance();
  if (manager) {
    manager->AdvanceMovieFrame();
  }
}

const char* g_movieFile000 = nullptr;
const char* g_movieFile001 = nullptr;
const char* g_movieFile002 = nullptr;
const char* g_movieFile003 = nullptr;
const char* g_movieFile004 = nullptr;
const char* g_movieFile005 = nullptr;
const char* g_movieFile101 = nullptr;
const char* g_movieFile102 = nullptr;
const char* g_movieFile103 = nullptr;
const char* g_movieFile104 = nullptr;
const char* g_movieFile105 = nullptr;
const char* g_movieFile106 = nullptr;
const char* g_movieFile107 = nullptr;
const char* g_movieFile108 = nullptr;
const char* g_movieFile201 = nullptr;
const char* g_movieFile202 = nullptr;
