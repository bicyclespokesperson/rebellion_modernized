#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"

// "Cool" is Coolhand's in-house GUI toolkit (shared with their StarTrader). Every screen, panel and control of
// the game is a CoolWindow: a C++ object that owns one real child window. All of them share one window procedure
// (CoolWindowProc) that finds the object through the window's extra bytes and forwards the message to its virtual
// WindowProc, MFC-style. docs/platform_layer.md describes the model.

class CoolWindow;
class DibBitmap;
class FontTable;
class String;
struct ResourceStringId;

// ---- Small helper containers used by the window base -----------------------------------------------------

// A window handle (+0x18) together with the CoolWindow that owns it (+0x1c, set by the creator).
class HwndNode : public AvlNode {
public:
    explicit HwndNode(HWND hwnd);  // 0x60f180
    virtual ~HwndNode();           // 0x60f1d0

    HWND m_hwnd;           // +0x18
    CoolWindow* m_window;  // +0x1c
};

// Tree of HwndNodes ordered by window handle (shares IdAvlTree's comparison, which reads the dword at +0x18).
class HwndTree : public AvlTree {
public:
    HwndTree();           // 0x60f020
    virtual ~HwndTree();  // 0x60f060
    virtual int IsLess(AvlNode* a, AvlNode* b) override;

    HwndNode* FindByHwnd(HWND hwnd);    // 0x60f070
    void RemoveAndDelete(HWND hwnd);    // 0x60f0b0
};

// One entry of g_popupStack: the HwndNode of an open popup window (owned by the entry).
class PopupEntry : public LinkNode {
public:
    explicit PopupEntry(HwndNode* node);  // 0x60f100
    virtual ~PopupEntry();                // 0x60f120

    HwndNode* m_node;  // +0x0c
};

// The children of a window, ordered by child id (+0x24); the windows themselves are the nodes.
class ChildWindowTree : public AvlTree {
public:
    ChildWindowTree();           // 0x604490
    virtual ~ChildWindowTree();  // 0x6044d0
    virtual int IsLess(AvlNode* a, AvlNode* b) override;  // 0x6044e0

    CoolWindow* FindById(uint32_t id);   // 0x604500
    CoolWindow* FindByHwnd(HWND hwnd);   // 0x604540
};

// IdAvlTree used as an array element by the animation resource classes (0x404220, 0x412a90).
class IdAvlTreeElement : public IdAvlTree {
public:
    IdAvlTreeElement();           // 0x5fef50
    virtual ~IdAvlTreeElement();  // 0x5fef70
};

// ---- CoolWindow -------------------------------------------------------------------------------------------

// Bits of CoolWindow::m_flags.
enum CoolWindowFlags : uint32_t {
    kCoolNoMove = 0x01,       // WM_SYSCOMMAND SC_MOVE (and caption double-click) is swallowed
    kCoolNoSize = 0x02,
    kCoolNoMinimize = 0x04,
    kCoolNoMaximize = 0x08,
    kCoolOwnCursor = 0x10,    // keep the cursor passed to the constructor instead of g_defaultCursor
};

// Base class of every game window. The object is an AvlNode so that parents can keep their children in a
// ChildWindowTree. Virtual slot 5 receives every window message of the HWND.
class CoolWindow : public AvlNode {
public:
    CoolWindow(HINSTANCE instance, const char* className, uint32_t style, int x, int y, int width, int height,
               CoolWindow* parent, const char* windowName, HBRUSH background, HICON icon, HCURSOR cursor,
               HMENU menu, uint32_t unknown58, uint32_t flags);  // 0x5ff910
    virtual ~CoolWindow();  // 0x5ffb60 (scalar deleting destructor 0x5ffb40)

