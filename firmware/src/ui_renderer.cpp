#include "ui_renderer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr int32_t SCREEN_SIZE = 466;
constexpr int32_t MAX_RPM     = 7000;
constexpr int32_t REDLINE_RPM = 5500;

constexpr uint32_t HEX_BLACK         = 0x000000;
constexpr uint32_t HEX_SURFACE       = 0x12151C;
constexpr uint32_t HEX_TRACK_DARK    = 0x1C2029;
constexpr uint32_t HEX_DIVIDER       = 0x262C38;
constexpr uint32_t HEX_ESPMOTO_ORANGE = 0xFF5500;
constexpr uint32_t HEX_WHITE         = 0xFFFFFF;
constexpr uint32_t HEX_SILVER        = 0xD0D4DC;
constexpr uint32_t HEX_MUTED         = 0x707888;
constexpr uint32_t HEX_GREEN         = 0x00E676;
constexpr uint32_t HEX_BLUE          = 0x00B0FF;
constexpr uint32_t HEX_AMBER         = 0xFFB300;
constexpr uint32_t HEX_RED           = 0xFF1744;

inline lv_color_t rgb(uint32_t hex) {
    return lv_color_hex(hex);
}

// ---------------------------------------------------------------------------
// European Latin diacritic wrapper around LVGL built-in Montserrat fonts
// ---------------------------------------------------------------------------

enum AccentType : uint8_t {
    ACC_NONE = 0,
    ACC_GRAVE,
    ACC_ACUTE,
    ACC_CIRCUMFLEX,
    ACC_TILDE,
    ACC_DIAERESIS,
    ACC_RING,
    ACC_CARON,
    ACC_CEDILLA,
    ACC_STROKE,
    ACC_MACRON,
    ACC_BREVE,
    ACC_DOT_ABOVE,
    ACC_DOUBLE_ACUTE,
    ACC_OGONEK
};

struct EuroGlyphMap {
    uint32_t codepoint;
    char baseAscii;
    AccentType accent;
    bool dotlessI;
};

constexpr EuroGlyphMap EURO_MAP[] = {
    // Latin-1 Supplement (U+00C0..U+00FF)
    {0x00C0, 'A', ACC_GRAVE, false},      {0x00C1, 'A', ACC_ACUTE, false},
    {0x00C2, 'A', ACC_CIRCUMFLEX, false}, {0x00C3, 'A', ACC_TILDE, false},
    {0x00C4, 'A', ACC_DIAERESIS, false},  {0x00C5, 'A', ACC_RING, false},
    {0x00C7, 'C', ACC_CEDILLA, false},    {0x00C8, 'E', ACC_GRAVE, false},
    {0x00C9, 'E', ACC_ACUTE, false},      {0x00CA, 'E', ACC_CIRCUMFLEX, false},
    {0x00CB, 'E', ACC_DIAERESIS, false},  {0x00CC, 'I', ACC_GRAVE, false},
    {0x00CD, 'I', ACC_ACUTE, false},      {0x00CE, 'I', ACC_CIRCUMFLEX, false},
    {0x00CF, 'I', ACC_DIAERESIS, false},  {0x00D0, 'D', ACC_STROKE, false},
    {0x00D1, 'N', ACC_TILDE, false},      {0x00D2, 'O', ACC_GRAVE, false},
    {0x00D3, 'O', ACC_ACUTE, false},      {0x00D4, 'O', ACC_CIRCUMFLEX, false},
    {0x00D5, 'O', ACC_TILDE, false},      {0x00D6, 'O', ACC_DIAERESIS, false},
    {0x00D8, 'O', ACC_STROKE, false},     {0x00D9, 'U', ACC_GRAVE, false},
    {0x00DA, 'U', ACC_ACUTE, false},      {0x00DB, 'U', ACC_CIRCUMFLEX, false},
    {0x00DC, 'U', ACC_DIAERESIS, false},  {0x00DD, 'Y', ACC_ACUTE, false},

    {0x00E0, 'a', ACC_GRAVE, false},      {0x00E1, 'a', ACC_ACUTE, false},
    {0x00E2, 'a', ACC_CIRCUMFLEX, false}, {0x00E3, 'a', ACC_TILDE, false},
    {0x00E4, 'a', ACC_DIAERESIS, false},  {0x00E5, 'a', ACC_RING, false},
    {0x00E7, 'c', ACC_CEDILLA, false},    {0x00E8, 'e', ACC_GRAVE, false},
    {0x00E9, 'e', ACC_ACUTE, false},      {0x00EA, 'e', ACC_CIRCUMFLEX, false},
    {0x00EB, 'e', ACC_DIAERESIS, false},  {0x00EC, 'i', ACC_GRAVE, true},
    {0x00ED, 'i', ACC_ACUTE, true},       {0x00EE, 'i', ACC_CIRCUMFLEX, true},
    {0x00EF, 'i', ACC_DIAERESIS, true},   {0x00F0, 'd', ACC_STROKE, false},
    {0x00F1, 'n', ACC_TILDE, false},      {0x00F2, 'o', ACC_GRAVE, false},
    {0x00F3, 'o', ACC_ACUTE, false},      {0x00F4, 'o', ACC_CIRCUMFLEX, false},
    {0x00F5, 'o', ACC_TILDE, false},      {0x00F6, 'o', ACC_DIAERESIS, false},
    {0x00F8, 'o', ACC_STROKE, false},     {0x00F9, 'u', ACC_GRAVE, false},
    {0x00FA, 'u', ACC_ACUTE, false},      {0x00FB, 'u', ACC_CIRCUMFLEX, false},
    {0x00FC, 'u', ACC_DIAERESIS, false},  {0x00FD, 'y', ACC_ACUTE, false},
    {0x00FF, 'y', ACC_DIAERESIS, false},

    // Latin Extended-A common European road-name characters
    {0x0100, 'A', ACC_MACRON, false},     {0x0101, 'a', ACC_MACRON, false},
    {0x0102, 'A', ACC_BREVE, false},      {0x0103, 'a', ACC_BREVE, false},
    {0x0104, 'A', ACC_OGONEK, false},     {0x0105, 'a', ACC_OGONEK, false},
    {0x0106, 'C', ACC_ACUTE, false},      {0x0107, 'c', ACC_ACUTE, false},
    {0x010C, 'C', ACC_CARON, false},      {0x010D, 'c', ACC_CARON, false},
    {0x010E, 'D', ACC_CARON, false},      {0x010F, 'd', ACC_CARON, false},
    {0x0110, 'D', ACC_STROKE, false},     {0x0111, 'd', ACC_STROKE, false},
    {0x0112, 'E', ACC_MACRON, false},     {0x0113, 'e', ACC_MACRON, false},
    {0x0116, 'E', ACC_DOT_ABOVE, false},  {0x0117, 'e', ACC_DOT_ABOVE, false},
    {0x0118, 'E', ACC_OGONEK, false},     {0x0119, 'e', ACC_OGONEK, false},
    {0x011A, 'E', ACC_CARON, false},      {0x011B, 'e', ACC_CARON, false},
    {0x011E, 'G', ACC_BREVE, false},      {0x011F, 'g', ACC_BREVE, false},
    {0x012A, 'I', ACC_MACRON, false},     {0x012B, 'i', ACC_MACRON, true},
    {0x012E, 'I', ACC_OGONEK, false},     {0x012F, 'i', ACC_OGONEK, false},
    {0x0130, 'I', ACC_DOT_ABOVE, false},  {0x0131, 'i', ACC_NONE, true},
    {0x0141, 'L', ACC_STROKE, false},     {0x0142, 'l', ACC_STROKE, false},
    {0x0143, 'N', ACC_ACUTE, false},      {0x0144, 'n', ACC_ACUTE, false},
    {0x0145, 'N', ACC_CEDILLA, false},    {0x0146, 'n', ACC_CEDILLA, false},
    {0x0147, 'N', ACC_CARON, false},      {0x0148, 'n', ACC_CARON, false},
    {0x0150, 'O', ACC_DOUBLE_ACUTE, false},{0x0151, 'o', ACC_DOUBLE_ACUTE, false},
    {0x0154, 'R', ACC_ACUTE, false},      {0x0155, 'r', ACC_ACUTE, false},
    {0x0158, 'R', ACC_CARON, false},      {0x0159, 'r', ACC_CARON, false},
    {0x015A, 'S', ACC_ACUTE, false},      {0x015B, 's', ACC_ACUTE, false},
    {0x015E, 'S', ACC_CEDILLA, false},    {0x015F, 's', ACC_CEDILLA, false},
    {0x0160, 'S', ACC_CARON, false},      {0x0161, 's', ACC_CARON, false},
    {0x0164, 'T', ACC_CARON, false},      {0x0165, 't', ACC_CARON, false},
    {0x016A, 'U', ACC_MACRON, false},     {0x016B, 'u', ACC_MACRON, false},
    {0x016E, 'U', ACC_RING, false},       {0x016F, 'u', ACC_RING, false},
    {0x0170, 'U', ACC_DOUBLE_ACUTE, false},{0x0171, 'u', ACC_DOUBLE_ACUTE, false},
    {0x0178, 'Y', ACC_DIAERESIS, false},  {0x0179, 'Z', ACC_ACUTE, false},
    {0x017A, 'z', ACC_ACUTE, false},      {0x017B, 'Z', ACC_DOT_ABOVE, false},
    {0x017C, 'z', ACC_DOT_ABOVE, false},  {0x017D, 'Z', ACC_CARON, false},
    {0x017E, 'z', ACC_CARON, false},
};

bool lookupEuroGlyph(uint32_t cp, char& baseAscii, AccentType& accent, bool& dotlessI) {
    for (const auto& entry : EURO_MAP) {
        if (entry.codepoint == cp) {
            baseAscii = entry.baseAscii;
            accent = entry.accent;
            dotlessI = entry.dotlessI;
            return true;
        }
    }
    return false;
}

uint32_t remapSimpleFallback(uint32_t cp) {
    if (cp == 0x00B7 || cp == 0x2219) return 0x2022; // middle dot -> bullet
    if (cp == 0x00BA) return 'o';
    if (cp == 0x00AA) return 'a';
    return cp;
}

uint32_t mapKerningChar(uint32_t cp) {
    cp = remapSimpleFallback(cp);
    char base = 0;
    AccentType acc = ACC_NONE;
    bool dotless = false;
    if (lookupEuroGlyph(cp, base, acc, dotless)) {
        return static_cast<uint32_t>(static_cast<unsigned char>(base));
    }
    return cp;
}

bool isTopAccent(AccentType acc) {
    return acc == ACC_GRAVE || acc == ACC_ACUTE || acc == ACC_CIRCUMFLEX ||
           acc == ACC_TILDE || acc == ACC_DIAERESIS || acc == ACC_RING ||
           acc == ACC_CARON || acc == ACC_MACRON || acc == ACC_BREVE ||
           acc == ACC_DOT_ABOVE || acc == ACC_DOUBLE_ACUTE;
}

