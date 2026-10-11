#include "ui/bitmap_overlay.h"

#include "gfx/dib_bitmap.h"

BitmapOverlay::BitmapOverlay() : m_bitmap(nullptr), m_x(0), m_y(0) {}

BitmapOverlay::~BitmapOverlay() {
    if (m_bitmap) delete m_bitmap;
}

DibBitmap* BitmapOverlay::GetBitmap() { return m_bitmap; }

void BitmapOverlay::SetBitmap(DibBitmap* bitmap) { m_bitmap = bitmap; }

void BitmapOverlay::Read(istream* in) {
    ReadId(in);
    m_bitmap = nullptr;
}

void BitmapOverlay::Write(ostream* out) { WriteId(out); }

void BitmapOverlay::GetRect(RECT* rect) {
    DibBitmap* bitmap = GetBitmap();
    if (!bitmap) return;
    rect->left = m_x;
    rect->right = bitmap->Width() + m_x;
    rect->top = m_y;
    rect->bottom = bitmap->Height() + m_y;
}

BitmapOverlayTree::BitmapOverlayTree() = default;

BitmapOverlayTree::~BitmapOverlayTree() = default;

int BitmapOverlayTree::IsLess(AvlNode* a, AvlNode* b) {
    if (!a || !b) return 0;
    BitmapOverlay* first = static_cast<BitmapOverlay*>(a);
    BitmapOverlay* second = static_cast<BitmapOverlay*>(b);
    return first->GetBitmap()->Height() / 2 + first->m_y < second->GetBitmap()->Height() / 2 + second->m_y;
}

BitmapOverlay* BitmapOverlayTree::FindOverlay(uint32_t id) {
    AvlNode* node = First();
    while (node && static_cast<BitmapOverlay*>(node)->m_id != id) node = node->m_successor;
    return static_cast<BitmapOverlay*>(node);
}
