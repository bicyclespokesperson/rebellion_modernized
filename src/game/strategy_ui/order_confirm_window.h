#pragma once
#include <cstdint>

#include "core/game_string.h"
#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_game_window.h"

class CoolLabel;
class GalaxyWindow;
class PlayerMessage;
class TextScrollField;

// Stand-in (class not identified): what an order confirmation shows. Polymorphic (the window deletes it).
class ConfirmRequest {
public:
    virtual ~ConfirmRequest();

    uint8_t m_unk04[0x2a];       // +0x04
    uint16_t m_pictureId;        // +0x2e bitmap of the picture shown in the window
    uint8_t m_unk30[0x14];       // +0x30
    String m_title;              // +0x44
    String m_text;               // +0x50 the question, scrolled in a text field
    PlayerMessage* m_order;      // +0x5c the order to send when the player agrees
};

// The "do you want to ...?" window of the galaxy view (child type 0x10): a picture, a title, the text of the
// request and two buttons. Yes (child 0x14, Return) hands the request's order to the human player, No (0x15,
// Escape) discards it.
class OrderConfirmWindow : public CoolGameWindow {
public:
    OrderConfirmWindow(HINSTANCE instance, GalaxyWindow* parent, uint32_t childId, ConfirmRequest* request);  // 0x44f060
    virtual ~OrderConfirmWindow();  // 0x44f0f0 (scalar deleting destructor 0x44f0d0)

    virtual void OnCreate1() override;                           // slot 14, 0x44f180
    virtual void OnCommand(int a, int b, int c) override;        // slot 18, 0x44f5e0
    virtual void OnKeyDown(WPARAM key, LPARAM lParam) override;  // slot 19, 0x44f640

    GalaxyWindow* m_galaxy;         // +0x114
    ConfirmRequest* m_request;      // +0x118 owned
    TextScrollField* m_textField;   // +0x11c owned
    CoolLabel* m_titleLabel;        // +0x120 owned
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(OrderConfirmWindow, 0x124);
REB_ASSERT_OFFSET(OrderConfirmWindow, m_galaxy, 0x114);
REB_ASSERT_OFFSET(OrderConfirmWindow, m_titleLabel, 0x120);
REB_ASSERT_SIZE(ConfirmRequest, 0x60);
REB_ASSERT_OFFSET(ConfirmRequest, m_title, 0x44);
REB_ASSERT_OFFSET(ConfirmRequest, m_order, 0x5c);
#pragma clang diagnostic pop
