// GDI text: CreateFontIndirectA, GetTextMetricsA, GetTextExtentPoint32A, TextOutA, and user32's DrawTextA, on FreeType glyphs.
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/user32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "../gdi/gdi_internal.h"
#include "ft_font.h"

using namespace mwgdi;
using mwtext::FontInstance;

namespace {

constexpr UINT kTaRight = 2, kTaCenter = 6, kTaBottom = 8, kTaBaseline = 24;
constexpr UINT kDtExpandTabs = 0x40, kDtExternalLeading = 0x200, kDtEditControl = 0x2000;

void FreeInstance(void* p) { delete static_cast<FontInstance*>(p); }

FontInstance* InstanceOf(Font* font) {
    mwgdi::g_fontCacheFree = FreeInstance;
    if (!font->textCache) font->textCache = FontInstance::Create(font->lf);
    return static_cast<FontInstance*>(font->textCache);
}

struct Metrics0 {
    int ascent, descent, height, externalLeading, aveCharWidth;
};

int TextWidth(FontInstance* f, const unsigned char* s, int n) {
    int w = 0;
    for (int i = 0; i < n; ++i) w += f->GetGlyph(s[i]).extentAdvance;
    return w;
}

// Draws the glyphs of s[0..n) with the DC's text colour; baseline at `baseline`, pen at x. The clip is the DC's, intersected with `clipRect`.
void DrawGlyphs(Dc* dc, FontInstance* f, const unsigned char* s, int n, int x, int baseline, uint8_t color, const RECT* clipRect) {
    for (int i = 0; i < n; ++i) {
        const mwtext::Glyph& g = f->GetGlyph(s[i]);
        if (g.width > 0 && g.height > 0) {
            int left = x + g.left, top = baseline - g.top;
            int l = left, t = top, r = left + g.width, b = top + g.height;
            if (clipRect) {
                if (l < clipRect->left) l = clipRect->left;
                if (t < clipRect->top) t = clipRect->top;
                if (r > clipRect->right) r = clipRect->right;
                if (b > clipRect->bottom) b = clipRect->bottom;
            }
            ForEachClipped(dc, l, t, r, b, [&](int x0, int y0, int x1, int y1) {
                for (int py = y0; py < y1; ++py) {
                    uint8_t* row = dc->surface.Row(py);
                    int gy = py - dc->originY - top;
                    for (int px = x0; px < x1; ++px)
                        if (g.Pixel(px - dc->originX - left, gy)) row[px] = color;
                }
            });
        }
        x += g.advance;
    }
}

void FillBackground(Dc* dc, int l, int t, int r, int b, const RECT* clipRect) {
    if (dc->bkMode != 2) return;
    if (clipRect) {
        if (l < clipRect->left) l = clipRect->left;
        if (t < clipRect->top) t = clipRect->top;
        if (r > clipRect->right) r = clipRect->right;
        if (b > clipRect->bottom) b = clipRect->bottom;
    }
    FillSurfaceRect(dc, l, t, r, b, ResolveColor(dc, dc->bkColor), 13);
}

// One output line: text, its width and the index of the underlined character (prefix handling), or -1.
struct Line {
    std::string text;
    int width = 0;
    int underline = -1;
};

}  // namespace

extern "C" {

HFONT REB_STDCALL CreateFontIndirectA(const LOGFONTA* lplf) {
    if (!lplf) return nullptr;
    Font* f = new Font;
    f->lf = *lplf;
    f->lf.lfFaceName[31] = 0;
    if (const char* trace = getenv("MW_TEXT_TRACE"))
        if (FILE* log = fopen(trace, "a")) {
        fprintf(log, "CreateFont h=%d w=%d wt=%d it=%d ul=%d cs=%d q=%d pf=%d face=%s\n", static_cast<int>(lplf->lfHeight), static_cast<int>(lplf->lfWidth),
                static_cast<int>(lplf->lfWeight), lplf->lfItalic, lplf->lfUnderline, lplf->lfCharSet, lplf->lfQuality, lplf->lfPitchAndFamily, lplf->lfFaceName);
        fclose(log);
        }
    Register(f);
    return reinterpret_cast<HFONT>(f);
}

BOOL REB_STDCALL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm) {
    Dc* dc = DcOf(hdc);
    if (!dc || !lptm) return FALSE;
    FontInstance* f = InstanceOf(dc->font);
    if (!f) return FALSE;
    const mwtext::TextMetrics& m = f->Metrics();
    memset(lptm, 0, sizeof *lptm);
    lptm->tmHeight = m.height;
    lptm->tmAscent = m.ascent;
    lptm->tmDescent = m.descent;
    lptm->tmInternalLeading = m.internalLeading;
    lptm->tmExternalLeading = m.externalLeading;
    lptm->tmAveCharWidth = m.aveCharWidth;
    lptm->tmMaxCharWidth = m.maxCharWidth;
    lptm->tmWeight = m.weight;
    lptm->tmOverhang = 0;
    lptm->tmDigitizedAspectX = 96;
    lptm->tmDigitizedAspectY = 96;
    lptm->tmFirstChar = static_cast<BYTE>(m.firstChar);
    lptm->tmLastChar = static_cast<BYTE>(m.lastChar);
    lptm->tmDefaultChar = static_cast<BYTE>(m.defaultChar);
    lptm->tmBreakChar = static_cast<BYTE>(m.breakChar);
    lptm->tmItalic = dc->font->lf.lfItalic;
    lptm->tmUnderlined = dc->font->lf.lfUnderline;
    lptm->tmStruckOut = dc->font->lf.lfStrikeOut;
    lptm->tmPitchAndFamily = static_cast<BYTE>(m.pitchAndFamily);
    lptm->tmCharSet = static_cast<BYTE>(m.charSet);
    return TRUE;
}

