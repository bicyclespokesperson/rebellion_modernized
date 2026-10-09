#include "text_ops.h"

#include <stdarg.h>
#include <string.h>

#include "../../gdi/tests/canvas.h"

using namespace scenario;

namespace {

struct FontSpec {
    int height;
    int weight;
};

// The distinct (lfHeight, lfWeight) pairs of FontTable::CreateSlotFont (src/game/ui/resource_dll.cpp); face "Arial", PROOF_QUALITY.
const FontSpec kFonts[] = {{8, 400}, {12, 400}, {12, 900}, {14, 400}, {14, 700}, {16, 400}, {16, 700}, {18, 400}, {18, 700}, {24, 400}, {30, 400}};

std::string Unescape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            ++i;
            out.push_back(s[i] == 'n' ? '\n' : s[i] == 't' ? '\t' : s[i]);
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

HFONT MakeFont(const FontSpec& f) {
    LOGFONTA lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = f.height;
    lf.lfWeight = f.weight;
    lf.lfQuality = PROOF_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    return CreateFontIndirectA(&lf);
}

std::string Fmt(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
std::string Fmt(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return buf;
}

}  // namespace

bool RunTextScenarios(const char* samplesPath, const char* filter, TextResult* out) {
    std::vector<std::string> samples;
    {
        FILE* f = fopen(samplesPath, "rb");
        if (!f) return false;
        std::string line;
        int c;
        while ((c = fgetc(f)) != EOF) {
            if (c == '\n') {
                samples.push_back(Unescape(line));
                line.clear();
            } else {
                line.push_back(static_cast<char>(c));
            }
        }
        fclose(f);
    }
    std::vector<std::string> shortLines;  // single-line samples for rendering
    std::vector<std::string> longTexts;
    for (const std::string& s : samples) {
        if (s.find('\n') == std::string::npos && s.find('\t') == std::string::npos && s.size() >= 1 && s.size() <= 48) shortLines.push_back(s);
        if (s.size() > 80) longTexts.push_back(s);
    }
    if (shortLines.size() > 24) shortLines.resize(24);
    if (longTexts.size() > 3) longTexts.resize(3);

    for (const FontSpec& spec : kFonts) {
        std::string name = Fmt("f%dw%d", spec.height, spec.weight);
        if (filter && !strstr(name.c_str(), filter)) continue;
        Canvas c(480, static_cast<int>((spec.height + 6) * shortLines.size()));
        HFONT font = MakeFont(spec);
        HGDIOBJ old = SelectObject(c.dc, font);

        TEXTMETRICA tm;
        memset(&tm, 0, sizeof tm);
        GetTextMetricsA(c.dc, &tm);
        out->lines.push_back(Fmt("%s metrics h=%d asc=%d desc=%d intlead=%d extlead=%d avgw=%d maxw=%d weight=%d overhang=%d aspx=%d aspy=%d "
                                 "first=%d last=%d default=%d brk=%d italic=%d ul=%d so=%d pf=%d cs=%d",
                                 name.c_str(), static_cast<int>(tm.tmHeight), static_cast<int>(tm.tmAscent), static_cast<int>(tm.tmDescent),
                                 static_cast<int>(tm.tmInternalLeading), static_cast<int>(tm.tmExternalLeading), static_cast<int>(tm.tmAveCharWidth),
                                 static_cast<int>(tm.tmMaxCharWidth), static_cast<int>(tm.tmWeight), static_cast<int>(tm.tmOverhang),
                                 static_cast<int>(tm.tmDigitizedAspectX), static_cast<int>(tm.tmDigitizedAspectY), tm.tmFirstChar, tm.tmLastChar,
                                 tm.tmDefaultChar, tm.tmBreakChar, tm.tmItalic, tm.tmUnderlined, tm.tmStruckOut, tm.tmPitchAndFamily, tm.tmCharSet));

        for (const char* fixed : {"The quick brown fox jumps over the lazy dog", "Hello, World!", "WWW iii 0123456789", "Rebellion"}) {
            SIZE sz;
            GetTextExtentPoint32A(c.dc, fixed, static_cast<int>(strlen(fixed)), &sz);
            out->lines.push_back(Fmt("%s fixedextent [%s] %d %d", name.c_str(), fixed, static_cast<int>(sz.cx), static_cast<int>(sz.cy)));
            RECT r = {0, 0, 100, 0};
            int h = DrawTextA(c.dc, fixed, -1, &r, DT_WORDBREAK | DT_CALCRECT);
            out->lines.push_back(Fmt("%s fixedwrap [%s] %d,%d ret=%d", name.c_str(), fixed, static_cast<int>(r.right), static_cast<int>(r.bottom), h));
        }
        for (size_t i = 0; i < samples.size(); ++i) {
            const std::string& s = samples[i];
            size_t nl = s.find_first_of("\n\t");
            std::string line = s.substr(0, nl == std::string::npos ? s.size() : nl);
            if (line.empty()) continue;
            SIZE sz = {0, 0};
            GetTextExtentPoint32A(c.dc, line.c_str(), static_cast<int>(line.size()), &sz);
            out->lines.push_back(Fmt("%s extent %zu %d %d", name.c_str(), i, static_cast<int>(sz.cx), static_cast<int>(sz.cy)));
        }
        for (size_t k = 0; k < shortLines.size() && k < 4; ++k) {
            std::string widths;
            for (size_t n = 1; n <= shortLines[k].size(); ++n) {
                SIZE sz;
                GetTextExtentPoint32A(c.dc, shortLines[k].c_str(), static_cast<int>(n), &sz);
                widths += Fmt(" %d", static_cast<int>(sz.cx));
            }
            out->lines.push_back(Fmt("%s prefix %zu%s", name.c_str(), k, widths.c_str()));
        }
        for (size_t k = 0; k < longTexts.size(); ++k) {
            for (int width : {120, 200, 330}) {
                RECT r = {0, 0, width, 0};
                int h = DrawTextA(c.dc, longTexts[k].c_str(), -1, &r, DT_EDITCONTROL | DT_CALCRECT | DT_WORDBREAK);
                out->lines.push_back(Fmt("%s calcrect %zu w=%d -> %d,%d,%d,%d ret=%d", name.c_str(), k, width, static_cast<int>(r.left),
                                         static_cast<int>(r.top), static_cast<int>(r.right), static_cast<int>(r.bottom), h));
            }
        }
        for (size_t k = 0; k < shortLines.size() && k < 8; ++k) {
            RECT r = {0, 0, 0, 0};
            int h = DrawTextA(c.dc, shortLines[k].c_str(), -1, &r, DT_CALCRECT | DT_NOPREFIX | DT_SINGLELINE | DT_VCENTER);
            out->lines.push_back(Fmt("%s calcrect_single %zu -> %d,%d,%d,%d ret=%d", name.c_str(), k, static_cast<int>(r.left), static_cast<int>(r.top),
                                     static_cast<int>(r.right), static_cast<int>(r.bottom), h));
        }

        if (spec.weight == 400 && (spec.height == 12 || spec.height == 16)) {
            const char* wraps[] = {"The quick brown fox jumps over the lazy dog", "aa  bb   cc dd  ee ff gg hh", "Supercalifragilistic expialidocious words",
                                   "one two\nthree four five six\r\nseven eight nine ten",
                                   "brown", "brown ", "brown fox", "brown  fox", " brown fox", "brown fox jumps", "jumps over the"};
            for (int k = 0; k < 11; ++k) {
                std::string text = wraps[k];
                for (size_t p = 0; (p = text.find("\\n", p)) != std::string::npos;) text.replace(p, 2, "\n");
                if (k == 1) {
                    std::string ex;
                    for (const char* piece : {"a", "aa", "aa ", "aa  ", " ", "b", "aa  bb", "bb", "bb   ", "bb   cc", "cc"}) {
                        SIZE sz;
                        GetTextExtentPoint32A(c.dc, piece, static_cast<int>(strlen(piece)), &sz);
                        ex += Fmt(" [%s]=%d", piece, static_cast<int>(sz.cx));
                    }
                    out->lines.push_back(Fmt("%s wrapprobe%s", name.c_str(), ex.c_str()));
                }
                for (int variant = 0; variant < 2; ++variant) {
                    std::string rects;
                    for (int width = 20; width <= 120; width += (width < 70 ? 1 : 5)) {
                        RECT r = {0, 0, width, 0};
                        DrawTextA(c.dc, text.c_str(), -1, &r, DT_WORDBREAK | DT_CALCRECT | (variant ? DT_EDITCONTROL : 0));
                        rects += Fmt(" %d/%d", static_cast<int>(r.right), static_cast<int>(r.bottom));
                    }
                    out->lines.push_back(Fmt("%s wrapscan %d%s%s", name.c_str(), k, variant ? "e" : "", rects.c_str()));
                }
            }
        }

        const int rowHeight = spec.height + 6;
        // opaque TextOut: black on white
        c.Fill(255);
        SetTextColor(c.dc, RGB(0, 0, 0));
        SetBkColor(c.dc, RGB(255, 255, 255));
        SetBkMode(c.dc, OPAQUE);
        for (size_t k = 0; k < shortLines.size(); ++k)
            TextOutA(c.dc, 2 + static_cast<int>(k % 3), static_cast<int>(k) * rowHeight + 1, shortLines[k].c_str(), static_cast<int>(shortLines[k].size()));
        out->pixels.push_back(c.Snapshot((name + "_textout_opaque").c_str()));

        // transparent: yellow text over a busy background
        c.Pattern(11);
        SetTextColor(c.dc, RGB(255, 255, 0));
        SetBkMode(c.dc, TRANSPARENT);
        for (size_t k = 0; k < shortLines.size(); ++k)
            TextOutA(c.dc, 3, static_cast<int>(k) * rowHeight + 2, shortLines[k].c_str(), static_cast<int>(shortLines[k].size()));
        out->pixels.push_back(c.Snapshot((name + "_textout_transparent").c_str()));

        // DrawText: centred / right / left in a fixed box, with VCENTER, NOCLIP
        c.Fill(0);
        SetTextColor(c.dc, RGB(255, 255, 255));
        SetBkMode(c.dc, TRANSPARENT);
        const UINT formats[] = {DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX, DT_RIGHT | DT_VCENTER | DT_SINGLELINE, DT_LEFT | DT_BOTTOM | DT_SINGLELINE,
                                DT_CENTER | DT_SINGLELINE | DT_NOCLIP};
        for (size_t k = 0; k < shortLines.size(); ++k) {
            RECT r = {4, static_cast<LONG>(k * rowHeight), 150 + static_cast<LONG>(k % 4) * 80, static_cast<LONG>(k * rowHeight + rowHeight)};
            DrawTextA(c.dc, shortLines[k].c_str(), -1, &r, formats[k % 4]);
        }
        out->pixels.push_back(c.Snapshot((name + "_drawtext_aligned").c_str()));

        // word-wrapped paragraph
        if (!longTexts.empty()) {
            Canvas box(240, 260);
            HGDIOBJ oldBoxFont = SelectObject(box.dc, font);
            box.Fill(255);
            SetTextColor(box.dc, RGB(0, 0, 0));
            SetBkMode(box.dc, TRANSPARENT);
            RECT r = {5, 5, 200, 250};
            DrawTextA(box.dc, longTexts[0].c_str(), -1, &r, DT_WORDBREAK | DT_NOPREFIX);
            out->pixels.push_back(box.Snapshot((name + "_drawtext_wrap").c_str()));
            SelectObject(box.dc, oldBoxFont);
        }
        SelectObject(c.dc, old);
        DeleteObject(font);
    }
    return true;
}