void calcPadMetrics(int32_t lineHeight,
                    uint8_t oldBoxW,
                    AccentType acc,
                    bool isUpper,
                    bool dotlessI,
                    int16_t& leftPad,
                    int16_t& rightPad,
                    int16_t& topPad,
                    int16_t& botPad) {
    leftPad = 0;
    rightPad = 0;
    topPad = 0;
    botPad = 0;

    if (isTopAccent(acc)) {
        if (dotlessI) {
            // 'i' already has height above stem for dot; ensure minimum width for accents like ^ or diaeresis
            int16_t minW = (lineHeight >= 26) ? 11 : ((lineHeight >= 18) ? 9 : 7);
            if (oldBoxW < minW) {
                int16_t extra = minW - oldBoxW;
                leftPad = extra / 2;
                rightPad = extra - leftPad;
            }
            topPad = (lineHeight <= 16) ? 1 : 0;
        } else {
            if (lineHeight >= 26) {
                topPad = isUpper ? 5 : 6;
            } else if (lineHeight >= 18) {
                topPad = isUpper ? 3 : 4;
            } else {
                topPad = isUpper ? 2 : 3;
            }
            int16_t minW = (lineHeight >= 26) ? 10 : 7;
            if (oldBoxW < minW) {
                int16_t extra = minW - oldBoxW;
                leftPad = extra / 2;
                rightPad = extra - leftPad;
            }
        }
    } else if (acc == ACC_CEDILLA || acc == ACC_OGONEK) {
        botPad = (lineHeight >= 26) ? 5 : ((lineHeight >= 18) ? 4 : 3);
    } else if (acc == ACC_STROKE && oldBoxW < ((lineHeight >= 26) ? 10 : 7)) {
        int16_t minW = (lineHeight >= 26) ? 10 : 7;
        int16_t extra = minW - oldBoxW;
        leftPad = extra / 2;
        rightPad = extra - leftPad;
    }
}

inline uint32_t packEuroGid(AccentType acc, bool dotlessI, bool isUpper, bool isDStroke, uint32_t baseGid) {
    return 0x80000000U |
           (static_cast<uint32_t>(acc) << 16) |
           (dotlessI ? (1U << 15) : 0U) |
           (isUpper ? (1U << 14) : 0U) |
           (isDStroke ? (1U << 13) : 0U) |
           (baseGid & 0x1FFFU);
}

void drawMaxPixel(uint8_t* dst, uint32_t stride, int32_t w, int32_t h, int32_t x, int32_t y, uint8_t alpha) {
    if (x < 0 || x >= w || y < 0 || y >= h || alpha == 0) {
        return;
    }
    uint8_t& cell = dst[static_cast<uint32_t>(y) * stride + static_cast<uint32_t>(x)];
    if (alpha > cell) {
        cell = alpha;
    }
}

void drawAASegment(uint8_t* dst,
                   uint32_t stride,
                   int32_t w,
                   int32_t h,
                   float x0,
                   float y0,
                   float x1,
                   float y1,
                   float radius) {
    float minXf = std::min(x0, x1) - radius - 1.2f;
    float maxXf = std::max(x0, x1) + radius + 1.2f;
    float minYf = std::min(y0, y1) - radius - 1.2f;
    float maxYf = std::max(y0, y1) + radius + 1.2f;

    int32_t ix0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(minXf)));
    int32_t ix1 = std::min<int32_t>(w - 1, static_cast<int32_t>(std::ceil(maxXf)));
    int32_t iy0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(minYf)));
    int32_t iy1 = std::min<int32_t>(h - 1, static_cast<int32_t>(std::ceil(maxYf)));

    float dx = x1 - x0;
    float dy = y1 - y0;
    float lenSq = dx * dx + dy * dy;

    for (int32_t py = iy0; py <= iy1; ++py) {
        float cy = static_cast<float>(py) + 0.5f;
        for (int32_t px = ix0; px <= ix1; ++px) {
            float cx = static_cast<float>(px) + 0.5f;
            float t = (lenSq > 1e-5f) ? (((cx - x0) * dx + (cy - y0) * dy) / lenSq) : 0.0f;
            t = std::max(0.0f, std::min(1.0f, t));
            float projX = x0 + t * dx;
            float projY = y0 + t * dy;
            float dist = std::sqrt((cx - projX) * (cx - projX) + (cy - projY) * (cy - projY));
            float cov = (radius + 0.55f - dist) / 1.1f;
            if (cov > 0.0f) {
                cov = std::min(1.0f, cov);
                drawMaxPixel(dst, stride, w, h, px, py, static_cast<uint8_t>(std::lround(cov * 255.0f)));
            }
        }
    }
}

void drawAADot(uint8_t* dst,
               uint32_t stride,
               int32_t w,
               int32_t h,
               float cx,
               float cy,
               float radius) {
    drawAASegment(dst, stride, w, h, cx, cy, cx, cy, radius);
}

void drawAccentGlyph(uint8_t* dst,
                     uint32_t stride,
                     int32_t boxW,
                     int32_t boxH,
                     int32_t topAreaH,
                     int32_t origBoxH,
                     AccentType acc,
                     int32_t lineHeight,
                     char baseAscii) {
    const float strokeR = (lineHeight >= 26) ? 1.05f : ((lineHeight >= 18) ? 0.85f : 0.65f);
    const float cx = static_cast<float>(boxW) * 0.5f;
    const float spanX = std::min(static_cast<float>(boxW) * 0.34f, (lineHeight >= 26) ? 5.2f : 3.6f);
    const float yTop = 0.7f;
    const float yBot = std::max(2.2f, static_cast<float>(topAreaH) - 1.7f);
    const float yMid = (yTop + yBot) * 0.5f;

    switch (acc) {
        case ACC_GRAVE:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX * 0.65f, yTop, cx + spanX * 0.45f, yBot, strokeR);
            break;
        case ACC_ACUTE:
            drawAASegment(dst, stride, boxW, boxH, cx + spanX * 0.65f, yTop, cx - spanX * 0.45f, yBot, strokeR);
            break;
        case ACC_DOUBLE_ACUTE:
            drawAASegment(dst, stride, boxW, boxH, cx - 0.8f, yTop, cx - 2.6f, yBot, strokeR * 0.9f);
            drawAASegment(dst, stride, boxW, boxH, cx + 2.6f, yTop, cx + 0.8f, yBot, strokeR * 0.9f);
            break;
        case ACC_CIRCUMFLEX:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX, yBot, cx, yTop, strokeR * 0.95f);
            drawAASegment(dst, stride, boxW, boxH, cx, yTop, cx + spanX, yBot, strokeR * 0.95f);
            break;
        case ACC_CARON:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX, yTop, cx, yBot, strokeR * 0.95f);
            drawAASegment(dst, stride, boxW, boxH, cx, yBot, cx + spanX, yTop, strokeR * 0.95f);
            break;
        case ACC_DIAERESIS: {
            float sep = std::min(static_cast<float>(boxW) * 0.27f, (lineHeight >= 26) ? 3.4f : 2.3f);
            float r = (lineHeight >= 26) ? 1.35f : ((lineHeight >= 18) ? 1.05f : 0.85f);
            drawAADot(dst, stride, boxW, boxH, cx - sep, yMid, r);
            drawAADot(dst, stride, boxW, boxH, cx + sep, yMid, r);
            break;
        }
        case ACC_DOT_ABOVE: {
            float r = (lineHeight >= 26) ? 1.45f : ((lineHeight >= 18) ? 1.15f : 0.9f);
            drawAADot(dst, stride, boxW, boxH, cx, yMid, r);
            break;
        }
        case ACC_TILDE:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX, yMid + 0.7f, cx - spanX * 0.35f, yMid - 0.8f, strokeR * 0.88f);
            drawAASegment(dst, stride, boxW, boxH, cx - spanX * 0.35f, yMid - 0.8f, cx + spanX * 0.35f, yMid + 0.8f, strokeR * 0.88f);
            drawAASegment(dst, stride, boxW, boxH, cx + spanX * 0.35f, yMid + 0.8f, cx + spanX, yMid - 0.7f, strokeR * 0.88f);
            break;
        case ACC_MACRON:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX, yMid, cx + spanX, yMid, strokeR * 0.9f);
            break;
        case ACC_BREVE:
            drawAASegment(dst, stride, boxW, boxH, cx - spanX, yTop + 0.4f, cx - spanX * 0.4f, yBot - 0.3f, strokeR * 0.85f);
            drawAASegment(dst, stride, boxW, boxH, cx - spanX * 0.4f, yBot - 0.3f, cx + spanX * 0.4f, yBot - 0.3f, strokeR * 0.85f);
            drawAASegment(dst, stride, boxW, boxH, cx + spanX * 0.4f, yBot - 0.3f, cx + spanX, yTop + 0.4f, strokeR * 0.85f);
            break;
        case ACC_RING: {
            float ringR = std::max(1.4f, (yBot - yTop) * 0.48f);
            float thick = strokeR * 0.78f;
            int32_t iy0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(yMid - ringR - 2.0f)));
            int32_t iy1 = std::min<int32_t>(boxH - 1, static_cast<int32_t>(std::ceil(yMid + ringR + 2.0f)));
            int32_t ix0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(cx - ringR - 2.0f)));
            int32_t ix1 = std::min<int32_t>(boxW - 1, static_cast<int32_t>(std::ceil(cx + ringR + 2.0f)));
            for (int32_t py = iy0; py <= iy1; ++py) {
                for (int32_t px = ix0; px <= ix1; ++px) {
                    float d = std::hypot(static_cast<float>(px) + 0.5f - cx, static_cast<float>(py) + 0.5f - yMid);
                    float cov = (thick + 0.5f - std::fabs(d - ringR)) / 1.0f;
                    if (cov > 0.0f) {
                        cov = std::min(1.0f, cov);
                        drawMaxPixel(dst, stride, boxW, boxH, px, py, static_cast<uint8_t>(std::lround(cov * 255.0f)));
                    }
                }
            }
            break;
        }
        case ACC_CEDILLA: {
            float neckTop = static_cast<float>(origBoxH) - 1.0f;
            float hookMidY = neckTop + static_cast<float>(boxH - origBoxH) * 0.45f;
            float hookBot = static_cast<float>(boxH) - 0.8f;
            float r = strokeR * 0.82f;
            drawAASegment(dst, stride, boxW, boxH, cx + 0.4f, neckTop, cx + 0.4f, hookMidY, r);
            drawAASegment(dst, stride, boxW, boxH, cx + 0.4f, hookMidY, cx + 1.6f, hookBot - 0.4f, r);
            drawAASegment(dst, stride, boxW, boxH, cx + 1.6f, hookBot - 0.4f, cx - 1.4f, hookBot, r);
            break;
        }
        case ACC_OGONEK: {
            float startX = cx + static_cast<float>(boxW) * 0.16f;
            float neckTop = static_cast<float>(origBoxH) - 1.2f;
            float botY = static_cast<float>(boxH) - 0.8f;
            float r = strokeR * 0.82f;
            drawAASegment(dst, stride, boxW, boxH, startX, neckTop, startX - 0.8f, botY - 0.4f, r);
            drawAASegment(dst, stride, boxW, boxH, startX - 0.8f, botY - 0.4f, startX + 1.8f, botY, r);
            break;
        }
        case ACC_STROKE: {
            if (baseAscii == 'd' || baseAscii == 'D') {
                float yBar = (baseAscii == 'D') ? static_cast<float>(boxH) * 0.50f : static_cast<float>(boxH) * 0.30f;
                drawAASegment(dst, stride, boxW, boxH, 0.8f, yBar, static_cast<float>(boxW) * 0.55f, yBar, strokeR * 0.9f);
            } else {
                drawAASegment(dst, stride, boxW, boxH, 1.0f, static_cast<float>(boxH) * 0.78f,
                              static_cast<float>(boxW) - 1.0f, static_cast<float>(boxH) * 0.22f, strokeR * 0.9f);
            }
            break;
        }
        case ACC_NONE:
        default:
            break;
    }
}