BOOL REB_STDCALL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl) {
    Dc* dc = DcOf(hdc);
    if (!dc || !psizl || c < 0) return FALSE;
    FontInstance* f = InstanceOf(dc->font);
    if (!f) return FALSE;
    psizl->cx = TextWidth(f, reinterpret_cast<const unsigned char*>(lpString), c);
    psizl->cy = f->Metrics().height;
    return TRUE;
}

BOOL REB_STDCALL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
    Dc* dc = DcOf(hdc);
    if (!dc || c < 0) return FALSE;
    FontInstance* f = InstanceOf(dc->font);
    if (!f) return FALSE;
    const unsigned char* s = reinterpret_cast<const unsigned char*>(lpString);
    const mwtext::TextMetrics& m = f->Metrics();
    int width = TextWidth(f, s, c);
    UINT align = dc->textAlign;
    if ((align & kTaCenter) == kTaCenter) x -= width / 2;
    else if (align & kTaRight) x -= width;
    int top = y;
    if ((align & kTaBaseline) == kTaBaseline) top = y - m.ascent;
    else if (align & kTaBottom) top = y - m.height;
    FillBackground(dc, x, top, x + width, top + m.height, nullptr);
    DrawGlyphs(dc, f, s, c, x, top + m.ascent, ResolveColor(dc, dc->textColor), nullptr);
    return TRUE;
}