    virtual void Show(int command);                                          // slot 1, 0x5ffe40: ShowWindow
    virtual void Hide();                                                     // slot 2, 0x405490: Show(0)
    virtual void Enable();                                                   // slot 3, 0x5ffe60
    virtual void Disable();                                                  // slot 4, 0x5ffe80
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam);  // slot 5, 0x6007b0
    virtual void SetFont(int fontSlot);                                      // slot 6, 0x600290
    virtual int OnRightButtonUp(int x, int y, CoolWindow* sender);           // slot 7, 0x5cd6b0: returns 0
    virtual void Virtual08(int a, int b, int c);                             // slot 8, 0x48b3d0: does nothing

    ATOM RegisterWindowClass(UINT classStyle);  // 0x5ffc80: RegisterClassA with CoolWindowProc
    // Registers the class if needed and creates the window; true if m_hwnd is non-null. BUG(original):
    // useExStyle has no effect, m_exStyle is always the extended style.
    bool Create(int useExStyle);                // 0x5ffce0
    BOOL IsEnabled();                           // 0x5ffea0
    void CenterInParent();                      // 0x5ffeb0
    void SetWindowTitle(const char* title);     // 0x5fff30
    void SetClassName(const char* className);   // 0x5fffb0
    void DestroyNativeWindow();                 // 0x600280
    void SelectCurrentFont(HDC dc);             // 0x6002c0
    void RestoreFont(HDC dc);                   // 0x6002f0
    void SetTooltipText(const char* text);      // 0x600860
    UINT RealizeCurrentPalette();               // 0x6001f0: selects and realizes g_palette in the window's DC

    // Tooltips (comctl32 "tooltips_class32", one shared control, g_tooltipWindow). `this` is the tool owner.
    void CreateTooltipControl(LPARAM delayMs);                                // 0x6008e0
    void AddTool(CoolWindow* tool, const char* text);                         // 0x600970
    void AddToolFromResource(CoolWindow* tool, UINT stringId);                // 0x6009f0
    void AddToolFromString(CoolWindow* tool, const ResourceStringId* stringId);                   // 0x600a40
    void AddRectTool(RECT area, UINT toolId, const char* text);               // 0x600bd0
    void AddRectToolFromResource(RECT area, UINT toolId, UINT stringId);      // 0x600ac0
    void AddRectToolFromString(RECT area, UINT toolId, const ResourceStringId* stringId);         // 0x600b30
    void UpdateToolFromString(CoolWindow* tool, const ResourceStringId* stringId);                // 0x600c40
    void UpdateToolText(CoolWindow* tool, const char* text);                  // 0x600cc0
    void RemoveTool(CoolWindow* tool);                                        // 0x600d30
    void RemoveRectTool(UINT toolId);                                         // 0x600db0

    bool DestroyChildById(uint32_t childId);    // 0x600f90: removes the child from m_children and deletes it

    // Popup mode: while a popup is open a mouse hook swallows clicks on windows that are not part of it.
    bool BeginPopup();                          // 0x6010e0
    bool EndPopup();                            // 0x6011f0
    void RegisterWithActivePopup();             // 0x601260
    void AddLinkedWindow(CoolWindow* window);   // 0x601380
    void RemoveLinkedWindow(CoolWindow* window);  // 0x601400

    HWND m_hwnd;                    // +0x18
    HINSTANCE m_instance;           // +0x1c resource DLL that supplies the artwork (also the class's hInstance)
    CoolWindow* m_parent;           // +0x20
    uint32_t m_childId;             // +0x24 key in the parent's m_children; the child-window id; not set by the base
    int m_x;                        // +0x28 updated by WM_MOVE (low 16 bits, unsigned)
    int m_y;                        // +0x2c
    int m_width;                    // +0x30 as given to CreateWindowEx
    int m_height;                   // +0x34
    int m_clientWidth;              // +0x38 updated by WM_SIZE
    int m_clientHeight;             // +0x3c
    uint32_t m_flags;               // +0x40 CoolWindowFlags
    uint32_t m_unknown44;           // +0x44 zero
    char* m_className;              // +0x48 heap copy
    char* m_windowName;             // +0x4c heap copy, may be null
    uint32_t m_style;               // +0x50
    uint32_t m_exStyle;             // +0x54
    uint32_t m_unknown58;           // +0x58
    HBRUSH m_background;            // +0x5c class background brush
    HICON m_icon;                   // +0x60
    HCURSOR m_cursor;               // +0x64
    HMENU m_menu;                   // +0x68 used as hMenu by top-level windows (children use m_childId)
    ChildWindowTree m_children;     // +0x6c
    HwndTree m_linkedWindows;       // +0x74 windows that are shown together with this one (WM_SHOWWINDOW)
    HFONT m_font;                   // +0x7c
    HGDIOBJ m_savedFont;            // +0x80 font that was selected into the DC before m_font
    char* m_tooltipText;            // +0x84 heap copy
    HwndTree m_popupMembers;        // +0x88 windows that stay clickable while this popup is open
    HWND m_tooltipOwner;            // +0x90 window the tool was registered under (as the tool's owner)
};

// Message dispatcher shared by every Cool window class (the lpfnWndProc passed to RegisterClassA).
LRESULT CALLBACK CoolWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);  // 0x600310

// Window objects that currently exist; the dispatcher ignores messages for objects that are gone.
bool RegisterWindowObject(CoolWindow* window);        // 0x601420
bool IsWindowObjectRegistered(CoolWindow* window);    // 0x6014b0
bool UnregisterWindowObject(CoolWindow* window);      // 0x6014d0
// The CoolWindow behind `hwnd`, or null.
CoolWindow* GetCoolWindow(HWND hwnd);                 // 0x600f60