bool euroGetGlyphDsc(const lv_font_t* font,
                     lv_font_glyph_dsc_t* dsc_out,
                     uint32_t letter,
                     uint32_t letter_next) {
    const auto* baseFont = static_cast<const lv_font_t*>(font->user_data);
    if (!baseFont) {
        return false;
    }

    uint32_t mappedNext = mapKerningChar(letter_next);
    uint32_t simpleCp = remapSimpleFallback(letter);
    if (baseFont->get_glyph_dsc(baseFont, dsc_out, simpleCp, mappedNext)) {
        return true;
    }

    char baseAscii = 0;
    AccentType acc = ACC_NONE;
    bool dotlessI = false;
    if (!lookupEuroGlyph(letter, baseAscii, acc, dotlessI)) {
        return false;
    }

    if (!baseFont->get_glyph_dsc(baseFont, dsc_out, static_cast<uint32_t>(static_cast<unsigned char>(baseAscii)), mappedNext)) {
        return false;
    }

    if (acc == ACC_NONE && !dotlessI) {
        return true;
    }

    const uint32_t baseGid = dsc_out->gid.index;
    const bool isUpper = (baseAscii >= 'A' && baseAscii <= 'Z');
    const bool isDStroke = (baseAscii == 'd' || baseAscii == 'D');
    int16_t leftPad = 0;
    int16_t rightPad = 0;
    int16_t topPad = 0;
    int16_t botPad = 0;
    calcPadMetrics(baseFont->line_height, static_cast<uint8_t>(dsc_out->box_w), acc, isUpper, dotlessI,
                   leftPad, rightPad, topPad, botPad);

    dsc_out->ofs_x = static_cast<int16_t>(dsc_out->ofs_x - leftPad);
    dsc_out->box_w = static_cast<uint16_t>(dsc_out->box_w + leftPad + rightPad);
    dsc_out->box_h = static_cast<uint16_t>(dsc_out->box_h + topPad + botPad);
    dsc_out->ofs_y = static_cast<int16_t>(dsc_out->ofs_y - botPad);
    if (dotlessI && baseFont->line_height >= 24 && dsc_out->adv_w < 10) {
        dsc_out->adv_w = 10;
    }

    dsc_out->gid.index = packEuroGid(acc, dotlessI, isUpper, isDStroke, baseGid);
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    dsc_out->is_placeholder = false;
    return true;
}

const uint8_t* unpackGlyph4bppOpaTable() {
    static constexpr uint8_t OPA4_TABLE[16] = {
        0, 17, 34, 51, 68, 85, 102, 119,
        136, 153, 170, 187, 204, 221, 238, 255
    };
    return OPA4_TABLE;
}

void unpackGlyph4bpp(const lv_font_fmt_txt_dsc_t* fdsc,
                     const lv_font_fmt_txt_glyph_dsc_t& gdsc,
                     uint8_t* dst,
                     uint32_t dstStride) {
    if (fdsc->bpp != 4 || fdsc->bitmap_format != LV_FONT_FMT_TXT_PLAIN || gdsc.box_w == 0 || gdsc.box_h == 0) {
        return;
    }
    const uint8_t* opa4 = unpackGlyph4bppOpaTable();
    const uint8_t* bitmapIn = &fdsc->glyph_bitmap[gdsc.bitmap_index];
    int32_t nibble = 0;
    for (int32_t y = 0; y < gdsc.box_h; ++y) {
        uint8_t* rowPtr = dst + static_cast<uint32_t>(y) * dstStride;
        for (int32_t x = 0; x < gdsc.box_w; ++x, ++nibble) {
            uint8_t val = ((nibble & 1) == 0) ? ((*bitmapIn) >> 4) : (*bitmapIn & 0x0F);
            rowPtr[x] = opa4[val];
            if ((nibble & 1) == 1) {
                ++bitmapIn;
            }
        }
    }
}

const void* euroGetGlyphBitmap(lv_font_glyph_dsc_t* g_dsc, lv_draw_buf_t* draw_buf) {
    const lv_font_t* wrapper = g_dsc->resolved_font;
    const auto* baseFont = static_cast<const lv_font_t*>(wrapper ? wrapper->user_data : nullptr);
    if (!baseFont || !draw_buf) {
        return nullptr;
    }

    const uint32_t gidTag = g_dsc->gid.index;
    if ((gidTag & 0x80000000U) == 0U) {
        g_dsc->resolved_font = baseFont;
        const void* res = baseFont->get_glyph_bitmap(g_dsc, draw_buf);
        g_dsc->resolved_font = wrapper;
        return res;
    }

    const AccentType acc = static_cast<AccentType>((gidTag >> 16) & 0x1FU);
    const bool dotlessI = ((gidTag >> 15) & 1U) != 0U;
    const bool isUpper = ((gidTag >> 14) & 1U) != 0U;
    const bool isDStroke = ((gidTag >> 13) & 1U) != 0U;
    const uint32_t baseGid = gidTag & 0x1FFFU;

    const auto* fdsc = static_cast<const lv_font_fmt_txt_dsc_t*>(baseFont->dsc);
    if (!fdsc || baseGid == 0) {
        return nullptr;
    }
    const lv_font_fmt_txt_glyph_dsc_t& gdsc = fdsc->glyph_dsc[baseGid];

    int16_t leftPad = 0;
    int16_t rightPad = 0;
    int16_t topPad = 0;
    int16_t botPad = 0;
    calcPadMetrics(baseFont->line_height, gdsc.box_w, acc, isUpper, dotlessI,
                   leftPad, rightPad, topPad, botPad);

    const uint32_t dstStride = draw_buf->header.stride;
    uint8_t* dst = draw_buf->data;
    std::memset(dst, 0, dstStride * g_dsc->box_h);

    unpackGlyph4bpp(fdsc, gdsc, dst + static_cast<uint32_t>(topPad) * dstStride + leftPad, dstStride);

    int32_t topAreaH = topPad;
    if (dotlessI) {
        // Erase the dot of 'i' above its stem gap and reuse that space for the diacritic
        int32_t yStart = topPad;
        int32_t yLimit = topPad + gdsc.box_h;
        int32_t firstInk = -1;
        int32_t gapRow = yStart;
        for (int32_t y = yStart; y < yLimit; ++y) {
            const uint8_t* rowPtr = dst + static_cast<uint32_t>(y) * dstStride;
            bool hasInk = false;
            for (int32_t x = 0; x < g_dsc->box_w; ++x) {
                if (rowPtr[x] > 16) {
                    hasInk = true;
                    break;
                }
            }
            if (hasInk && firstInk < 0) {
                firstInk = y;
            } else if (!hasInk && firstInk >= 0) {
                gapRow = y;
                break;
            }
        }
        if (firstInk >= 0 && gapRow > firstInk) {
            for (int32_t y = 0; y <= gapRow; ++y) {
                std::memset(dst + static_cast<uint32_t>(y) * dstStride, 0, g_dsc->box_w);
            }
            topAreaH = std::max<int32_t>(gapRow, (baseFont->line_height >= 26) ? 6 : 4);
        }
    }

    char baseCharHint = isDStroke ? (isUpper ? 'D' : 'd') : 'o';
    if (acc != ACC_NONE) {
        drawAccentGlyph(dst, dstStride, g_dsc->box_w, g_dsc->box_h,
                        topAreaH, gdsc.box_h, acc, baseFont->line_height, baseCharHint);
    }

    return draw_buf;
}

lv_font_t makeEuroFont(const lv_font_t* baseFont) {
    lv_font_t f = *baseFont;
    f.get_glyph_dsc = euroGetGlyphDsc;
    f.get_glyph_bitmap = euroGetGlyphBitmap;
    f.release_glyph = nullptr;
    f.fallback = baseFont->fallback;
    f.user_data = const_cast<lv_font_t*>(baseFont);
    return f;
}

const lv_font_t* getEuroFont28() {
    static lv_font_t f = makeEuroFont(&lv_font_montserrat_28);
    return &f;
}

const lv_font_t* getEuroFont20() {
    static lv_font_t f = makeEuroFont(&lv_font_montserrat_20);
    return &f;
}

