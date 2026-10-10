#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/layout.h"
#include "gameobj/object_key.h"
#include "platform/win32.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_game_window.h"

class CoolLabel;
class CoolSelectionBox;
class CoolStringField;
class ManufactureOrderBase;
class ObjectNameNode;
class String;

// The dialog (child 0x62 of the strategic window, gokres.dll pictures) the human player fills in to manufacture
// units: a selection box of the unit types the team can build, the quantity, and the totals of the order (what it
// costs and what is available). OK (Return) sends the order to the player (UiHumanReceiveMessage); the dialog
// remembers the last ship / troop / facility type chosen in g_lastShipType / g_lastTroopType / g_lastFacilityType.
class ManufactureOrderWindow : public CoolGameWindow {
public:
    ManufactureOrderWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                           uint32_t stringSlot, uint32_t bitmapSlot, uint32_t backgroundId, uint32_t childId,
                           uint32_t style, uint32_t flags, ManufactureOrderBase* order,
                           uint32_t unused);  // 0x437880
    virtual ~ManufactureOrderWindow();  // 0x437cb0 (scalar deleting destructor 0x437c90)

    virtual LRESULT REB_STDCALL WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;  // slot 5, 0x438780
    virtual void OnCreate1() override;                        // slot 14, 0x437f80: selection box, labels, quantity field
    virtual void OnCreate2() override;                        // slot 15, 0x438500: the static artwork and texts
    virtual void OnCreate3() override;                        // slot 16, 0x438620: the buttons
    virtual void DrawOverlay(HDC dc) override;                // slot 17, 0x438d80
    virtual void OnCommand(int a, int b, int c) override;     // slot 18, 0x438800
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x438b60
    virtual int Virtual14() override;                         // slot 20, 0x438b30: 1 when there is a choice and an order

    // Opens the dialog for `order` over the strategic window; 1 if it was opened.
    static int Open(ManufactureOrderBase* order, uint32_t unused);  // 0x437df0
    // Puts the quantity and the selected type into the order, remembers the type and hands the order to the player.
    void SendOrder();            // 0x438980
    void IncrementQuantity();    // 0x438c30
    void DecrementQuantity();    // 0x438c60
    void UpdateQuantityField();  // 0x438c90: writes m_quantity into the field
    void ReadQuantityField();    // 0x438d30: m_quantity from the field, clamped to 1..255
    // Shows `cost` and `available` (or "--" when `ok` is 0).
    void ShowCostLabels(int ok, int cost, int available);   // 0x438dd0
    void ShowUnitLabels(int ok, int cost, int available);   // 0x438f30
    // Recomputes what the order would cost for the selected type and quantity and shows it.
    void RefreshTotals();        // 0x439160

    CoolSelectionBox* m_selector;       // +0x114 owned
    CoolListItemList m_choices;         // +0x118 one item per buildable type (m_userData: a heap ObjectKey)
    CoolStringField* m_quantityField;   // +0x128 owned, child 0x6b
    CoolLabel* m_unitLabelA;            // +0x12c owned
    CoolLabel* m_unitLabelB;            // +0x130 owned
    CoolLabel* m_costLabelA;            // +0x134 owned
    CoolLabel* m_costLabelB;            // +0x138 owned
    uint32_t m_unknown13c;              // +0x13c
    uint32_t m_unknown140;              // +0x140
    ManufactureOrderBase* m_order;      // +0x144 the order being edited (owned until it is sent)
    int m_quantity;                     // +0x148 1..255
    HMODULE m_gokres;                   // +0x14c GOKRES.DLL (resource slot 10)
    NullableKey m_remembered;           // +0x150 the type chosen last time, selected when the dialog opens
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ManufactureOrderWindow, 0x154);
REB_ASSERT_OFFSET(ManufactureOrderWindow, m_selector, 0x114);
REB_ASSERT_OFFSET(ManufactureOrderWindow, m_choices, 0x118);
REB_ASSERT_OFFSET(ManufactureOrderWindow, m_quantityField, 0x128);
REB_ASSERT_OFFSET(ManufactureOrderWindow, m_order, 0x144);
REB_ASSERT_OFFSET(ManufactureOrderWindow, m_remembered, 0x150);
#pragma clang diagnostic pop

extern ManufactureOrderWindow* g_manufactureOrderWindow;  // 0x6b28ac
extern NullableKey g_lastFacilityType;                    // 0x6b289c
extern NullableKey g_lastTroopType;                       // 0x6b28a0
extern NullableKey g_lastShipType;                        // 0x6b28a4