int REB_STDCALL DrawTextA(HDC hdc, LPCSTR lpchText, int cchText, LPRECT lprc, UINT format) {
    Dc* dc = DcOf(hdc);
    if (!dc || !lprc || !lpchText) return 0;
    FontInstance* f = InstanceOf(dc->font);
    if (!f) return 0;
    if (cchText < 0) cchText = static_cast<int>(strlen(lpchText));
    if (cchText == 0) return 0;
    const mwtext::TextMetrics& m = f->Metrics();
    const unsigned char* text = reinterpret_cast<const unsigned char*>(lpchText);
    const bool calc = (format & DT_CALCRECT) != 0;
    const bool single = (format & DT_SINGLELINE) != 0;
    bool wordBreak = (format & DT_WORDBREAK) != 0 && !single;
    const bool noClip = calc || (format & DT_NOCLIP) != 0;
    const int lineHeight = m.height + ((format & kDtExternalLeading) ? m.externalLeading : 0);
    const int tabWidth = m.aveCharWidth * 8;
    const int avail = lprc->right - lprc->left;

    // 1. prefix handling and line-break characters, leaving plain text plus the underlined positions
    std::string clean;
    std::vector<int> underlined;  // indices into `clean`
    for (int i = 0; i < cchText; ++i) {
        unsigned char ch = text[i];
        if (single && (ch == '\r' || ch == '\n')) continue;
        if (ch == '&' && !(format & DT_NOPREFIX)) {
            if (i + 1 < cchText && text[i + 1] == '&') {
                ++i;
            } else {
                if (i + 1 < cchText) underlined.push_back(static_cast<int>(clean.size()));
                continue;
            }
        }
        clean.push_back(static_cast<char>(ch));
    }

    // 2. lines
    auto advanceOf = [&](unsigned char ch, int x) {
        if (ch == '\t' && (format & kDtExpandTabs) && tabWidth > 0) return tabWidth - (x % tabWidth);
        return f->GetGlyph(ch).extentAdvance;
    };
    auto widthOf = [&](size_t from, size_t to) {
        int w = 0;
        for (size_t k = from; k < to; ++k) w += advanceOf(static_cast<unsigned char>(clean[k]), w);
        return w;
    };
    std::vector<Line> lines;
    {
        size_t pos = 0;
        const size_t total = clean.size();
        while (true) {
            size_t end = pos;
            while (end < total && clean[end] != '\r' && clean[end] != '\n') ++end;  // this line's segment is [pos, end)
            size_t lineEnd = end, next = end;
            if (wordBreak && widthOf(pos, end) > avail) {
                size_t fit = 0;  // characters of the segment whose right edge is inside the box
                for (int x = 0; pos + fit < end;) {
                    x += advanceOf(static_cast<unsigned char>(clean[pos + fit]), x);
                    if (x > avail) break;
                    ++fit;
                }
                if (fit >= 1 && clean[pos + fit - 1] == ' ') {
                    lineEnd = next = pos + fit;  // the space is the last character that fits: it stays on the line
                    while (next < end && clean[next] == ' ') ++next;
                } else if (pos + fit < end && clean[pos + fit] == ' ') {
                    lineEnd = pos + fit;  // the word ends exactly at the box edge: break at the following space
                    next = lineEnd + 1;
                } else {
                    size_t p = std::string::npos;
                    for (size_t k = pos + (fit ? fit - 1 : 0); k > pos + 1; --k)
                        if (clean[k - 1] == ' ') {
                            p = k - 1;
                            break;
                        }
                    if (p != std::string::npos) {
                        lineEnd = p;
                        next = p + 1;
                    } else if (format & kDtEditControl) {
                        lineEnd = next = pos + (fit ? fit : 1);
                    } else {
                        size_t q = pos + fit;
                        while (q < end && clean[q] != ' ') ++q;  // an over-long word keeps its own line
                        lineEnd = q;
                        next = q < end ? q + 1 : q;
                    }
                }
            }
            Line line;
            line.text = clean.substr(pos, lineEnd - pos);
            line.width = widthOf(pos, lineEnd);
            for (int u : underlined)
                if (u >= static_cast<int>(pos) && u < static_cast<int>(lineEnd)) line.underline = u - static_cast<int>(pos);
            lines.push_back(line);
            if (next < end) {
                pos = next;
                continue;
            }
            if (end >= total) break;
            if (clean[end] == '\r' && end + 1 < total && clean[end + 1] == '\n') ++end;
            else if (clean[end] == '\n' && end + 1 < total && clean[end + 1] == '\r') ++end;
            pos = end + 1;
            if (pos >= total) break;
        }
    }

    int y = lprc->top;
    int maxWidth = 0;
    RECT clip = *lprc;
    const RECT* clipPtr = noClip ? nullptr : &clip;
    uint8_t color = ResolveColor(dc, dc->textColor);
    for (const Line& line : lines) {
        if ((format & kDtEditControl) && !calc && y + lineHeight > lprc->bottom && y > lprc->top) break;
        int x;
        if (format & DT_CENTER) x = (lprc->left + lprc->right - line.width) / 2;
        else if (format & DT_RIGHT) x = lprc->right - line.width;
        else x = lprc->left;
        if (single) {
            if (format & DT_VCENTER) y = lprc->top + (lprc->bottom - lprc->top) / 2 - lineHeight / 2;
            else if (format & DT_BOTTOM) y = lprc->bottom - lineHeight;
        }
        int lineTop = y;
        if (line.width > maxWidth) maxWidth = line.width;
        if (!calc) {
            const unsigned char* s = reinterpret_cast<const unsigned char*>(line.text.data());
            FillBackground(dc, x, lineTop, x + line.width, lineTop + m.height, clipPtr);
            DrawGlyphs(dc, f, s, static_cast<int>(line.text.size()), x, lineTop + m.ascent, color, clipPtr);
            if (line.underline >= 0 && line.underline < static_cast<int>(line.text.size())) {
                int ux = x + TextWidth(f, s, line.underline);
                int uw = f->GetGlyph(s[line.underline]).extentAdvance;
                RECT u = {ux, lineTop + m.ascent + 1, ux + uw, lineTop + m.ascent + 2};
                if (clipPtr) {
                    if (u.left < clip.left) u.left = clip.left;
                    if (u.right > clip.right) u.right = clip.right;
                    if (u.top < clip.top || u.bottom > clip.bottom) u.right = u.left;
                }
                FillSurfaceRect(dc, u.left, u.top, u.right, u.bottom, color, 13);
            }
        }
        y += lineHeight;
    }
    if (calc) {
        lprc->right = lprc->left + maxWidth;
        lprc->bottom = y;
    }
    return y - lprc->top;
}

}  // extern "C"