const lv_font_t* getEuroFont14() {
    static lv_font_t f = makeEuroFont(&lv_font_montserrat_14);
    return &f;
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80U) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800U) {
        out.push_back(static_cast<char>(0xC0U | (cp >> 6)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else if (cp < 0x10000U) {
        out.push_back(static_cast<char>(0xE0U | (cp >> 12)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    } else {
        out.push_back(static_cast<char>(0xF0U | (cp >> 18)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 12) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | ((cp >> 6) & 0x3FU)));
        out.push_back(static_cast<char>(0x80U | (cp & 0x3FU)));
    }
}

uint32_t combineNfdCodepoint(uint32_t baseCp, uint32_t mark) {
    switch (mark) {
        case 0x0300: // COMBINING GRAVE
            switch (baseCp) {
                case 'A': return 0x00C0; case 'E': return 0x00C8; case 'I': return 0x00CC;
                case 'O': return 0x00D2; case 'U': return 0x00D9; case 'a': return 0x00E0;
                case 'e': return 0x00E8; case 'i': return 0x00EC; case 'o': return 0x00F2;
                case 'u': return 0x00F9; default: return 0;
            }
        case 0x0301: // COMBINING ACUTE
            switch (baseCp) {
                case 'A': return 0x00C1; case 'E': return 0x00C9; case 'I': return 0x00CD;
                case 'O': return 0x00D3; case 'U': return 0x00DA; case 'Y': return 0x00DD;
                case 'a': return 0x00E1; case 'e': return 0x00E9; case 'i': return 0x00ED;
                case 'o': return 0x00F3; case 'u': return 0x00FA; case 'y': return 0x00FD;
                case 'C': return 0x0106; case 'c': return 0x0107; case 'N': return 0x0143;
                case 'n': return 0x0144; case 'S': return 0x015A; case 's': return 0x015B;
                case 'Z': return 0x0179; case 'z': return 0x017A; default: return 0;
            }
        case 0x0302: // COMBINING CIRCUMFLEX
            switch (baseCp) {
                case 'A': return 0x00C2; case 'E': return 0x00CA; case 'I': return 0x00CE;
                case 'O': return 0x00D4; case 'U': return 0x00DB; case 'a': return 0x00E2;
                case 'e': return 0x00EA; case 'i': return 0x00EE; case 'o': return 0x00F4;
                case 'u': return 0x00FB; default: return 0;
            }
        case 0x0303: // COMBINING TILDE
            switch (baseCp) {
                case 'A': return 0x00C3; case 'N': return 0x00D1; case 'O': return 0x00D5;
                case 'a': return 0x00E3; case 'n': return 0x00F1; case 'o': return 0x00F5;
                default: return 0;
            }
        case 0x0304: // COMBINING MACRON
            switch (baseCp) {
                case 'A': return 0x0100; case 'a': return 0x0101; case 'E': return 0x0112;
                case 'e': return 0x0113; case 'I': return 0x012A; case 'i': return 0x012B;
                case 'U': return 0x016A; case 'u': return 0x016B; default: return 0;
            }
        case 0x0306: // COMBINING BREVE
            switch (baseCp) {
                case 'A': return 0x0102; case 'a': return 0x0103;
                case 'G': return 0x011E; case 'g': return 0x011F; default: return 0;
            }
        case 0x0307: // COMBINING DOT ABOVE
            switch (baseCp) {
                case 'E': return 0x0116; case 'e': return 0x0117; case 'I': return 0x0130;
                case 'Z': return 0x017B; case 'z': return 0x017C; default: return 0;
            }
        case 0x0308: // COMBINING DIAERESIS
            switch (baseCp) {
                case 'A': return 0x00C4; case 'E': return 0x00CB; case 'I': return 0x00CF;
                case 'O': return 0x00D6; case 'U': return 0x00DC; case 'Y': return 0x0178;
                case 'a': return 0x00E4; case 'e': return 0x00EB; case 'i': return 0x00EF;
                case 'o': return 0x00F6; case 'u': return 0x00FC; case 'y': return 0x00FF;
                default: return 0;
            }
        case 0x030A: // COMBINING RING ABOVE
            switch (baseCp) {
                case 'A': return 0x00C5; case 'a': return 0x00E5;
                case 'U': return 0x016E; case 'u': return 0x016F; default: return 0;
            }
        case 0x030C: // COMBINING CARON
            switch (baseCp) {
                case 'C': return 0x010C; case 'c': return 0x010D; case 'D': return 0x010E;
                case 'd': return 0x010F; case 'E': return 0x011A; case 'e': return 0x011B;
                case 'N': return 0x0147; case 'n': return 0x0148; case 'R': return 0x0158;
                case 'r': return 0x0159; case 'S': return 0x0160; case 's': return 0x0161;
                case 'T': return 0x0164; case 't': return 0x0165; case 'Z': return 0x017D;
                case 'z': return 0x017E; default: return 0;
            }
        case 0x0327: // COMBINING CEDILLA
            switch (baseCp) {
                case 'C': return 0x00C7; case 'c': return 0x00E7; case 'N': return 0x0145;
                case 'n': return 0x0146; case 'S': return 0x015E; case 's': return 0x015F;
                default: return 0;
            }
        case 0x0328: // COMBINING OGONEK
            switch (baseCp) {
                case 'A': return 0x0104; case 'a': return 0x0105; case 'E': return 0x0118;
                case 'e': return 0x0119; case 'I': return 0x012E; case 'i': return 0x012F;
                default: return 0;
            }
        default:
            return 0;
    }
}

bool isSupportedDisplayCodepoint(uint32_t cp) {
    if ((cp >= 0x20U && cp <= 0x7EU) || cp == 0x00B0U || cp == 0x2022U) {
        return true;
    }
    char base = 0;
    AccentType acc = ACC_NONE;
    bool dotless = false;
    return lookupEuroGlyph(cp, base, acc, dotless);
}

std::string normalizeUtf8Text(const char* raw) {
    if (!raw) {
        return {};
    }

    std::string out;
    const auto* p = reinterpret_cast<const uint8_t*>(raw);
    uint32_t prevCp = 0;
    bool hasPrev = false;

    auto flushPrev = [&]() {
        if (!hasPrev) return;
        if (isSupportedDisplayCodepoint(prevCp)) {
            appendUtf8(out, prevCp);
        }
        hasPrev = false;
        prevCp = 0;
    };

    while (*p != 0) {
        uint32_t cp = 0;
        if ((*p & 0x80U) == 0U) {
            cp = *p++;
        } else if ((*p & 0xE0U) == 0xC0U && (p[1] & 0xC0U) == 0x80U) {
            cp = (static_cast<uint32_t>(p[0] & 0x1FU) << 6) | (p[1] & 0x3FU);
            p += 2;
        } else if ((*p & 0xF0U) == 0xE0U && (p[1] & 0xC0U) == 0x80U && (p[2] & 0xC0U) == 0x80U) {
            cp = (static_cast<uint32_t>(p[0] & 0x0FU) << 12) |
                 (static_cast<uint32_t>(p[1] & 0x3FU) << 6) |
                 (p[2] & 0x3FU);
            p += 3;
        } else if ((*p & 0xF8U) == 0xF0U && (p[1] & 0xC0U) == 0x80U && (p[2] & 0xC0U) == 0x80U && (p[3] & 0xC0U) == 0x80U) {
            cp = (static_cast<uint32_t>(p[0] & 0x07U) << 18) |
                 (static_cast<uint32_t>(p[1] & 0x3FU) << 12) |
                 (static_cast<uint32_t>(p[2] & 0x3FU) << 6) |
                 (p[3] & 0x3FU);
            p += 4;
        } else {
            ++p;
            continue;
        }

        // Normalize common typographic punctuation / ligatures before checking combining marks
        if (cp == 0x00A0U || cp == 0x2009U || cp == 0x202FU) cp = ' ';
        else if (cp == 0x00B7U || cp == 0x2219U) cp = 0x2022U;
        else if (cp == 0x2013U || cp == 0x2014U || cp == 0x2212U) cp = '-';
        else if (cp == 0x2018U || cp == 0x2019U || cp == 0x02BCU) cp = '\'';
        else if (cp == 0x201CU || cp == 0x201DU) cp = '"';
        else if (cp == 0x00BAU) cp = 'o';
        else if (cp == 0x00AAU) cp = 'a';

        if (cp == 0x2026U) { // ellipsis
            flushPrev();
            out += "...";
            continue;
        }
        if (cp == 0x00DFU) { // German Eszett
            flushPrev();
            out += "ss";
            continue;
        }
        if (cp == 0x00C6U) { flushPrev(); out += "AE"; continue; }
        if (cp == 0x00E6U) { flushPrev(); out += "ae"; continue; }
        if (cp == 0x0152U) { flushPrev(); out += "OE"; continue; }
        if (cp == 0x0153U) { flushPrev(); out += "oe"; continue; }

        if (cp >= 0x0300U && cp <= 0x036FU) {
            if (hasPrev) {
                uint32_t combined = combineNfdCodepoint(prevCp, cp);
                if (combined != 0U) {
                    prevCp = combined;
                }
            }
            continue;
        }

        flushPrev();
        prevCp = cp;
        hasPrev = true;
    }

    flushPrev();
    return out;
}

// ---------------------------------------------------------------------------
// European roundabout exit resolution
// ---------------------------------------------------------------------------

bool isRoundaboutManeuver(ManeuverType type) {
    return type == ManeuverType::ROUNDABOUT ||
           (type >= ManeuverType::ROUNDABOUT_EXIT_1 && type <= ManeuverType::ROUNDABOUT_EXIT_5);
}

uint8_t extractRoundaboutExitFromText(const char* street) {
    if (!street || street[0] == '\0') {
        return 0;
    }

    std::string lower;
    for (const char* p = street; *p != '\0'; ++p) {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(*p))));
    }

    static const char* PREFIX_KEYS[] = {
        "exit #", "exit ", "sortida #", "sortida ", "salida #", "salida ",
        "sortie #", "sortie ", "uscita #", "uscita ", "ausfahrt #", "ausfahrt "
    };

    for (const char* key : PREFIX_KEYS) {
        std::size_t pos = 0;
        const std::size_t keyLen = std::strlen(key);
        while ((pos = lower.find(key, pos)) != std::string::npos) {
            bool wordBoundary = (pos == 0) || !std::isalpha(static_cast<unsigned char>(lower[pos - 1]));
            std::size_t numPos = pos + keyLen;
            while (numPos < lower.size() && lower[numPos] == ' ') {
                ++numPos;
            }
            if (wordBoundary && numPos < lower.size() && lower[numPos] >= '1' && lower[numPos] <= '9') {
                return static_cast<uint8_t>(lower[numPos] - '0');
            }
            pos += keyLen;
        }
    }

    // Ordinal patterns like "1st exit", "2nd exit", "3rd exit", "4th exit"
    for (std::size_t i = 0; i + 4 < lower.size(); ++i) {
        if (lower[i] >= '1' && lower[i] <= '9') {
            bool boundary = (i == 0) || !std::isdigit(static_cast<unsigned char>(lower[i - 1]));
            if (boundary &&
                (lower.compare(i + 1, 7, "st exit") == 0 ||
                 lower.compare(i + 1, 7, "nd exit") == 0 ||
                 lower.compare(i + 1, 7, "rd exit") == 0 ||
                 lower.compare(i + 1, 7, "th exit") == 0)) {
                return static_cast<uint8_t>(lower[i] - '0');
            }
        }
    }

    return 0;
}

uint8_t resolveRoundaboutExit(ManeuverType type, uint8_t explicitExit, const char* street) {
    if (type >= ManeuverType::ROUNDABOUT_EXIT_1 && type <= ManeuverType::ROUNDABOUT_EXIT_5) {
        return static_cast<uint8_t>(static_cast<int>(type) - static_cast<int>(ManeuverType::ROUNDABOUT_EXIT_1) + 1);
    }
    if (!isRoundaboutManeuver(type)) {
        return 0;
    }
    if (explicitExit > 0) {
        return explicitExit;
    }
    uint8_t parsed = extractRoundaboutExitFromText(street);
    return (parsed > 0) ? parsed : 2;
}

lv_obj_t* createPageContainer(lv_obj_t* parent) {
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_center(cont);
    lv_obj_set_style_bg_color(cont, rgb(HEX_BLACK), 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(cont, LV_OBJ_FLAG_CLICKABLE);
    return cont;
}

lv_obj_t* createArcGauge(lv_obj_t* parent,
                         int32_t size,
                         int32_t width,
                         uint32_t trackHex,
                         uint32_t indHex,
                         int32_t startDeg,
                         int32_t endDeg,
                         int32_t maxVal) {
    lv_obj_t* arc = lv_arc_create(parent);
    lv_obj_set_size(arc, size, size);
    lv_obj_center(arc);
    lv_arc_set_bg_angles(arc, startDeg, endDeg);
    lv_arc_set_range(arc, 0, maxVal);
    lv_arc_set_value(arc, 0);

    lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, rgb(trackHex), LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);

    lv_obj_set_style_arc_width(arc, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, rgb(indHex), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);

    lv_obj_set_style_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    return arc;
}

lv_obj_t* createPillBadge(lv_obj_t* parent, int32_t yOffset, int32_t padH, int32_t padV) {
    lv_obj_t* pill = lv_obj_create(parent);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, yOffset);
    lv_obj_set_style_bg_color(pill, rgb(HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, rgb(HEX_DIVIDER), 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, padH, 0);
    lv_obj_set_style_pad_ver(pill, padV, 0);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(pill, LV_OBJ_FLAG_CLICKABLE);
    return pill;
}

lv_obj_t* createStrokeLine(lv_obj_t* parent, int32_t width, uint32_t colorHex) {
    lv_obj_t* line = lv_line_create(parent);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, rgb(colorHex), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_remove_flag(line, LV_OBJ_FLAG_CLICKABLE);
    return line;
}

lv_obj_t* createCenteredLabel(lv_obj_t* parent,
                              const lv_font_t* font,
                              uint32_t colorHex,
                              int32_t yOffset) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, rgb(colorHex), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, yOffset);
    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

void setPoints(lv_point_precise_t* dst, const lv_point_precise_t* src, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        dst[i] = src[i];
    }
}

bool hero72GetGlyphDsc(const lv_font_t* font,
                       lv_font_glyph_dsc_t* dsc_out,
                       uint32_t letter,
                       uint32_t letter_next) {
    const auto* baseFont = static_cast<const lv_font_t*>(font ? font->user_data : nullptr);
    if (!baseFont || !baseFont->get_glyph_dsc(baseFont, dsc_out, letter, letter_next)) {
        return false;
    }

    dsc_out->resolved_font = font;
    dsc_out->adv_w = static_cast<uint16_t>((dsc_out->adv_w * 3 + 1) / 2);
    if (dsc_out->box_w > 0 && dsc_out->box_h > 0) {
        dsc_out->box_w = static_cast<uint16_t>((dsc_out->box_w * 3 + 1) / 2);
        dsc_out->box_h = static_cast<uint16_t>((dsc_out->box_h * 3 + 1) / 2);
        const int32_t scaledOfsX = static_cast<int32_t>(dsc_out->ofs_x) * 3;
        dsc_out->ofs_x = static_cast<int16_t>((scaledOfsX >= 0) ? ((scaledOfsX + 1) / 2) : ((scaledOfsX - 1) / 2));
        const int32_t scaledOfsY = static_cast<int32_t>(dsc_out->ofs_y) * 3;
        dsc_out->ofs_y = static_cast<int16_t>((scaledOfsY >= 0) ? ((scaledOfsY + 1) / 2) : ((scaledOfsY - 1) / 2));
        dsc_out->format = LV_FONT_GLYPH_FORMAT_A8;
    }
    return true;
}