// Hooks.
LRESULT CALLBACK TooltipHookProc(int code, WPARAM wParam, LPARAM lParam);  // 0x600e50: relays mouse messages
LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam);    // 0x600ef0: popup click filter
void ShutdownTooltips();                                                   // 0x600e20
HWND CurrentPopupHwnd();                                                   // 0x601320
bool DismissPopup();                                                       // 0x601340
PopupEntry* __stdcall FindPopupEntry(HWND hwnd);                           // 0x6012f0
int ReleaseAllCaptures();                                                  // 0x6010a0
HWND __stdcall SetCaptureWindow(HWND hwnd);  // 0x601080: returns the previous capture window

// Drawing helpers.
void LoadResourceString(UINT id, LPSTR buffer, int size);                  // 0x601060 (LoadStringA on g_stringModule)
void __stdcall DrawShadowedText(HDC dc, LPCSTR text, LPRECT rect, COLORREF color, COLORREF shadow, UINT format,
                                int offset, int length);                   // 0x600fd0

// Palette: the identity palette of the display (see docs/platform_layer.md).
UINT __stdcall SetIdentityPalette(HDC dc, HPALETTE palette);  // 0x600010
void __stdcall ReleasePalette(HDC dc);              // 0x600230

// Resource DLL slots (slot << 16 | id is how resources are named across the game).
HMODULE FindResourceModule(uint32_t slot);                              // 0x5fefd0
uint32_t FindSlotOfModule(HMODULE module);                              // 0x5feff0: slot of a loaded DLL, 0 if none
HMODULE LoadResourceModule(const char* basePath, const char* fileName, uint32_t slot);  // 0x5ff020

extern const char g_tooltipClassName[];  // 0x6ac7c0 "tooltips_class32"
extern HPALETTE g_savedPalette;       // 0x6be59c palette that was selected before ours
extern HPALETTE g_palette;            // 0x6be5a0 current game palette
extern HWND g_tooltipWindow;          // 0x6be5a4
extern HHOOK g_tooltipHook;           // 0x6be5a8
extern HINSTANCE g_stringModule;      // 0x6be5ac module LoadResourceString reads
extern HHOOK g_mouseHook;             // 0x6be5b0
extern HCURSOR g_defaultCursor;       // 0x6be5b4 cursor given to every window that does not set kCoolOwnCursor
extern HwndNode* g_captureStack;      // 0x6be598 linked through AvlNode::m_successor
extern NodeStack g_popupStack;        // 0x6be4e0 of PopupEntry
extern IdAvlTree g_windowRegistry;    // 0x6be4e8 ids are CoolWindow addresses
extern HPALETTE g_displayPalette;    // 0x6be4c0 the palette SetIdentityPalette installed (DibBitmap::DrawToDc selects it)

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(HwndNode, 0x20);
REB_ASSERT_SIZE(HwndTree, 0x08);
REB_ASSERT_SIZE(PopupEntry, 0x10);
REB_ASSERT_SIZE(ChildWindowTree, 0x08);
REB_ASSERT_SIZE(IdAvlTreeElement, 0x0c);
REB_ASSERT_SIZE(CoolWindow, 0x94);
REB_ASSERT_OFFSET(CoolWindow, m_hwnd, 0x18);
REB_ASSERT_OFFSET(CoolWindow, m_instance, 0x1c);
REB_ASSERT_OFFSET(CoolWindow, m_parent, 0x20);
REB_ASSERT_OFFSET(CoolWindow, m_childId, 0x24);
REB_ASSERT_OFFSET(CoolWindow, m_x, 0x28);
REB_ASSERT_OFFSET(CoolWindow, m_width, 0x30);
REB_ASSERT_OFFSET(CoolWindow, m_clientWidth, 0x38);
REB_ASSERT_OFFSET(CoolWindow, m_flags, 0x40);
REB_ASSERT_OFFSET(CoolWindow, m_className, 0x48);
REB_ASSERT_OFFSET(CoolWindow, m_style, 0x50);
REB_ASSERT_OFFSET(CoolWindow, m_background, 0x5c);
REB_ASSERT_OFFSET(CoolWindow, m_menu, 0x68);
REB_ASSERT_OFFSET(CoolWindow, m_children, 0x6c);
REB_ASSERT_OFFSET(CoolWindow, m_linkedWindows, 0x74);
REB_ASSERT_OFFSET(CoolWindow, m_font, 0x7c);
REB_ASSERT_OFFSET(CoolWindow, m_tooltipText, 0x84);
REB_ASSERT_OFFSET(CoolWindow, m_popupMembers, 0x88);
REB_ASSERT_OFFSET(CoolWindow, m_tooltipOwner, 0x90);
#pragma clang diagnostic pop