const void* hero72GetGlyphBitmap(lv_font_glyph_dsc_t* g_dsc, lv_draw_buf_t* draw_buf) {
    const lv_font_t* wrapper = g_dsc ? g_dsc->resolved_font : nullptr;
    const auto* baseFont = static_cast<const lv_font_t*>(wrapper ? wrapper->user_data : nullptr);
    if (!baseFont || !draw_buf || !draw_buf->data) {
        return nullptr;
    }

    const uint32_t gid = g_dsc->gid.index;
    const auto* fdsc = static_cast<const lv_font_fmt_txt_dsc_t*>(baseFont->dsc);
    if (!fdsc || gid == 0 || g_dsc->box_w == 0 || g_dsc->box_h == 0) {
        return nullptr;
    }

    const lv_font_fmt_txt_glyph_dsc_t& gdsc = fdsc->glyph_dsc[gid];
    const int32_t srcW = gdsc.box_w;
    const int32_t srcH = gdsc.box_h;
    const int32_t dstW = g_dsc->box_w;
    const int32_t dstH = g_dsc->box_h;
    if (srcW <= 0 || srcH <= 0) {
        return nullptr;
    }

    constexpr int32_t MAX_SRC_PIXELS = 8192;
    if (srcW * srcH > MAX_SRC_PIXELS) {
        return nullptr;
    }

    static uint8_t srcBuf[MAX_SRC_PIXELS];
    std::memset(srcBuf, 0, static_cast<size_t>(srcW * srcH));
    unpackGlyph4bpp(fdsc, gdsc, srcBuf, static_cast<uint32_t>(srcW));

    const uint32_t dstStride = draw_buf->header.stride;
    uint8_t* dst = draw_buf->data;
    std::memset(dst, 0, dstStride * static_cast<uint32_t>(dstH));

    for (int32_t dy = 0; dy < dstH; ++dy) {
        float sy = (static_cast<float>(dy) + 0.5f) * static_cast<float>(srcH) / static_cast<float>(dstH) - 0.5f;
        if (sy < 0.0f) {
            sy = 0.0f;
        } else if (sy > static_cast<float>(srcH - 1)) {
            sy = static_cast<float>(srcH - 1);
        }
        const int32_t y0 = static_cast<int32_t>(sy);
        const int32_t y1 = (y0 + 1 < srcH) ? (y0 + 1) : y0;
        const float fy = sy - static_cast<float>(y0);

        uint8_t* dstRow = dst + static_cast<uint32_t>(dy) * dstStride;
        const uint8_t* srcRow0 = srcBuf + y0 * srcW;
        const uint8_t* srcRow1 = srcBuf + y1 * srcW;

        for (int32_t dx = 0; dx < dstW; ++dx) {
            float sx = (static_cast<float>(dx) + 0.5f) * static_cast<float>(srcW) / static_cast<float>(dstW) - 0.5f;
            if (sx < 0.0f) {
                sx = 0.0f;
            } else if (sx > static_cast<float>(srcW - 1)) {
                sx = static_cast<float>(srcW - 1);
            }
            const int32_t x0 = static_cast<int32_t>(sx);
            const int32_t x1 = (x0 + 1 < srcW) ? (x0 + 1) : x0;
            const float fx = sx - static_cast<float>(x0);

            const float v00 = static_cast<float>(srcRow0[x0]);
            const float v10 = static_cast<float>(srcRow0[x1]);
            const float v01 = static_cast<float>(srcRow1[x0]);
            const float v11 = static_cast<float>(srcRow1[x1]);

            const float top = v00 + (v10 - v00) * fx;
            const float bot = v01 + (v11 - v01) * fx;
            const float a = top + (bot - top) * fy;

            // Smoothstep sharpen [80..175] so 1.5x upscaled curves stay crisp without bilinear halo
            float t = (a - 80.0f) / (175.0f - 80.0f);
            if (t < 0.0f) {
                t = 0.0f;
            } else if (t > 1.0f) {
                t = 1.0f;
            }
            const float s = t * t * (3.0f - 2.0f * t);
            dstRow[dx] = static_cast<uint8_t>(s * 255.0f + 0.5f);
        }
    }

    return draw_buf;
}

const lv_font_t* getHeroFont72() {
    static lv_font_t f = []() {
        const lv_font_t* base = &lv_font_montserrat_48;
        lv_font_t font = *base;
        font.get_glyph_dsc = hero72GetGlyphDsc;
        font.get_glyph_bitmap = hero72GetGlyphBitmap;
        font.release_glyph = nullptr;
        font.line_height = (base->line_height * 3 + 1) / 2;
        font.base_line = (base->base_line * 3 + 1) / 2;
        font.underline_position = static_cast<int8_t>((base->underline_position * 3) / 2);
        font.underline_thickness = static_cast<int8_t>((base->underline_thickness * 3 + 1) / 2);
        font.fallback = nullptr;
        font.user_data = const_cast<lv_font_t*>(base);
        return font;
    }();
    return &f;
}

} // namespace

void UiRenderer::begin() {
    lv_obj_t* root = lv_screen_active();
    lv_obj_set_style_bg_color(root, rgb(HEX_BLACK), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, &UiRenderer::onScreenClicked, LV_EVENT_CLICKED, this);
    lv_obj_add_event_cb(root, &UiRenderer::onScreenTouchEvent, LV_EVENT_PRESSED, this);
    lv_obj_add_event_cb(root, &UiRenderer::onScreenTouchEvent, LV_EVENT_PRESSING, this);
    lv_obj_add_event_cb(root, &UiRenderer::onScreenTouchEvent, LV_EVENT_RELEASED, this);

    buildNavScreen(root);
    buildTelemetryScreen(root);
    buildSettingsScreen(root);
    buildPairingScreen(root);

    _initialized = true;
    _forceIconRefresh = true;
    updatePageVisibility();
}

void UiRenderer::onScreenClicked(lv_event_t* e) {
    auto* self = static_cast<UiRenderer*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_longPressTriggered) {
        self->_longPressTriggered = false;
        return;
    }
    if (self->_page == ScreenPage::NAVIGATION || self->_page == ScreenPage::TELEMETRY) {
        self->nextPage();
    }
}

void UiRenderer::onScreenTouchEvent(lv_event_t* e) {
    auto* self = static_cast<UiRenderer*>(lv_event_get_user_data(e));
    if (!self) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) {
        self->_pressStartMs = lv_tick_get();
        self->_isPressing = true;
        self->_longPressTriggered = false;
    } else if (code == LV_EVENT_PRESSING) {
        if (self->_isPressing && !self->_longPressTriggered &&
            (self->_page == ScreenPage::NAVIGATION || self->_page == ScreenPage::TELEMETRY)) {
            if (lv_tick_elaps(self->_pressStartMs) >= 10000U) {
                self->_longPressTriggered = true;
                self->_isPressing = false;
                self->setPage(ScreenPage::SETTINGS);
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        self->_isPressing = false;
    }
}

void UiRenderer::onSettingsPairClicked(lv_event_t* e) {
    auto* self = static_cast<UiRenderer*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_resetPairingCb) {
        self->_resetPairingCb(self->_resetPairingUserData);
    }
    self->setPage(ScreenPage::PAIRING);
}

void UiRenderer::onBackToMainClicked(lv_event_t* e) {
    auto* self = static_cast<UiRenderer*>(lv_event_get_user_data(e));
    if (!self) return;
    self->setPage(self->_returnPage);
}

void UiRenderer::nextPage() {
    setPage((_page == ScreenPage::NAVIGATION) ? ScreenPage::TELEMETRY : ScreenPage::NAVIGATION);
}

void UiRenderer::setPage(ScreenPage page) {
    if (_page == page) {
        return;
    }
    if (_page == ScreenPage::NAVIGATION || _page == ScreenPage::TELEMETRY) {
        _returnPage = _page;
    }
    _page = page;
    if (_initialized) {
        updatePageVisibility();
    }
}

void UiRenderer::setPairingPayload(const char* payload, const char* subtitle) {
    if (_pairingQr && payload && payload[0] != '\0') {
        lv_qrcode_update(_pairingQr, payload, static_cast<uint32_t>(std::strlen(payload)));
    }
    if (_pairingSubtitleLabel) {
        lv_label_set_text(_pairingSubtitleLabel, (subtitle && subtitle[0] != '\0') ? subtitle : "Scan with espMoto app");
    }
}

void UiRenderer::setResetPairingCallback(ResetPairingCallback cb, void* userData) {
    _resetPairingCb = cb;
    _resetPairingUserData = userData;
}

void UiRenderer::updatePageVisibility() {
    auto setVisible = [](lv_obj_t* obj, bool visible) {
        if (!obj) return;
        if (visible) {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
        }
    };

    setVisible(_navCont, _page == ScreenPage::NAVIGATION);
    setVisible(_telCont, _page == ScreenPage::TELEMETRY);
    setVisible(_settingsCont, _page == ScreenPage::SETTINGS);
    setVisible(_pairingCont, _page == ScreenPage::PAIRING);
    lv_obj_invalidate(lv_screen_active());
}

void UiRenderer::buildNavScreen(lv_obj_t* root) {
    _navCont = createPageContainer(root);

    // Subtle outer circular frame
    _navBezelArc = createArcGauge(_navCont, 458, 2, HEX_TRACK_DARK, HEX_TRACK_DARK, 0, 360, 100);
    lv_obj_set_style_arc_opa(_navBezelArc, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Top status pill
    _navStatusPill = createPillBadge(_navCont, 28, 14, 4);
    _navStatusLabel = lv_label_create(_navStatusPill);
    lv_obj_set_style_text_font(_navStatusLabel, getEuroFont14(), 0);
    lv_obj_set_style_text_letter_space(_navStatusLabel, 1, 0);
    lv_label_set_text(_navStatusLabel, "DEMO");
    lv_obj_center(_navStatusLabel);

    // Maneuver vector icon container (128x128 centered horizontally)
    _navIconBox = lv_obj_create(_navCont);
    lv_obj_set_size(_navIconBox, 128, 128);
    lv_obj_align(_navIconBox, LV_ALIGN_TOP_MID, 0, 74);
    lv_obj_set_style_bg_opa(_navIconBox, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_navIconBox, 0, 0);
    lv_obj_set_style_pad_all(_navIconBox, 0, 0);
    lv_obj_remove_flag(_navIconBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_navIconBox, LV_OBJ_FLAG_CLICKABLE);

    _navRbtBaseRing = createArcGauge(_navIconBox, 64, 11, HEX_DIVIDER, HEX_DIVIDER, 0, 360, 100);
    lv_obj_set_style_arc_opa(_navRbtBaseRing, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_align(_navRbtBaseRing, LV_ALIGN_CENTER, 0, -2);
    lv_obj_add_flag(_navRbtBaseRing, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < 3; ++i) {
        _navRbtStub[i] = createStrokeLine(_navIconBox, 8, HEX_DIVIDER);
        lv_obj_add_flag(_navRbtStub[i], LV_OBJ_FLAG_HIDDEN);
    }

    _navArrowRing = createArcGauge(_navIconBox, 64, 12, HEX_WHITE, HEX_WHITE, 0, 360, 100);
    lv_obj_set_style_arc_opa(_navArrowRing, LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_align(_navArrowRing, LV_ALIGN_CENTER, 0, -2);
    lv_obj_add_flag(_navArrowRing, LV_OBJ_FLAG_HIDDEN);

    _navArrowShaft = createStrokeLine(_navIconBox, 15, HEX_WHITE);
    _navArrowAux   = createStrokeLine(_navIconBox, 15, HEX_WHITE);
    _navArrowHead  = createStrokeLine(_navIconBox, 15, HEX_WHITE);

    _navRbtExitLabel = lv_label_create(_navIconBox);
    lv_obj_set_style_text_font(_navRbtExitLabel, getEuroFont20(), 0);
    lv_obj_set_style_text_color(_navRbtExitLabel, rgb(HEX_ESPMOTO_ORANGE), 0);
    lv_obj_align(_navRbtExitLabel, LV_ALIGN_CENTER, 0, -2);
    lv_label_set_text(_navRbtExitLabel, "2");
    lv_obj_remove_flag(_navRbtExitLabel, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_navRbtExitLabel, LV_OBJ_FLAG_HIDDEN);

    // Distance countdown
    _navDistLabel = createCenteredLabel(_navCont, &lv_font_montserrat_48, HEX_WHITE, 222);
    lv_obj_set_width(_navDistLabel, 380);
    lv_label_set_text(_navDistLabel, "--");

    // Street name (circular scroll if longer than bezel width)
    _navStreetLabel = createCenteredLabel(_navCont, getEuroFont28(), HEX_ESPMOTO_ORANGE, 290);
    lv_obj_set_width(_navStreetLabel, 360);
    lv_label_set_long_mode(_navStreetLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(_navStreetLabel, "Waiting for phone...");

    // Divider line
    _dividerPts[0] = {0, 0};
    _dividerPts[1] = {200, 0};
    _navDivider = createStrokeLine(_navCont, 2, HEX_DIVIDER);
    lv_line_set_points(_navDivider, _dividerPts, 2);
    lv_obj_align(_navDivider, LV_ALIGN_TOP_MID, 0, 344);

    // Bottom ETA badge
    _navEtaPill = createPillBadge(_navCont, 368, 18, 7);
    _navEtaLabel = lv_label_create(_navEtaPill);
    lv_obj_set_style_text_font(_navEtaLabel, getEuroFont20(), 0);
    lv_obj_set_style_text_color(_navEtaLabel, rgb(HEX_SILVER), 0);
    lv_label_set_text(_navEtaLabel, "ETA --:--");
    lv_obj_center(_navEtaLabel);
}

void UiRenderer::buildTelemetryScreen(lv_obj_t* root) {
    _telCont = createPageContainer(root);

    // Full 270-degree RPM tachometer arc (bold outer sweep)
    _telRpmArc = createArcGauge(_telCont, 444, 22, HEX_TRACK_DARK, HEX_ESPMOTO_ORANGE, 135, 45, MAX_RPM);
    _telInnerArc = createArcGauge(_telCont, 390, 2, HEX_DIVIDER, HEX_DIVIDER, 135, 45, 100);
    lv_obj_set_style_arc_opa(_telInnerArc, LV_OPA_TRANSP, LV_PART_INDICATOR);

    // Header status (bike name suppressed)
    _telHeaderLabel = createCenteredLabel(_telCont, getEuroFont14(), HEX_ESPMOTO_ORANGE, 56);
    lv_obj_set_style_text_letter_space(_telHeaderLabel, 1, 0);
    lv_label_set_text(_telHeaderLabel, "DEMO");

    // Hero 72pt numeric RPM readout
    _telRpmLabel = createCenteredLabel(_telCont, getHeroFont72(), HEX_WHITE, 82);
    lv_obj_set_width(_telRpmLabel, 340);
    lv_label_set_text(_telRpmLabel, "0");

    _telUnitLabel = createCenteredLabel(_telCont, &lv_font_montserrat_16, HEX_ESPMOTO_ORANGE, 160);
    lv_obj_set_style_text_letter_space(_telUnitLabel, 3, 0);
    lv_label_set_text(_telUnitLabel, "RPM");

    // Larger gear badge (prominent, secondary to hero RPM)
    _telGearBadge = lv_obj_create(_telCont);
    lv_obj_set_size(_telGearBadge, 86, 86);
    lv_obj_align(_telGearBadge, LV_ALIGN_TOP_MID, 0, 194);
    lv_obj_set_style_bg_color(_telGearBadge, rgb(HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(_telGearBadge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_telGearBadge, 3, 0);
    lv_obj_set_style_border_color(_telGearBadge, rgb(HEX_GREEN), 0);
    lv_obj_set_style_radius(_telGearBadge, 20, 0);
    lv_obj_remove_flag(_telGearBadge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(_telGearBadge, LV_OBJ_FLAG_CLICKABLE);

    _telGearLabel = lv_label_create(_telGearBadge);
    lv_obj_set_style_text_font(_telGearLabel, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_telGearLabel, rgb(HEX_GREEN), 0);
    lv_label_set_text(_telGearLabel, "N");
    lv_obj_center(_telGearLabel);

    // Bottom diagnostics: Engine Temp + Battery Volts
    _telTempLabel = lv_label_create(_telCont);
    lv_obj_set_style_text_font(_telTempLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_telTempLabel, rgb(HEX_GREEN), 0);
    lv_obj_align(_telTempLabel, LV_ALIGN_TOP_MID, -68, 338);
    lv_label_set_text(_telTempLabel, "92\xC2\xB0" "C");
    lv_obj_remove_flag(_telTempLabel, LV_OBJ_FLAG_CLICKABLE);

    _telVoltLabel = lv_label_create(_telCont);
    lv_obj_set_style_text_font(_telVoltLabel, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(_telVoltLabel, rgb(HEX_GREEN), 0);
    lv_obj_align(_telVoltLabel, LV_ALIGN_TOP_MID, 68, 338);
    lv_label_set_text(_telVoltLabel, "13.8V");
    lv_obj_remove_flag(_telVoltLabel, LV_OBJ_FLAG_CLICKABLE);

    _telAuxLabel = createCenteredLabel(_telCont, &lv_font_montserrat_16, HEX_SILVER, 372);
    lv_label_set_text(_telAuxLabel, "");
    lv_obj_add_flag(_telAuxLabel, LV_OBJ_FLAG_HIDDEN);
}

void UiRenderer::buildSettingsScreen(lv_obj_t* root) {
    _settingsCont = createPageContainer(root);

    lv_obj_t* bezel = createArcGauge(_settingsCont, 458, 2, HEX_TRACK_DARK, HEX_TRACK_DARK, 0, 360, 100);
    lv_obj_set_style_arc_opa(bezel, LV_OPA_TRANSP, LV_PART_INDICATOR);

    lv_obj_t* title = createCenteredLabel(_settingsCont, &lv_font_montserrat_20, HEX_ESPMOTO_ORANGE, 76);
    lv_label_set_text(title, "SETTINGS");

    lv_obj_t* pairBtn = lv_button_create(_settingsCont);
    lv_obj_set_size(pairBtn, 260, 64);
    lv_obj_align(pairBtn, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_style_bg_color(pairBtn, rgb(HEX_ESPMOTO_ORANGE), 0);
    lv_obj_set_style_bg_opa(pairBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pairBtn, 32, 0);
    lv_obj_set_style_border_width(pairBtn, 0, 0);
    lv_obj_set_style_shadow_width(pairBtn, 0, 0);
    lv_obj_add_event_cb(pairBtn, &UiRenderer::onSettingsPairClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* pairLbl = lv_label_create(pairBtn);
    lv_label_set_text(pairLbl, "SHOW PAIRING");
    lv_obj_set_style_text_font(pairLbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(pairLbl, rgb(HEX_WHITE), 0);
    lv_obj_center(pairLbl);

    lv_obj_t* backBtn = lv_button_create(_settingsCont);
    lv_obj_set_size(backBtn, 160, 48);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -56);
    lv_obj_set_style_bg_color(backBtn, rgb(HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(backBtn, 24, 0);
    lv_obj_set_style_border_width(backBtn, 2, 0);
    lv_obj_set_style_border_color(backBtn, rgb(HEX_DIVIDER), 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, &UiRenderer::onBackToMainClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "BACK");
    lv_obj_set_style_text_font(backLbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(backLbl, rgb(HEX_SILVER), 0);
    lv_obj_center(backLbl);
}

void UiRenderer::buildPairingScreen(lv_obj_t* root) {
    _pairingCont = createPageContainer(root);

    lv_obj_t* bezel = createArcGauge(_pairingCont, 458, 2, HEX_ESPMOTO_ORANGE, HEX_ESPMOTO_ORANGE, 0, 360, 100);
    lv_obj_set_style_arc_opa(bezel, LV_OPA_TRANSP, LV_PART_INDICATOR);

    lv_obj_t* title = createCenteredLabel(_pairingCont, &lv_font_montserrat_20, HEX_ESPMOTO_ORANGE, 44);
    lv_label_set_text(title, "SCAN TO PAIR");

    lv_obj_t* qrCard = lv_obj_create(_pairingCont);
    lv_obj_set_size(qrCard, 232, 232);
    lv_obj_align(qrCard, LV_ALIGN_CENTER, 0, -8);
    lv_obj_set_style_bg_color(qrCard, rgb(HEX_WHITE), 0);
    lv_obj_set_style_bg_opa(qrCard, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(qrCard, 16, 0);
    lv_obj_set_style_border_width(qrCard, 0, 0);
    lv_obj_set_style_pad_all(qrCard, 12, 0);
    lv_obj_remove_flag(qrCard, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(qrCard, LV_OBJ_FLAG_CLICKABLE);

    _pairingQr = lv_qrcode_create(qrCard);
    lv_qrcode_set_size(_pairingQr, 208);
    lv_qrcode_set_dark_color(_pairingQr, rgb(HEX_BLACK));
    lv_qrcode_set_light_color(_pairingQr, rgb(HEX_WHITE));
    const char* defaultPayload = "espmoto://pair?mac=00:00:00:00:00:00&token=0000000000000000&name=espMoto";
    lv_qrcode_update(_pairingQr, defaultPayload, static_cast<uint32_t>(std::strlen(defaultPayload)));
    lv_obj_center(_pairingQr);

    _pairingSubtitleLabel = createCenteredLabel(_pairingCont, &lv_font_montserrat_14, HEX_SILVER, 352);
    lv_label_set_text(_pairingSubtitleLabel, "Open espMoto app -> Scan QR");

    lv_obj_t* backBtn = lv_button_create(_pairingCont);
    lv_obj_set_size(backBtn, 140, 42);
    lv_obj_align(backBtn, LV_ALIGN_BOTTOM_MID, 0, -34);
    lv_obj_set_style_bg_color(backBtn, rgb(HEX_SURFACE), 0);
    lv_obj_set_style_bg_opa(backBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(backBtn, 21, 0);
    lv_obj_set_style_border_width(backBtn, 2, 0);
    lv_obj_set_style_border_color(backBtn, rgb(HEX_DIVIDER), 0);
    lv_obj_set_style_shadow_width(backBtn, 0, 0);
    lv_obj_add_event_cb(backBtn, &UiRenderer::onBackToMainClicked, LV_EVENT_CLICKED, this);

    lv_obj_t* backLbl = lv_label_create(backBtn);
    lv_label_set_text(backLbl, "CANCEL");
    lv_obj_set_style_text_font(backLbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(backLbl, rgb(HEX_SILVER), 0);
    lv_obj_center(backLbl);
}

void UiRenderer::render(const NavState& nav,
                        const BikeTelemetry& bike,
                        bool phoneConnected,
                        bool demoMode,
                        const ObdConfig* obdConfig) {
    if (!_initialized) {
        return;
    }

    if (nav.active != _lastAutoNavActive) {
        _lastAutoNavActive = nav.active;
        if (_page == ScreenPage::NAVIGATION || _page == ScreenPage::TELEMETRY) {
            setPage(nav.active ? ScreenPage::NAVIGATION : ScreenPage::TELEMETRY);
        }
    }

    if (_page == ScreenPage::NAVIGATION) {
        updateNavScreen(nav, phoneConnected, demoMode);
    } else if (_page == ScreenPage::TELEMETRY) {
        ObdDisplayConfig displayCfg;
        if (obdConfig) {
            displayCfg = obdConfig->display;
        }
        updateTelemetryScreen(bike, demoMode, displayCfg);
    }
}

void UiRenderer::updateNavScreen(const NavState& nav,
                                 bool phoneConnected,
                                 bool demoMode) {
    // Status pill (phone BLE connection takes priority over demo mode)
    const char* statusText = phoneConnected ? "BLE OK" : (demoMode ? "DEMO" : "NO PHONE");
    uint32_t statusHex = phoneConnected ? HEX_GREEN : (demoMode ? HEX_ESPMOTO_ORANGE : HEX_MUTED);
    lv_label_set_text(_navStatusLabel, statusText);
    lv_obj_set_style_text_color(_navStatusLabel, rgb(statusHex), 0);
    lv_obj_set_style_border_color(_navStatusPill, rgb(demoMode || phoneConnected ? statusHex : HEX_DIVIDER), 0);

    // Maneuver icon
    std::string normStreet = normalizeUtf8Text(nav.street.c_str());
    ManeuverType effectiveManeuver = nav.active ? nav.maneuver : ManeuverType::STRAIGHT;
    uint8_t effectiveExit = resolveRoundaboutExit(effectiveManeuver, nav.roundaboutExit, nav.street.c_str());
    if (_forceIconRefresh || effectiveManeuver != _lastManeuver ||
        effectiveExit != _lastRoundaboutExit || nav.active != _lastNavActive) {
        updateManeuverIcon(effectiveManeuver, effectiveExit, nav.active);
        _lastManeuver = effectiveManeuver;
        _lastRoundaboutExit = effectiveExit;
        _lastNavActive = nav.active;
        _forceIconRefresh = false;
    }

    // Distance, street, and ETA
    const char* distText = nav.active ? nav.distance.c_str() : "READY";
    if (std::strcmp(lv_label_get_text(_navDistLabel), distText) != 0) {
        lv_label_set_text(_navDistLabel, distText);
    }

    if (std::strcmp(lv_label_get_text(_navStreetLabel), normStreet.c_str()) != 0) {
        lv_label_set_text(_navStreetLabel, normStreet.c_str());
    }

    std::string normEta = normalizeUtf8Text(nav.eta.c_str());
    char etaBuf[48];
    snprintf(etaBuf, sizeof(etaBuf), "ETA %s", normEta.c_str());
    if (std::strcmp(lv_label_get_text(_navEtaLabel), etaBuf) != 0) {
        lv_label_set_text(_navEtaLabel, etaBuf);
    }
}

namespace {

void formatMetricSlot(MetricId id,
                      const BikeTelemetry& bike,
                      bool demoMode,
                      char* buf,
                      size_t bufSize,
                      uint32_t& outColorHex) {
    outColorHex = HEX_SILVER;
    if (id == MetricId::NONE) {
        buf[0] = '\0';
        return;
    }

    const bool hasData = demoMode || !bike.obdConnected || bike.isValid(id);
    if (!hasData) {
        std::snprintf(buf, bufSize, "%s --", defaultMetricLabel(id));
        outColorHex = HEX_MUTED;
        return;
    }

    switch (id) {
        case MetricId::SPEED:
            std::snprintf(buf, bufSize, "%u km/h", bike.speedKmh);
            outColorHex = HEX_WHITE;
            break;
        case MetricId::COOLANT_TEMP:
            std::snprintf(buf, bufSize, "%d\xC2\xB0" "C", bike.engineTempC);
            if (bike.engineTempC < 70) outColorHex = HEX_BLUE;
            else if (bike.engineTempC >= 120) outColorHex = HEX_RED;
            else if (bike.engineTempC >= 105) outColorHex = HEX_AMBER;
            else outColorHex = HEX_GREEN;
            break;
        case MetricId::VOLTAGE:
            std::snprintf(buf, bufSize, "%.1fV", static_cast<double>(bike.batteryVolts));
            if (bike.batteryVolts < 12.1f) outColorHex = HEX_RED;
            else if (bike.batteryVolts < 13.0f) outColorHex = HEX_AMBER;
            else outColorHex = HEX_GREEN;
            break;
        case MetricId::THROTTLE:
            std::snprintf(buf, bufSize, "TPS %u%%", bike.throttlePct);
            outColorHex = (bike.throttlePct >= 85) ? HEX_AMBER : HEX_SILVER;
            break;
        case MetricId::INTAKE_TEMP:
            std::snprintf(buf, bufSize, "IAT %d\xC2\xB0" "C", bike.intakeTempC);
            outColorHex = HEX_SILVER;
            break;
        case MetricId::GEAR:
            if (bike.gear <= 0) {
                std::snprintf(buf, bufSize, "GEAR N");
                outColorHex = HEX_GREEN;
            } else {
                std::snprintf(buf, bufSize, "GEAR %d", constrain(static_cast<int>(bike.gear), 1, 6));
                outColorHex = HEX_ESPMOTO_ORANGE;
            }
            break;
        case MetricId::OIL_PRESSURE:
            std::snprintf(buf, bufSize, "%.1f bar", static_cast<double>(bike.oilPressureBar));
            outColorHex = (bike.oilPressureBar < 0.8f) ? HEX_RED : HEX_GREEN;
            break;
        case MetricId::LEAN_ANGLE:
            std::snprintf(buf, bufSize, "%d\xC2\xB0", bike.leanAngleDeg);
            outColorHex = (abs(bike.leanAngleDeg) >= 40) ? HEX_AMBER : HEX_WHITE;
            break;
        case MetricId::CLUTCH_KICKSTAND:
            std::snprintf(buf, bufSize, "%s", bike.kickstandDown ? "STAND" : (bike.clutchEngaged ? "CLUTCH" : "RUN"));
            outColorHex = bike.kickstandDown ? HEX_AMBER : HEX_GREEN;
            break;
        case MetricId::ABS_TC:
            std::snprintf(buf, bufSize, "%s", bike.absActive ? "ABS ACT" : "ABS OK");
            outColorHex = bike.absActive ? HEX_AMBER : HEX_GREEN;
            break;
        default:
            std::snprintf(buf, bufSize, "%u", bike.rpm);
            break;
    }
}

} // namespace

void UiRenderer::updateTelemetryScreen(const BikeTelemetry& bike, bool demoMode, const ObdDisplayConfig& display) {
    const uint16_t maxRpm = (display.maxRpm >= 1000) ? display.maxRpm : MAX_RPM;
    const uint16_t redlineRpm = (display.redlineRpm >= 1000) ? display.redlineRpm : 5500;
    const uint16_t amberRpm = static_cast<uint16_t>((static_cast<uint32_t>(redlineRpm) * 85U) / 100U);

    if (maxRpm != _lastMaxRpm) {
        lv_arc_set_range(_telRpmArc, 0, maxRpm);
        _lastMaxRpm = maxRpm;
    }

    int32_t rpmClamped = constrain(static_cast<int32_t>(bike.rpm), 0, static_cast<int32_t>(maxRpm));
    uint32_t rpmArcHex = HEX_ESPMOTO_ORANGE;
    uint32_t rpmNumHex = HEX_WHITE;
    uint32_t rpmUnitHex = HEX_ESPMOTO_ORANGE;
    if (rpmClamped >= redlineRpm) {
        rpmArcHex = HEX_RED;
        rpmNumHex = HEX_RED;
        rpmUnitHex = HEX_RED;
    } else if (rpmClamped >= amberRpm) {
        rpmArcHex = HEX_AMBER;
        rpmNumHex = HEX_AMBER;
        rpmUnitHex = HEX_AMBER;
    }

    lv_arc_set_value(_telRpmArc, rpmClamped);
    lv_obj_set_style_arc_color(_telRpmArc, rgb(rpmArcHex), LV_PART_INDICATOR);

    // Bike name suppressed: show only connection / demo status
    const char* hdrText = demoMode ? "DEMO" : (bike.obdConnected ? "OBD OK" : "NO OBD");
    uint32_t hdrHex = demoMode ? HEX_ESPMOTO_ORANGE : (bike.obdConnected ? HEX_GREEN : HEX_MUTED);
    lv_label_set_text(_telHeaderLabel, hdrText);
    lv_obj_set_style_text_color(_telHeaderLabel, rgb(hdrHex), 0);

    char rpmBuf[16];
    snprintf(rpmBuf, sizeof(rpmBuf), "%u", bike.rpm);
    lv_label_set_text(_telRpmLabel, rpmBuf);
    lv_obj_set_style_text_color(_telRpmLabel, rgb(rpmNumHex), 0);
    lv_obj_set_style_text_color(_telUnitLabel, rgb(rpmUnitHex), 0);

    // Center badge (configurable: GEAR, SPEED, THROTTLE, or NONE)
    if (display.centerSlot == MetricId::NONE) {
        lv_obj_add_flag(_telGearBadge, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(_telGearBadge, LV_OBJ_FLAG_HIDDEN);
        char centerBuf[12] = "--";
        uint32_t centerTextHex = HEX_WHITE;
        uint32_t centerBorderHex = HEX_ESPMOTO_ORANGE;
        const bool centerValid = demoMode || !bike.obdConnected || bike.isValid(display.centerSlot);

        if (!centerValid) {
            std::snprintf(centerBuf, sizeof(centerBuf), "--");
            centerTextHex = HEX_MUTED;
            centerBorderHex = HEX_DIVIDER;
            lv_obj_set_style_text_font(_telGearLabel, &lv_font_montserrat_32, 0);
        } else if (display.centerSlot == MetricId::GEAR) {
            bool isNeutral = (bike.gear <= 0);
            centerBuf[0] = isNeutral ? 'N' : static_cast<char>('0' + constrain(static_cast<int>(bike.gear), 1, 6));
            centerBuf[1] = '\0';
            centerTextHex = isNeutral ? HEX_GREEN : HEX_WHITE;
            centerBorderHex = isNeutral ? HEX_GREEN : HEX_ESPMOTO_ORANGE;
            lv_obj_set_style_text_font(_telGearLabel, &lv_font_montserrat_48, 0);
        } else if (display.centerSlot == MetricId::SPEED) {
            std::snprintf(centerBuf, sizeof(centerBuf), "%u", bike.speedKmh);
            centerTextHex = HEX_WHITE;
            centerBorderHex = HEX_ESPMOTO_ORANGE;
            lv_obj_set_style_text_font(_telGearLabel, bike.speedKmh >= 100 ? &lv_font_montserrat_32 : &lv_font_montserrat_48, 0);
        } else if (display.centerSlot == MetricId::THROTTLE) {
            std::snprintf(centerBuf, sizeof(centerBuf), "%u%%", bike.throttlePct);
            centerTextHex = HEX_WHITE;
            centerBorderHex = HEX_ESPMOTO_ORANGE;
            lv_obj_set_style_text_font(_telGearLabel, &lv_font_montserrat_24, 0);
        } else {
            formatMetricSlot(display.centerSlot, bike, demoMode, centerBuf, sizeof(centerBuf), centerTextHex);
            lv_obj_set_style_text_font(_telGearLabel, &lv_font_montserrat_20, 0);
        }

        lv_label_set_text(_telGearLabel, centerBuf);
        lv_obj_set_style_text_color(_telGearLabel, rgb(centerTextHex), 0);
        lv_obj_set_style_border_color(_telGearBadge, rgb(centerBorderHex), 0);
    }

    // Bottom Left slot
    char leftBuf[20];
    uint32_t leftHex = HEX_GREEN;
    formatMetricSlot(display.bottomLeftSlot, bike, demoMode, leftBuf, sizeof(leftBuf), leftHex);
    lv_label_set_text(_telTempLabel, leftBuf);
    lv_obj_set_style_text_color(_telTempLabel, rgb(leftHex), 0);

    // Bottom Right slot
    char rightBuf[20];
    uint32_t rightHex = HEX_GREEN;
    formatMetricSlot(display.bottomRightSlot, bike, demoMode, rightBuf, sizeof(rightBuf), rightHex);
    lv_label_set_text(_telVoltLabel, rightBuf);
    lv_obj_set_style_text_color(_telVoltLabel, rgb(rightHex), 0);

    // Optional Bottom Center auxiliary slot
    if (display.bottomCenterSlot == MetricId::NONE) {
        lv_obj_add_flag(_telAuxLabel, LV_OBJ_FLAG_HIDDEN);
    } else {
        char auxBuf[24];
        uint32_t auxHex = HEX_SILVER;
        formatMetricSlot(display.bottomCenterSlot, bike, demoMode, auxBuf, sizeof(auxBuf), auxHex);
        lv_label_set_text(_telAuxLabel, auxBuf);
        lv_obj_set_style_text_color(_telAuxLabel, rgb(auxHex), 0);
        lv_obj_remove_flag(_telAuxLabel, LV_OBJ_FLAG_HIDDEN);
    }
}

void UiRenderer::updateManeuverIcon(ManeuverType type, uint8_t roundaboutExit, bool active) {
    uint32_t colorHex = active ? HEX_WHITE : HEX_MUTED;
    lv_color_t strokeColor = rgb(colorHex);

    lv_obj_set_style_line_color(_navArrowShaft, strokeColor, 0);
    lv_obj_set_style_line_color(_navArrowHead, strokeColor, 0);
    lv_obj_set_style_line_color(_navArrowAux, strokeColor, 0);
    lv_obj_set_style_line_width(_navArrowShaft, 15, 0);
    lv_obj_set_style_line_width(_navArrowAux, 15, 0);
    lv_obj_set_style_line_width(_navArrowHead, 15, 0);
    lv_obj_set_style_arc_color(_navArrowRing, strokeColor, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_navArrowRing, 14, LV_PART_MAIN);
    lv_arc_set_bg_angles(_navArrowRing, 0, 360);

    // Hide optional primitives by default; enable per maneuver
    lv_obj_add_flag(_navRbtBaseRing, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 3; ++i) {
        lv_obj_add_flag(_navRbtStub[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(_navRbtExitLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_navArrowRing, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(_navArrowAux, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_navArrowShaft, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(_navArrowHead, LV_OBJ_FLAG_HIDDEN);

    switch (type) {
        case ManeuverType::TURN_RIGHT: {
            const lv_point_precise_t shaft[] = {{36, 110}, {36, 54}, {102, 54}};
            const lv_point_precise_t head[]  = {{74, 28}, {102, 54}, {74, 80}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::TURN_LEFT: {
            const lv_point_precise_t shaft[] = {{92, 110}, {92, 54}, {26, 54}};
            const lv_point_precise_t head[]  = {{54, 28}, {26, 54}, {54, 80}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::SLIGHT_RIGHT: {
            const lv_point_precise_t shaft[] = {{44, 110}, {44, 68}, {88, 24}};
            const lv_point_precise_t head[]  = {{52, 24}, {88, 24}, {88, 60}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::SLIGHT_LEFT: {
            const lv_point_precise_t shaft[] = {{84, 110}, {84, 68}, {40, 24}};
            const lv_point_precise_t head[]  = {{76, 24}, {40, 24}, {40, 60}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::SHARP_RIGHT: {
            const lv_point_precise_t shaft[] = {{36, 108}, {36, 32}, {92, 88}};
            const lv_point_precise_t head[]  = {{56, 88}, {92, 88}, {92, 52}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::SHARP_LEFT: {
            const lv_point_precise_t shaft[] = {{92, 108}, {92, 32}, {36, 88}};
            const lv_point_precise_t head[]  = {{72, 88}, {36, 88}, {36, 52}};
            setPoints(_shaftPts, shaft, 3);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::UTURN_LEFT: {
            const lv_point_precise_t shaft[] = {{88, 108}, {88, 44}, {76, 26}, {52, 26}, {40, 44}, {40, 90}};
            const lv_point_precise_t head[]  = {{20, 68}, {40, 90}, {60, 68}};
            setPoints(_shaftPts, shaft, 6);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 6);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::UTURN_RIGHT: {
            const lv_point_precise_t shaft[] = {{40, 108}, {40, 44}, {52, 26}, {76, 26}, {88, 44}, {88, 90}};
            const lv_point_precise_t head[]  = {{68, 68}, {88, 90}, {108, 68}};
            setPoints(_shaftPts, shaft, 6);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 6);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
        case ManeuverType::ROUNDABOUT:
        case ManeuverType::ROUNDABOUT_EXIT_1:
        case ManeuverType::ROUNDABOUT_EXIT_2:
        case ManeuverType::ROUNDABOUT_EXIT_3:
        case ManeuverType::ROUNDABOUT_EXIT_4:
        case ManeuverType::ROUNDABOUT_EXIT_5: {
            // European right-hand drive counter-clockwise roundabout around center (64, 62)
            struct RoundaboutExitSpec {
                uint16_t arcStartDeg;
                uint16_t arcEndDeg;
                bool showStub[3]; // [0]=3 o'clock (1st), [1]=12 o'clock (2nd), [2]=9 o'clock (3rd)
                lv_point_precise_t exitShaft[2];
                lv_point_precise_t exitHead[3];
            };

            static constexpr lv_point_precise_t STUB_GEOM[3][2] = {
                {{95, 62}, {109, 62}}, // 1st exit stub (3 o'clock)
                {{64, 31}, {64, 17}},  // 2nd exit stub (12 o'clock)
                {{33, 62}, {19, 62}}   // 3rd exit stub (9 o'clock)
            };

            static constexpr RoundaboutExitSpec EXIT_SPECS[4] = {
                // Exit 1: Right (3 o'clock)
                {0, 90, {false, true, true}, {{90, 62}, {118, 62}}, {{102, 46}, {118, 62}, {102, 78}}},
                // Exit 2: Straight (12 o'clock)
                {270, 90, {true, false, true}, {{64, 36}, {64, 8}}, {{48, 24}, {64, 8}, {80, 24}}},
                // Exit 3: Left (9 o'clock)
                {180, 90, {true, true, false}, {{38, 62}, {10, 62}}, {{26, 46}, {10, 62}, {26, 78}}},
                // Exit 4+: Bottom-Left / U-turn (~7:30 o'clock)
                {135, 90, {true, true, true}, {{46, 80}, {22, 104}}, {{22, 82}, {22, 104}, {44, 104}}}
            };

            const uint8_t exitNum = (roundaboutExit >= 1) ? roundaboutExit : 2;
            const std::size_t specIdx = std::min<std::size_t>(exitNum - 1, 3);
            const auto& spec = EXIT_SPECS[specIdx];

            lv_obj_remove_flag(_navRbtBaseRing, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(_navArrowRing, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(_navArrowAux, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(_navRbtExitLabel, LV_OBJ_FLAG_HIDDEN);

            lv_obj_align(_navArrowRing, LV_ALIGN_CENTER, 0, -2);
            lv_obj_set_style_arc_width(_navArrowRing, 12, LV_PART_MAIN);
            lv_arc_set_bg_angles(_navArrowRing, spec.arcStartDeg, spec.arcEndDeg);

            lv_obj_set_style_line_width(_navArrowShaft, 12, 0);
            lv_obj_set_style_line_width(_navArrowAux, 12, 0);
            lv_obj_set_style_line_width(_navArrowHead, 12, 0);

            for (int i = 0; i < 3; ++i) {
                if (spec.showStub[i]) {
                    setPoints(_rbtStubPts[i], STUB_GEOM[i], 2);
                    lv_line_set_points(_navRbtStub[i], _rbtStubPts[i], 2);
                    lv_obj_remove_flag(_navRbtStub[i], LV_OBJ_FLAG_HIDDEN);
                }
            }

            // Entry stem from bottom (6 o'clock) into the roundabout ring
            const lv_point_precise_t shaft[] = {{64, 118}, {64, 90}};
            setPoints(_shaftPts, shaft, 2);
            setPoints(_auxPts, spec.exitShaft, 2);
            setPoints(_headPts, spec.exitHead, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 2);
            lv_line_set_points(_navArrowAux, _auxPts, 2);
            lv_line_set_points(_navArrowHead, _headPts, 3);

            char exitBuf[8];
            snprintf(exitBuf, sizeof(exitBuf), "%u", static_cast<unsigned>(exitNum));
            lv_label_set_text(_navRbtExitLabel, exitBuf);
            lv_obj_set_style_text_color(_navRbtExitLabel, rgb(active ? HEX_ESPMOTO_ORANGE : HEX_MUTED), 0);
            break;
        }
        case ManeuverType::MERGE: {
            lv_obj_remove_flag(_navArrowAux, LV_OBJ_FLAG_HIDDEN);
            const lv_point_precise_t shaft[] = {{64, 110}, {64, 22}};
            const lv_point_precise_t head[]  = {{36, 50}, {64, 22}, {92, 50}};
            const lv_point_precise_t aux[]   = {{28, 104}, {64, 64}};
            setPoints(_shaftPts, shaft, 2);
            setPoints(_headPts, head, 3);
            setPoints(_auxPts, aux, 2);
            lv_line_set_points(_navArrowShaft, _shaftPts, 2);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            lv_line_set_points(_navArrowAux, _auxPts, 2);
            break;
        }
        case ManeuverType::DESTINATION: {
            lv_obj_remove_flag(_navArrowRing, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(_navArrowRing, LV_ALIGN_CENTER, 0, -14);
            lv_obj_add_flag(_navArrowHead, LV_OBJ_FLAG_HIDDEN);

            const lv_point_precise_t shaft[] = {{40, 62}, {64, 108}, {88, 62}};
            setPoints(_shaftPts, shaft, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 3);
            break;
        }
        case ManeuverType::STRAIGHT:
        case ManeuverType::NONE:
        default: {
            const lv_point_precise_t shaft[] = {{64, 110}, {64, 22}};
            const lv_point_precise_t head[]  = {{34, 52}, {64, 22}, {94, 52}};
            setPoints(_shaftPts, shaft, 2);
            setPoints(_headPts, head, 3);
            lv_line_set_points(_navArrowShaft, _shaftPts, 2);
            lv_line_set_points(_navArrowHead, _headPts, 3);
            break;
        }
    }

    lv_obj_invalidate(_navIconBox);
}
