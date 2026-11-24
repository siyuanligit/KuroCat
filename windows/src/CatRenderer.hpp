#pragma once
#define NOMINMAX
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>
#include "CatPhysics.hpp"
#include "TabbyCoat.hpp"
#include "../../shared/CoatPatterns.hpp"
#include "../../shared/SkinSpec.hpp"

enum class CatHitZone {
    None,
    Head,
    Body
};

// The six built-in coats expressed as SkinSpecs, using the exact 8-bit colours the GDI+ renderer always
// used, so built-ins render exactly as before. (skins/*.kurocat are the portable templates of these coats.)
inline const SkinSpec& builtInSkin(CatCoatTheme coat) {
    static const std::vector<SkinSpec> skins = [] {
        auto c8 = [](int r, int g, int b, int a = 255) { return SkinColor::rgba(r / 255.0, g / 255.0, b / 255.0, a / 255.0); };
        const SkinColor black = c8(22, 22, 24);
        const SkinColor milk = c8(0xF2, 0xEE, 0xE4);
        auto tailRings = [&](int r, int g, int b) {
            std::vector<SkinMark> rings;
            for (float x : {50.0f, 78.0f}) {
                SkinMark m;
                m.space = SkinSpace::Tail; m.stroke = true; m.width = 3.5f; m.color = c8(r, g, b, 128);
                m.points = {{x, 8.0f}, {x, -8.0f}};
                rings.push_back(m);
            }
            return rings;
        };
        std::vector<SkinSpec> v(7);
        SkinSpec& k = v[0];
        k.id = "builtin:0"; k.name = "经典小黑猫"; k.builtIn = true;
        k.palette.body = k.palette.legs = k.palette.paws = k.palette.earOuter = k.palette.tail = black;
        k.palette.whiskers = c8(224, 230, 235, 174); k.palette.crease = c8(209, 235, 227, 194);

        SkinSpec& s = v[1];
        s.id = "builtin:1"; s.name = "挖煤暹罗猫"; s.builtIn = true; s.shape.build = SkinBuild::Sleek;
        const SkinColor point = c8(53, 38, 31);
        s.palette.body = c8(246, 239, 228);
        s.palette.bodyShade = c8(234, 219, 200);
        s.palette.points = s.palette.paws = s.palette.earOuter = s.palette.tail = s.palette.legs = point;
        s.palette.pointsMid = c8(106, 80, 65);
        s.palette.earInner = c8(93, 70, 58);
        s.palette.outline = c8(173, 150, 128, 107);
        s.palette.crease = c8(141, 120, 103, 138);
        s.palette.whiskers = c8(122, 99, 83, 158);
        s.palette.nose = c8(36, 23, 20, 242);
        s.palette.mouth = c8(199, 168, 143, 140);

        SkinSpec& h = v[2];
        h.id = "builtin:2"; h.name = "美短（银虎斑加白）"; h.builtIn = true;
        h.palette.body = h.palette.legs = h.palette.earOuter = h.palette.tail = c8(0xC2, 0xC5, 0xC1);
        h.palette.belly = h.palette.paws = milk;
        h.palette.stripes = c8(0x55, 0x59, 0x5A);
        h.palette.whiskers = c8(0x8D, 0x81, 0x70, 174); h.palette.crease = c8(0x8B, 0x81, 0x71, 153);
        h.headSurface = SkinHeadSurface::Plane; h.shape.pawStagger = 0;
        h.marks = skinMarksFromCoat(shorthairMarks());
        for (auto& m : tailRings(0x55, 0x59, 0x5A)) h.marks.push_back(m);

        SkinSpec& o = v[3];
        o.id = "builtin:3"; o.name = "橘白猫"; o.builtIn = true;
        o.palette.body = milk;
        o.palette.legs = o.palette.earOuter = o.palette.tail = c8(0xD8, 0xAA, 0x73);
        o.palette.belly = o.palette.paws = milk;
        o.palette.stripes = c8(0xA5, 0x75, 0x43);
        o.palette.whiskers = c8(0x8D, 0x81, 0x70, 174); o.palette.crease = c8(0x8B, 0x81, 0x71, 153);
        o.headSurface = SkinHeadSurface::Dome; o.shape.pawStagger = 0;
        o.marks = skinMarksFromCoat(orangeWhiteMarks());
        for (auto& m : tailRings(0xA5, 0x75, 0x43)) o.marks.push_back(m);

        SkinSpec& d = v[4];
        d.id = "builtin:4"; d.name = "呆猫"; d.builtIn = true;
        d.palette.body = c8(0x4E, 0x57, 0x92);
        d.palette.legs = c8(0xE3, 0xC9, 0x9F);
        d.palette.paws = c8(0xFF, 0xF1, 0xCD);
        d.palette.earOuter = c8(0x55, 0x5F, 0x9B);
        d.palette.tail = c8(0x28, 0x31, 0x57);
        d.palette.whiskers = c8(0x8D, 0x81, 0x70, 174); d.palette.crease = c8(0x7C, 0x86, 0xBC, 155);

        SkinSpec& t = v[5];
        t.id = "builtin:5"; t.name = "狸花猫"; t.builtIn = true;
        t.palette.body = c8(202, 180, 149);
        t.palette.legs = c8(174, 152, 123);
        t.palette.paws = c8(239, 231, 213);
        t.palette.earOuter = c8(103, 86, 66);
        t.palette.tail = c8(163, 143, 117);
        t.palette.whiskers = c8(104, 85, 65, 174); t.palette.crease = c8(104, 85, 65, 255);

        // Solid British Blue (dark gray, no markings); eyes come from the eye-colour menu (paired: warm amber).
        SkinSpec& g = v[6];
        g.id = "builtin:6"; g.name = "英短蓝猫（灰毛橘黄瞳）"; g.builtIn = true;
        g.palette.body = g.palette.legs = g.palette.paws = g.palette.earOuter = g.palette.tail = c8(0x3A, 0x3F, 0x47);
        g.palette.earInner = c8(0xB9, 0xA4, 0xA4); g.palette.nose = c8(0x9C, 0x7F, 0x82);
        g.palette.whiskers = c8(0xD8, 0xDC, 0xE0, 174); g.palette.crease = c8(0x18, 0x1B, 0x20, 192);
        g.headSurface = SkinHeadSurface::Dome; g.shape.pawStagger = 0;
        for (SkinSpec& b : v) b.idleLegsTinted = false;
        return v;
    }();
    int i = std::clamp((int)coat, 0, static_cast<int>(CatCoatTheme::BritishShorthair));
    return skins[(size_t)i];
}

class CatRenderer {
public:
    static constexpr float RadToDeg = 57.2957795f;

    static CatHitZone hitTest(Pt p, const CatPhysics& physics, const SkinSpec& skin) {
        const SkinShape& shape = skin.shape;
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        Pt headCenter = { cx + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f,
                          126.0f + buildOf(skin).headLift + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f };

        // Petting zones follow the reshaped head (size + cheeks) and body (width), plus Dazed Cat hood proportions.
        bool dazed = isDazed(skin);
        float hdx = (p.x - headCenter.x) / ((dazed ? 47.0f : 44.0f) * shape.headScale * shape.cheekFullness);
        float hdy = (p.y - headCenter.y - (dazed ? 3.0f : 0.0f)) / ((dazed ? 40.0f : 38.0f) * shape.headScale);
        if (hdx * hdx + hdy * hdy <= 1.0f) return CatHitZone::Head;

        float bdx = (p.x - cx) / (58.0f * shape.bodyWidth);
        float bdy = (p.y - 196.0f) / 56.0f;
        if (bdx * bdx + bdy * bdy <= 1.0f) return CatHitZone::Body;

        if (p.x >= 58.0f && p.x <= 294.0f && p.y >= 232.0f && p.y <= 296.0f) return CatHitZone::Body;
        return CatHitZone::None;
    }

    /// Edit-mode hit test in unflipped 360x320 canvas coordinates:
    /// 0..3 = corner resize handles, 10 = flip button, 11 = done button, 12 = inside edit box, -1 = outside.
    static int hitTestEditOverlay(Pt p) {
        if (p.x >= 92.0f && p.x <= 178.0f && p.y >= 10.0f && p.y <= 38.0f) return 10;
        if (p.x >= 182.0f && p.x <= 268.0f && p.y >= 10.0f && p.y <= 38.0f) return 11;
        constexpr Pt corners[4] = { {10.0f, 10.0f}, {350.0f, 10.0f}, {10.0f, 310.0f}, {350.0f, 310.0f} };
        for (int i = 0; i < 4; ++i) {
            if (std::hypot(p.x - corners[i].x, p.y - corners[i].y) <= 18.0f) return i;
        }
        if (p.x >= 6.0f && p.x <= 354.0f && p.y >= 6.0f && p.y <= 314.0f) return 12;
        return -1;
    }

    static void draw(Gdiplus::Graphics& g, const CatPhysics& physics, const CatSettings& settings, const SkinSpec& skin) {
        using namespace Gdiplus;
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

        GraphicsState flipState = g.Save();
        if (settings.flipHorizontal) {
            g.TranslateTransform(CatPhysics::CanvasWidth, 0.0f);
            g.ScaleTransform(-1.0f, 1.0f);
        }

        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        float seatX = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float scruff = std::clamp(physics.scruffDrag.value, 0.0f, 1.0f);
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float dizzy = std::clamp(physics.dizzyIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float dizzyFallRaw = std::clamp((dizzy - 0.50f) / 0.50f, 0.0f, 1.0f);
        float dizzyFall = dizzyFallRaw * dizzyFallRaw * (3.0f - 2.0f * dizzyFallRaw);
        float preWobble = std::clamp((dizzy - 0.12f) / 0.38f, 0.0f, 1.0f) * (1.0f - dizzyFall);

        float seatY = CatPhysics::SeatAnchor.y - scruff * 22.0f + lieS * 10.0f;
        float fallAngle = -dizzyFall * 1.46f + std::sin(physics.time * 9.5f) * 0.16f * preWobble;
        float fallShiftX = dizzyFall * 56.0f;
        float fallShiftY = dizzyFall * 20.0f;

        Pt headCenter = {
            seatX + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f - lieS * 4.0f,
            126.0f + buildOf(skin).headLift + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f - scruff * 22.0f + lieS * 60.0f
        };

        // 1. Mini White Mouse OR Graphics Tablet (when drawing tablet/stylus is connected!)
        if (physics.isTabletActive) {
            drawTablet(g, physics, settings.keyboardTheme, settings.eyeTheme);
            if (!physics.leftPawOnMouse || scruff > 0.25f || dizzy > 0.35f || lie > 0.45f) {
                drawParkedStylus(g, settings.keyboardTheme, settings.eyeTheme);
            }
        } else {
            drawMouse(g, physics, settings.keyboardTheme);
        }

        // 2. Mini 65% Mechanical Keyboard (facing cat) + Backlit Glow
        drawKeyboard(g, physics, settings.keyboardTheme, settings.eyeTheme, settings.showKeyLegends, settings.keyboardMarquee);

        GraphicsState fallState = g.Save();
        if (dizzy > 0.01f) {
            g.TranslateTransform(seatX + fallShiftX, seatY + fallShiftY);
            g.RotateTransform(fallAngle * RadToDeg);
            g.TranslateTransform(-seatX, -seatY);
        }

        if (isDazed(skin)) drawDazedSword(g, seatX, seatY, physics);
        bool drawBlackRim = isClassicBlack(skin);
        bool tailInFront = (physics.tailTyping.value >= 0.5f) || (lieS >= 0.45f);
        if (drawBlackRim) {
            if (!tailInFront) drawTail(g, seatX, seatY, physics, skin, true);
            drawEarsLayer(g, headCenter, physics, skin, true);
            drawUnifiedSilhouette(g, seatX, seatY, headCenter, breath, physics, skin, true);
        }
        // 3. Tail (behind body unless 4-key tail typing or curled up asleep)
        if (!tailInFront) drawTail(g, seatX, seatY, physics, skin);

        // 4. 3D-Projected Ears (behind crown silhouette)
        drawEarsLayer(g, headCenter, physics, skin);

        // 5. Unified C1-Continuous Silhouette (+ regions, colour-point mask, marks)
        drawUnifiedSilhouette(g, seatX, seatY, headCenter, breath, physics, skin);

        // 6. Hind Foot (3rd simultaneous key easter egg!)
        if (physics.hindFootActive.value > 0.03f) {
            drawHindFoot(g, seatX, seatY, physics, skin);
        }

        // 7. 2.5D Eyes & Whiskers (+ optional nose/mouth, yawn, sweat drop)
        drawHeadFeatures(g, headCenter, physics, settings.eyeTheme, skin);

        // 7b. Skin stickers anchored to head / neck / tail tip
        if (!skin.decorations.empty()) drawDecorations(g, seatX, seatY, headCenter, physics, skin);

        // 8. Front Paws (Idle Sitting & Dynamic Reaching)
        drawFrontPaws(g, seatX, seatY, breath, physics, skin);

        // 8b. Held Stylus gripped cleanly in the cat's paw when drawing on the graphics tablet!
        if (physics.isTabletActive && physics.leftPawOnMouse && scruff <= 0.25f && dizzy <= 0.35f && lie <= 0.45f) {
            drawHeldStylus(g, physics, settings.keyboardTheme, settings.eyeTheme);
        }

        if (tailInFront) {
            if (drawBlackRim) drawTail(g, seatX, seatY, physics, skin, true);
            drawTail(g, seatX, seatY, physics, skin);
        }

        g.Restore(fallState);

        // Upright orbiting golden dizzy stars floating right above the head (appear FIRST before falling!)
        if (dizzy > 0.10f) {
            float relX = headCenter.x - seatX;
            float relY = headCenter.y - seatY;
            float worldHeadX = seatX + fallShiftX + relX * std::cos(fallAngle) - relY * std::sin(fallAngle);
            float worldHeadY = seatY + fallShiftY + relX * std::sin(fallAngle) + relY * std::cos(fallAngle);
            float dAlpha = std::min(1.0f, (dizzy - 0.10f) / 0.22f);
            float starLift = 42.0f + dizzyFall * 10.0f;
            GraphicsState ss = g.Save();
            g.TranslateTransform(worldHeadX, worldHeadY - starLift);
            SolidBrush sb(Color((BYTE)(245 * dAlpha), 255, 220, 56));
            for (int i = 0; i < 3; ++i) {
                float ang = physics.dizzyPhase * 0.85f + (float)i * (6.283185f / 3.0f);
                float sx = std::cos(ang) * 26.0f;
                float sy = std::sin(ang) * 6.5f;
                float r = 4.4f * (0.75f + 0.25f * std::sin(ang));
                PointF pts[8];
                float rot = physics.dizzyPhase + (float)i;
                for (int k = 0; k < 8; ++k) {
                    float rad = (k % 2 == 0) ? r : r * 0.42f;
                    float a = rot + (float)k * 0.785398f;
                    pts[k] = PointF(sx + std::cos(a) * rad, sy + std::sin(a) * rad);
                }
                g.FillPolygon(&sb, pts, 8);
            }
            g.Restore(ss);
        }

        // 9. Floating Keycap Badges (keyboard + mouse clicks)
        if (!physics.floatingKeycaps.empty()) {
            drawFloatingKeycaps(g, physics);
        }

        // 10. Subtle Petting Sparkles & Sleep Z
        drawSparkles(g, physics, settings.eyeTheme);

        g.Restore(flipState);

        if (settings.isEditMode) {
            drawEditOverlay(g, settings.isEnglish, settings.scale);
        }
    }

    static void drawEditOverlay(Gdiplus::Graphics& g, bool isEnglish, float scale) {
        using namespace Gdiplus;
        GraphicsPath boxPath;
        addRoundedRect(boxPath, 10.0f, 10.0f, 340.0f, 300.0f, 12.0f);
        SolidBrush tint(Color(18, 41, 143, 250));
        g.FillPath(&tint, &boxPath);

        Pen dashPen(Color(235, 51, 158, 255), 1.8f);
        REAL dashes[2] = { 4.0f, 2.5f };
        dashPen.SetDashPattern(dashes, 2);
        g.DrawPath(&dashPen, &boxPath);

        constexpr Pt corners[4] = { {10.0f, 10.0f}, {350.0f, 10.0f}, {10.0f, 310.0f}, {350.0f, 310.0f} };
        SolidBrush whiteBrush(Color(255, 255, 255, 255));
        Pen handlePen(Color(255, 36, 133, 245), 2.0f);
        for (const auto& c : corners) {
            g.FillEllipse(&whiteBrush, c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
            g.DrawEllipse(&handlePen, c.x - 6.5f, c.y - 6.5f, 13.0f, 13.0f);
        }

        FontFamily ff(L"Segoe UI");
        Font btnFont(&ff, 10.0f, FontStyleBold, UnitPixel);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        auto drawBtn = [&](float x, float y, float w, float h, const wchar_t* txt, bool primary) {
            GraphicsPath bp;
            addRoundedRect(bp, x, y, w, h, 11.0f);
            SolidBrush fill(primary ? Color(242, 41, 138, 245) : Color(224, 36, 38, 48));
            Pen rim(Color(72, 255, 255, 255), 0.9f);
            g.FillPath(&fill, &bp);
            g.DrawPath(&rim, &bp);
            g.DrawString(txt, -1, &btnFont, RectF(x, y, w, h), &sf, &whiteBrush);
        };

        drawBtn(94.0f, 12.0f, 82.0f, 24.0f, isEnglish ? L"\u21C4 Flip" : L"\u21C4 \u7FFB\u8F6C", false);
        drawBtn(184.0f, 12.0f, 82.0f, 24.0f, isEnglish ? L"\u2713 Done" : L"\u2713 \u5B8C\u6210", true);

        wchar_t buf[16] = {};
        swprintf_s(buf, L"%.0f%%", scale * 100.0f);
        GraphicsPath sp;
        addRoundedRect(sp, 158.0f, 290.0f, 44.0f, 16.0f, 6.0f);
        SolidBrush spBg(Color(210, 30, 33, 41));
        Font spFont(&ff, 9.5f, FontStyleBold, UnitPixel);
        g.FillPath(&spBg, &sp);
        g.DrawString(buf, -1, &spFont, RectF(158.0f, 290.0f, 44.0f, 16.0f), &sf, &whiteBrush);
    }

private:
    static Gdiplus::Color catBlack(BYTE a = 255)      { return Gdiplus::Color(a, 22, 22, 24); }
    static constexpr float BlackOutlineW = 1.6f;
    static Gdiplus::Color blackOutline(BYTE a = 240)  { return Gdiplus::Color(a, 255, 255, 255); }
    static Gdiplus::Color rgb(unsigned int hex, BYTE a = 255) { return Gdiplus::Color(a, (hex >> 16) & 255, (hex >> 8) & 255, hex & 255); }

    static bool isBuiltInCoat(const SkinSpec& skin, const char* builtinId) { return skin.builtIn && skin.id == builtinId; }
    static bool isClassicBlack(const SkinSpec& skin) { return isBuiltInCoat(skin, "builtin:0"); }
    static bool isSiamese(const SkinSpec& skin)      { return isBuiltInCoat(skin, "builtin:1"); }
    static bool isShorthair(const SkinSpec& skin)    { return isBuiltInCoat(skin, "builtin:2"); }
    static bool isOrangeWhite(const SkinSpec& skin)  { return isBuiltInCoat(skin, "builtin:3"); }
    static bool isDazed(const SkinSpec& skin)        { return isBuiltInCoat(skin, "builtin:4"); }
    static bool isTabby(const SkinSpec& skin)        { return isBuiltInCoat(skin, "builtin:5"); }
    static bool isPatternedBuiltIn(const SkinSpec& skin) { return isShorthair(skin) || isOrangeWhite(skin); }

    // Base skeleton per SkinBuild; SkinShape multipliers are applied on top (all exact no-ops at 1).
    struct Build {
        float headLift;
        float cheekHalfW, crownCtrlW, crownBaseY, cheekY, cheekCtrlTopY, neckPinchX, neckCtrlY;
        float hipHalfW, waistCtrlW, waistCtrlY, botOuterW, botInnerW, botHalfW;
        float earFold, earLen, earInX, earInY, earOutX, earOutY, earTipX, earTipY, earTipSpread;
        float tailWidth;
        float hindLeftX, hindW, hindRightX, hindLeftY, hindLeftH, hindStaggerY, hindStaggerH;
        float idleTopDY, idleTopX, idleBotX, shoulderX, limbStartW, limbEndW;
        bool sleekEyes;
    };
    static const Build& buildOf(const SkinSpec& skin) {
        static const Build standard = { 0.0f,
            42.5f, 26.0f, -30.0f, 5.0f, -14.0f, 2.5f, 24.0f,
            49.0f, 51.5f, -48.0f, 47.0f, 36.0f, 24.0f,
            0.06f, 32.0f, 9.0f, -16.0f, 35.0f, -5.0f, 24.5f, -20.0f, 3.8f,
            11.0f,
            -35.0f, 17.0f, 18.0f, 3.0f, 11.0f, 4.5f, 12.5f,
            30.0f, 18.0f, 16.0f, 20.0f, 19.0f, 13.5f,
            false };
        static const Build sleek = { -3.0f,
            42.5f, 25.0f, -29.5f, 6.5f, -13.0f, 6.5f, 27.5f,
            44.5f, 41.0f, -50.0f, 42.0f, 32.0f, 22.0f,
            0.14f, 29.5f, 10.0f, -16.5f, 35.5f, -4.5f, 25.0f, -18.5f, 4.2f,
            9.5f,
            -31.0f, 15.5f, 15.5f, 3.5f, 12.0f, 4.5f, 12.5f,
            31.0f, 16.5f, 15.0f, 18.5f, 17.0f, 12.5f,
            true };
        return skin.shape.build == SkinBuild::Sleek ? sleek : standard;
    }

    // Scale v around c by k; returns v untouched when k == 1 so default shapes stay bit-identical.
    static float around(float v, float c, float k) { return k == 1.0f ? v : c + (v - c) * k; }

    static BYTE byteOf(double v) { return (BYTE)std::lround(std::clamp(v, 0.0, 1.0) * 255.0); }
    static Gdiplus::Color col(const SkinColor& c) { return Gdiplus::Color(byteOf(c.a), byteOf(c.r), byteOf(c.g), byteOf(c.b)); }
    static Gdiplus::Color withAlpha(const SkinColor& c, BYTE a) { return Gdiplus::Color(a, byteOf(c.r), byteOf(c.g), byteOf(c.b)); }
    // Legacy crease strokes ask for an absolute alpha `a`: opaque-ish creases (like the black cat's mint,
    // alpha >= 0.75) use it as-is, translucent ones scale it by their own alpha (the patterned coats' 0.60)
    // — exactly what the pre-SkinSpec creaseColor(coat, a) produced.
    static Gdiplus::Color creaseWith(const SkinColor& c, BYTE a) { return withAlpha(c, c.a >= 0.75 ? a : (BYTE)(a * (float)c.a)); }

    // Head-local point -> canvas, on the skin's head surface (dome / plane) or flat skull space.
    static Gdiplus::PointF headPoint(SkinPoint p, bool skull, const SkinSpec& skin, Pt head, const CatPhysics& physics) {
        float depth = skin.headSurface == SkinHeadSurface::Dome ? 29.6f*std::sqrt(std::max(0.0f,1-p.x*p.x/(46*46)-p.y*p.y/(40*40))) : 27.5f;
        Pt q = skull ? Pt{p.x,p.y} : projectHead3D(p.x,p.y,depth,physics.headYaw.value*0.50f,physics.headPitch.value*0.36f);
        float hs = skin.shape.headScale;
        q.x *= hs; q.y *= hs;
        float c = std::cos(physics.headTilt.value), s = std::sin(physics.headTilt.value);
        return {head.x + q.x*c - q.y*s, head.y + q.x*s + q.y*c};
    }
    static Gdiplus::PointF bodyPoint(SkinPoint p, const SkinSpec& skin, float seatX, float seatY, const CatPhysics& physics) {
        float bw = skin.shape.bodyWidth;
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        float c = std::cos(physics.bodyTilt.value), s = std::sin(physics.bodyTilt.value);
        return {seatX+p.x*bw*sx*c-p.y*sy*s, seatY+p.x*bw*sx*s+p.y*sy*c};
    }
    static Gdiplus::PointF coatMarkPoint(CoatPoint p, bool headSurface, bool skull, CatCoatTheme coat, float seatX, float seatY, Pt head, const CatPhysics& physics, bool whiteSurface = false) {
        if (headSurface) {
            float radius = std::max(0.0f, 1.0f - p.x*p.x/(46.0f*46.0f) - p.y*p.y/(40.0f*40.0f));
            float surface = whiteSurface ? radius*radius*(3.0f - 2.0f*radius) : std::sqrt(radius);
            float depth = coat == CatCoatTheme::OrangeWhite ? 29.6f*surface : 27.5f;
            Pt q = skull ? Pt{p.x,p.y} : projectHead3D(p.x,p.y,depth,physics.headYaw.value*0.50f,physics.headPitch.value*0.36f);
            float c = std::cos(physics.headTilt.value), s = std::sin(physics.headTilt.value);
            return {head.x + q.x*c - q.y*s, head.y + q.x*s + q.y*c};
        }
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        float c = std::cos(physics.bodyTilt.value), s = std::sin(physics.bodyTilt.value);
        return {seatX+p.x*sx*c-p.y*sy*s, seatY+p.x*sx*s+p.y*sy*c};
    }
    static void drawCoatMarks(Gdiplus::Graphics& g, float seatX, float seatY, Pt head, const CatPhysics& physics, CatCoatTheme coat) {
        using namespace Gdiplus;
        const auto& marks = coat == CatCoatTheme::Shorthair ? shorthairMarks() : orangeWhiteMarks();
        const CoatMark* bib = nullptr;
        for (const auto& mark : marks) if (mark.head && !mark.skull && mark.rgb == 0xF2EEE4) {
            for (const auto& p : mark.points) if (std::abs(p.x) > 46) { bib = &mark; break; }
            if (bib) break;
        }
        float overlap = std::min(1.0f, std::abs(physics.headYaw.value) * std::max(0.0f, physics.headPitch.value));
        float coverage = overlap * overlap * (3.0f - 2.0f * overlap);
        for (const auto& mark : marks) {
            if (mark.clipToBib && coverage == 0) continue;
            GraphicsState saved = g.Save();
            if (mark.clipToBib && bib) {
                std::vector<PointF> clipPoints;
                for (const auto& p : bib->points) clipPoints.push_back(coatMarkPoint(p, true, false, coat, seatX, seatY, head, physics, true));
                GraphicsPath clip; clip.AddPolygon(clipPoints.data(), (INT)clipPoints.size()); g.SetClip(&clip, CombineModeIntersect);
            }
            std::vector<PointF> points;
            for (auto p : mark.points) {
                if (mark.clipToBib) {
                    float rim = std::min(1.0f, std::max(0.0f, (p.y - 20.0f) / 6.0f));
                    p.x *= 1.0f - 0.5f * rim * (1.0f - coverage);
                    if (p.y > 20.0f) p.y = 20.0f + (p.y - 20.0f) * (0.75f + 0.25f * coverage);
                }
                points.push_back(coatMarkPoint(p, mark.head, mark.skull, coat, seatX, seatY, head, physics, mark.rgb == 0xF2EEE4));
            }
            GraphicsPath path;
            Color color = rgb(mark.rgb, (BYTE)(mark.alpha * 255));
            if (mark.width == 0) { path.AddPolygon(points.data(), (INT)points.size()); SolidBrush brush(color); g.FillPath(&brush, &path); }
            else { path.AddLines(points.data(), (INT)points.size()); Pen pen(color, mark.width); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound); g.DrawPath(&pen, &path); }
            g.Restore(saved);
        }
    }
    template <class Map>
    static std::vector<Gdiplus::PointF> mapPoints(const std::vector<SkinPoint>& pts, Map map) {
        std::vector<Gdiplus::PointF> out;
        out.reserve(pts.size());
        for (const SkinPoint& p : pts) out.push_back(map(p));
        return out;
    }
    // Strokes get round caps, except tail-space strokes (bands around the tail) which are cut flat.
    template <class Map>
    static void drawMark(Gdiplus::Graphics& g, const SkinMark& mark, Map map, bool roundCaps = true) {
        using namespace Gdiplus;
        std::vector<PointF> points = mapPoints(mark.points, map);
        GraphicsPath path;
        Color color = col(mark.color);
        if (!mark.stroke) { path.AddPolygon(points.data(), (INT)points.size()); SolidBrush brush(color); g.FillPath(&brush, &path); }
        else if (!roundCaps) {
            Pen pen(color, mark.width);
            if (points.size() == 2) g.DrawLine(&pen, points[0].X, points[0].Y, points[1].X, points[1].Y);
            else { pen.SetLineJoin(LineJoinRound); g.DrawLines(&pen, points.data(), (INT)points.size()); }
        }
        else { path.AddLines(points.data(), (INT)points.size()); Pen pen(color, mark.width); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound); g.DrawPath(&pen, &path); }
    }
    template <class Map>
    static void drawRegion(Gdiplus::Graphics& g, const SkinRegion& region, Map map) {
        using namespace Gdiplus;
        std::vector<PointF> points = mapPoints(region.flat, map);
        SolidBrush brush(col(region.fill));
        g.FillPolygon(&brush, points.data(), (INT)points.size(), FillModeWinding);
    }
    // The head (above) or body (below) side of the jaw line, following the head like head marks do.
    static void jawSide(Gdiplus::GraphicsPath& path, bool headSide, const SkinSpec& skin, Pt head, const CatPhysics& physics) {
        std::vector<Gdiplus::PointF> pts = mapPoints(skinJawSide(headSide, skin.shape.cheekFullness),
            [&](SkinPoint p) { return headPoint(p, false, skin, head, physics); });
        path.AddPolygon(pts.data(), (INT)pts.size());
    }

    static Gdiplus::Color eyePrimary(EyeColorTheme t, BYTE a = 255) {
        switch (t) {
            case EyeColorTheme::NeonChartreuse: return Gdiplus::Color(a, 201, 242, 64);
            case EyeColorTheme::SiameseBlue:    return Gdiplus::Color(a, 176, 218, 242);
            case EyeColorTheme::WarmAmber:      return Gdiplus::Color(a, 250, 194, 56);
            case EyeColorTheme::IceCyan:        return Gdiplus::Color(a, 115, 235, 245);
            case EyeColorTheme::ReferenceHazel: return Gdiplus::Color(a, 218, 178, 87);
            case EyeColorTheme::OliveGold:      return Gdiplus::Color(a, 191, 179, 102);
            case EyeColorTheme::SoftAmber: return Gdiplus::Color(a, 217, 173, 85);
        }
        return Gdiplus::Color(a, 201, 242, 64);
    }

    static Gdiplus::Color blendColor(Gdiplus::Color a, Gdiplus::Color b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        auto lerp = [t](BYTE x, BYTE y) -> BYTE { return (BYTE)std::clamp((int)(x + (y - x) * t + 0.5f), 0, 255); };
        return Gdiplus::Color(lerp(a.GetA(), b.GetA()), lerp(a.GetR(), b.GetR()), lerp(a.GetG(), b.GetG()), lerp(a.GetB(), b.GetB()));
    }

    // Hue wheel helper for the keyboard marquee (one full hue cycle per sweep).
    static Gdiplus::Color hsvColor(float h, float s, float v, BYTE a = 255) {
        h = h - std::floor(h);
        float c = v * s;
        float x = c * (1.0f - std::fabs(std::fmod(h * 6.0f, 2.0f) - 1.0f));
        float m = v - c;
        float r = 0.0f, g = 0.0f, b = 0.0f;
        switch ((int)(h * 6.0f) % 6) {
            case 0: r = c; g = x; break;
            case 1: r = x; g = c; break;
            case 2: g = c; b = x; break;
            case 3: g = x; b = c; break;
            case 4: r = x; b = c; break;
            default: r = c; b = x; break;
        }
        return Gdiplus::Color(a, (BYTE)((r + m) * 255.0f), (BYTE)((g + m) * 255.0f), (BYTE)((b + m) * 255.0f));
    }

    static void addRoundedRect(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
        float d = std::min(r * 2.0f, std::min(w, h));
        if (d <= 0.5f) {
            p.AddRectangle(Gdiplus::RectF(x, y, w, h));
            return;
        }
        p.AddArc(x, y, d, d, 180.0f, 90.0f);
        p.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
        p.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
        p.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
        p.CloseFigure();
    }

    static void addQuadPoly(Gdiplus::GraphicsPath& p, Pt tl, Pt tr, Pt br, Pt bl) {
        Gdiplus::PointF pts[4] = { {tl.x, tl.y}, {tr.x, tr.y}, {br.x, br.y}, {bl.x, bl.y} };
        p.AddPolygon(pts, 4);
    }

    static void addQuadBezier(Gdiplus::GraphicsPath& p, Pt p0, Pt ctrl, Pt p2) {
        // Convert quadratic Bezier (p0, ctrl, p2) -> exact cubic Bezier for GDI+
        Gdiplus::PointF c1(p0.x + (2.0f / 3.0f) * (ctrl.x - p0.x), p0.y + (2.0f / 3.0f) * (ctrl.y - p0.y));
        Gdiplus::PointF c2(p2.x + (2.0f / 3.0f) * (ctrl.x - p2.x), p2.y + (2.0f / 3.0f) * (ctrl.y - p2.y));
        p.AddBezier(Gdiplus::PointF(p0.x, p0.y), c1, c2, Gdiplus::PointF(p2.x, p2.y));
    }

    static Pt projectHead3D(float x, float y, float z, float yawRad, float pitchRad) {
        float cy = std::cos(yawRad), sy = std::sin(yawRad);
        float x1 = x * cy + z * sy;
        float z1 = -x * sy + z * cy;
        float cp = std::cos(pitchRad), sp = std::sin(pitchRad);
        float y2 = y * cp + z1 * sp;
        return { x1, y2 };
    }

    static Pt quadPoint(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { mt * mt * p0.x + 2.0f * mt * t * p1.x + t * t * p2.x,
                 mt * mt * p0.y + 2.0f * mt * t * p1.y + t * t * p2.y };
    }

    static Pt quadTangent(Pt p0, Pt p1, Pt p2, float t) {
        float mt = 1.0f - t;
        return { 2.0f * mt * (p1.x - p0.x) + 2.0f * t * (p2.x - p1.x),
                 2.0f * mt * (p1.y - p0.y) + 2.0f * t * (p2.y - p1.y) };
    }

    // MARK: - Mini White Mouse (180° facing the cat)

    static void drawMouse(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme) {
        using namespace Gdiplus;
        Pt center = { CatPhysics::MouseBaseCenter.x + physics.mouseOffset.point().x,
                      CatPhysics::MouseBaseCenter.y + physics.mouseOffset.point().y };
        float pressShift = std::max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6f;

        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y + pressShift);
        g.RotateTransform(0.22f * RadToDeg);

        SolidBrush shadowBrush(Color(60, 0, 0, 0));
        g.FillEllipse(&shadowBrush, -13.5f, -9.5f, 27.0f, 31.0f);

        Color bodyCol, borderCol, wheelCol;
        if (theme == KeyboardTheme::Graphite) {
            bodyCol = Color(255, 69, 71, 77); borderCol = Color(255, 26, 26, 28); wheelCol = Color(255, 102, 105, 110);  // wheel: same grey, a step lighter
        } else if (theme == KeyboardTheme::MidnightHacker) {
            bodyCol = Color(255, 51, 54, 61); borderCol = Color(255, 20, 20, 26); wheelCol = Color(255, 199, 242, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            bodyCol = Color(255, 227, 222, 209); borderCol = Color(255, 51, 48, 46); wheelCol = Color(255, 224, 107, 61);
        } else {
            bodyCol = Color(255, 244, 242, 236); borderCol = Color(255, 36, 36, 41); wheelCol = Color(255, 245, 176, 41);
        }

        SolidBrush bodyBrush(bodyCol);
        g.FillEllipse(&bodyBrush, -13.5f, -16.5f, 27.0f, 33.0f);

        if (physics.mousePressLeft.value > 0.05f || physics.mousePressRight.value > 0.05f) {
            GraphicsPath clipPath;
            clipPath.AddEllipse(-13.5f, -16.5f, 27.0f, 33.0f);
            GraphicsState cs = g.Save();
            g.SetClip(&clipPath);
            if (physics.mousePressLeft.value > 0.05f) {
                SolidBrush hb(theme == KeyboardTheme::Graphite ? Color((BYTE)(96 * physics.mousePressLeft.value), 255, 255, 255)
                                                               : Color((BYTE)(96 * physics.mousePressLeft.value), 201, 242, 64));
                g.FillRectangle(&hb, 0.0f, 0.0f, 14.0f, 17.0f);
            }
            if (physics.mousePressRight.value > 0.05f) {
                SolidBrush hb(theme == KeyboardTheme::Graphite ? Color((BYTE)(96 * physics.mousePressRight.value), 255, 255, 255)
                                                               : Color((BYTE)(96 * physics.mousePressRight.value), 201, 242, 64));
                g.FillRectangle(&hb, -14.0f, 0.0f, 14.0f, 17.0f);
            }
            g.Restore(cs);
        }

        // Front button split seam pointing away from cat (bottom)
        Pen seamPen(Color(82, borderCol.GetR(), borderCol.GetG(), borderCol.GetB()), 1.05f);
        g.DrawLine(&seamPen, 0.0f, 16.0f, 0.0f, 1.5f);

        // Amber scroll wheel on front (bottom) half
        GraphicsPath wheelPath;
        addRoundedRect(wheelPath, -1.9f, 2.5f, 3.8f, 8.5f, 1.9f);
        SolidBrush wheelBrush(wheelCol);
        g.FillPath(&wheelBrush, &wheelPath);

        Pen borderPen(borderCol, 1.7f);
        g.DrawEllipse(&borderPen, -13.5f, -16.5f, 27.0f, 33.0f);
        g.Restore(st);
    }

    // MARK: - Mini 2.5D Graphics Tablet & Stylus

    static void drawTablet(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        using namespace Gdiplus;
        Pt tl = CatPhysics::tabletQuadPoint(0.0f, 0.0f);
        Pt tr = CatPhysics::tabletQuadPoint(1.0f, 0.0f);
        Pt bl = CatPhysics::tabletQuadPoint(0.0f, 1.0f);
        Pt br = CatPhysics::tabletQuadPoint(1.0f, 1.0f);
        constexpr float depth = 3.6f;

        Color baseCol, bodyCol, padCol, keyCol, accentCol;
        if (theme == KeyboardTheme::Graphite) {
            baseCol = Color(255, 20, 20, 23); bodyCol = Color(255, 43, 43, 46);
            padCol = Color(255, 56, 56, 61); keyCol = Color(255, 69, 71, 77); accentCol = keyCol;
        } else if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = Color(255, 13, 13, 18); bodyCol = Color(255, 28, 31, 38);
            padCol = Color(255, 41, 43, 54); keyCol = Color(255, 61, 64, 77); accentCol = Color(255, 199, 242, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = Color(255, 56, 54, 48); bodyCol = Color(255, 84, 79, 71);
            padCol = Color(255, 107, 99, 89); keyCol = Color(255, 138, 128, 115); accentCol = Color(255, 224, 107, 61);
        } else {
            baseCol = Color(255, 26, 26, 31); bodyCol = Color(255, 43, 46, 54);
            padCol = Color(255, 59, 61, 71); keyCol = Color(255, 77, 82, 94); accentCol = Color(255, 245, 176, 41);
        }

        // Drop shadow
        {
            GraphicsPath sp;
            addQuadPoly(sp, {tl.x, tl.y + depth + 2.0f}, {tr.x, tr.y + depth + 2.0f},
                            {br.x, br.y + depth + 2.6f}, {bl.x, bl.y + depth + 2.6f});
            SolidBrush sb(Color(56, 0, 0, 0));
            Pen spen(Color(56, 0, 0, 0), 3.0f); spen.SetLineJoin(LineJoinRound);
            g.FillPath(&sb, &sp); g.DrawPath(&spen, &sp);
        }
        // Lower 3D slab
        {
            GraphicsPath lp;
            addQuadPoly(lp, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
            SolidBrush bb(baseCol);
            Pen bp(baseCol, 2.5f); bp.SetLineJoin(LineJoinRound);
            g.FillPath(&bb, &lp); g.DrawPath(&bp, &lp);
        }
        // Top body
        GraphicsPath topPath;
        addQuadPoly(topPath, tl, tr, br, bl);
        {
            SolidBrush tb(bodyCol);
            Pen tp(bodyCol, 2.0f); tp.SetLineJoin(LineJoinRound);
            g.FillPath(&tb, &topPath); g.DrawPath(&tp, &topPath);
            Pen rim(Color(36, 255, 255, 255), 0.9f); rim.SetLineJoin(LineJoinRound);
            g.DrawPath(&rim, &topPath);
        }
        // Recessed active drawing pad
        Pt pTL = CatPhysics::tabletQuadPoint(0.21f, 0.11f);
        Pt pTR = CatPhysics::tabletQuadPoint(0.94f, 0.11f);
        Pt pBR = CatPhysics::tabletQuadPoint(0.94f, 0.89f);
        Pt pBL = CatPhysics::tabletQuadPoint(0.21f, 0.89f);
        GraphicsPath padPath;
        addQuadPoly(padPath, pTL, pTR, pBR, pBL);
        {
            SolidBrush pb(padCol);
            g.FillPath(&pb, &padPath);
        }
        // Corner L-brackets
        {
            Pen cp(Color(88, 255, 255, 255), 0.95f);
            cp.SetStartCap(LineCapRound); cp.SetEndCap(LineCapRound);
            const float corners[4][4] = {
                {0.24f, 0.15f,  0.07f,  0.11f},
                {0.91f, 0.15f, -0.07f,  0.11f},
                {0.24f, 0.85f,  0.07f, -0.11f},
                {0.91f, 0.85f, -0.07f, -0.11f}
            };
            for (const auto& c : corners) {
                Pt pt = CatPhysics::tabletQuadPoint(c[0], c[1]);
                Pt ph = CatPhysics::tabletQuadPoint(c[0] + c[2], c[1]);
                Pt pv = CatPhysics::tabletQuadPoint(c[0], c[1] + c[3]);
                g.DrawLine(&cp, ph.x, ph.y, pt.x, pt.y);
                g.DrawLine(&cp, pt.x, pt.y, pv.x, pv.y);
            }
        }
        // Status LED + 4 ExpressKeys
        Color eyeCol = eyePrimary(eyeTheme);
        Color ledCol = physics.tabletPenPress.value > 0.15f ? eyeCol : accentCol;
        Pt ledPt = CatPhysics::tabletQuadPoint(0.105f, 0.14f);
        {
            SolidBrush glow(Color(95, ledCol.GetR(), ledCol.GetG(), ledCol.GetB()));
            g.FillEllipse(&glow, ledPt.x - 3.6f, ledPt.y - 3.6f, 7.2f, 7.2f);
            SolidBrush lb(ledCol);
            g.FillEllipse(&lb, ledPt.x - 1.35f, ledPt.y - 1.35f, 2.7f, 2.7f);
        }
        {
            SolidBrush kb(keyCol);
            for (float kv : { 0.28f, 0.44f, 0.60f, 0.76f }) {
                GraphicsPath kp;
                addQuadPoly(kp,
                    CatPhysics::tabletQuadPoint(0.06f, kv - 0.05f),
                    CatPhysics::tabletQuadPoint(0.15f, kv - 0.05f),
                    CatPhysics::tabletQuadPoint(0.15f, kv + 0.05f),
                    CatPhysics::tabletQuadPoint(0.06f, kv + 0.05f));
                g.FillPath(&kb, &kp);
            }
        }
        // Live fading ink trail
        if (physics.tabletTrail.size() >= 2) {
            GraphicsState cs = g.Save();
            g.SetClip(&padPath, CombineModeIntersect);
            for (size_t i = 1; i < physics.tabletTrail.size(); ++i) {
                const auto& a = physics.tabletTrail[i - 1];
                const auto& b = physics.tabletTrail[i];
                if (std::hypot(b.point.x - a.point.x, b.point.y - a.point.y) > 22.0f) continue;
                float life = std::max(0.0f, 1.0f - b.age / 0.72f);
                if (life <= 0.01f) continue;
                Pen ink(Color((BYTE)(life * 224.0f), eyeCol.GetR(), eyeCol.GetG(), eyeCol.GetB()), 1.2f + life * 1.4f);
                ink.SetStartCap(LineCapRound); ink.SetEndCap(LineCapRound);
                g.DrawLine(&ink, a.point.x, a.point.y, b.point.x, b.point.y);
            }
            g.Restore(cs);
        }
        // Nib contact glow
        if (physics.leftPawOnMouse && physics.tabletPenPress.value > 0.08f) {
            Pt nib = physics.tabletNib.point();
            float r = 7.0f + physics.tabletPenPress.value * 3.5f;
            GraphicsPath gp;
            gp.AddEllipse(nib.x - r, nib.y - r, r * 2.0f, r * 2.0f);
            PathGradientBrush pgb(&gp);
            pgb.SetCenterColor(Color((BYTE)(115 * physics.tabletPenPress.value), eyeCol.GetR(), eyeCol.GetG(), eyeCol.GetB()));
            Color surr[1] = { Color(0, eyeCol.GetR(), eyeCol.GetG(), eyeCol.GetB()) };
            INT cnt = 1; pgb.SetSurroundColors(surr, &cnt);
            g.FillPath(&pgb, &gp);
        }
    }

    static void drawStylusBody(Gdiplus::Graphics& g, float len, KeyboardTheme theme, EyeColorTheme eyeTheme, float pressed) {
        using namespace Gdiplus;
        Color barrelCol, gripCol, ringCol;
        if (theme == KeyboardTheme::Graphite) {
            barrelCol = Color(255, 46, 46, 51); gripCol = Color(255, 69, 71, 77); ringCol = Color(255, 102, 105, 110);
        } else if (theme == KeyboardTheme::MidnightHacker) {
            barrelCol = Color(255, 33, 36, 43); gripCol = Color(255, 56, 61, 74); ringCol = Color(255, 199, 242, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            barrelCol = Color(255, 71, 66, 61); gripCol = Color(255, 107, 99, 89); ringCol = Color(255, 224, 107, 61);
        } else {
            barrelCol = Color(255, 46, 48, 59); gripCol = Color(255, 71, 77, 89); ringCol = Color(255, 245, 176, 41);
        }
        GraphicsPath penPath;
        penPath.AddLine(0.0f, 0.0f, 7.5f, -2.4f);
        penPath.AddLine(7.5f, -2.4f, len - 3.0f, -2.6f);
        addQuadBezier(penPath, { len - 3.0f, -2.6f }, { len, -2.4f }, { len, 0.0f });
        addQuadBezier(penPath, { len, 0.0f }, { len, 2.4f }, { len - 3.0f, 2.6f });
        penPath.AddLine(len - 3.0f, 2.6f, 7.5f, 2.4f);
        penPath.CloseFigure();

        Pen halo(Color(204, 245, 245, 245), 1.6f);
        halo.SetLineJoin(LineJoinRound);
        g.DrawPath(&halo, &penPath);
        SolidBrush bb(barrelCol);
        g.FillPath(&bb, &penPath);

        GraphicsState cs = g.Save();
        g.SetClip(&penPath, CombineModeIntersect);
        SolidBrush gb(gripCol);
        g.FillRectangle(&gb, 7.5f, -3.0f, 11.5f, 6.0f);
        SolidBrush rb(ringCol);
        g.FillRectangle(&rb, 19.0f, -3.0f, 2.4f, 6.0f);
        Color nibCol = pressed > 0.15f ? eyePrimary(eyeTheme) : Color(255, 240, 240, 240);
        SolidBrush nb(nibCol);
        g.FillRectangle(&nb, -0.5f, -1.5f, 3.4f, 3.0f);
        SolidBrush sh(Color(60, 255, 255, 255));
        g.FillRectangle(&sh, 8.0f, -1.9f, std::max(1.0f, len - 12.0f), 1.1f);
        g.Restore(cs);

        GraphicsPath btn;
        addRoundedRect(btn, 11.0f, -3.1f, 6.0f, 1.4f, 0.7f);
        SolidBrush btnB(Color(230, 199, 199, 199));
        g.FillPath(&btnB, &btn);
    }

    static void drawParkedStylus(Gdiplus::Graphics& g, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        using namespace Gdiplus;
        Pt nib = CatPhysics::tabletQuadPoint(0.88f, -0.02f);
        Pt tail = CatPhysics::tabletQuadPoint(0.18f, -0.02f);
        float ang = std::atan2(tail.y - nib.y, tail.x - nib.x) * RadToDeg;
        float len = std::hypot(tail.x - nib.x, tail.y - nib.y);

        GraphicsState st = g.Save();
        g.TranslateTransform(nib.x, nib.y + 2.2f);
        g.RotateTransform(ang);
        SolidBrush sb(Color(56, 0, 0, 0));
        g.FillRectangle(&sb, 2.0f, -2.0f, len - 4.0f, 4.5f);
        g.Restore(st);

        st = g.Save();
        g.TranslateTransform(nib.x, nib.y);
        g.RotateTransform(ang);
        drawStylusBody(g, len, theme, eyeTheme, 0.0f);
        g.Restore(st);
    }

    static void drawHeldStylus(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        using namespace Gdiplus;
        Pt lPos = physics.leftPaw.point();
        float press = std::clamp(physics.tabletPenPress.value, 0.0f, 1.0f);
        Pt nib = { lPos.x - 5.5f, lPos.y + 7.0f + press * 1.4f };
        float angleDeg = (-2.18f + physics.tabletPenTilt.value) * RadToDeg;

        GraphicsState st = g.Save();
        g.TranslateTransform(nib.x, nib.y);
        g.RotateTransform(angleDeg);
        drawStylusBody(g, 35.0f, theme, eyeTheme, press);
        g.Restore(st);
    }

    // MARK: - Mini 65% Mechanical Keyboard (180° facing the cat)

    static std::wstring keycapLegend(PhysicalKey k) {
        switch (k) {
            case PhysicalKey::Escape: return L"ESC";
            case PhysicalKey::Tab: return L"\u21E5";
            case PhysicalKey::CapsLock: return L"\u21EA";
            case PhysicalKey::LeftShift:
            case PhysicalKey::RightShift: return L"\u21E7";
            case PhysicalKey::LeftControl:
            case PhysicalKey::RightControl: return L"Ctrl";
            case PhysicalKey::LeftOption:
            case PhysicalKey::RightOption: return L"Alt";
            case PhysicalKey::LeftCommand:
            case PhysicalKey::RightCommand: return L"Win";
            case PhysicalKey::Backspace: return L"\u232B";
            case PhysicalKey::Enter: return L"\u21B5";
            case PhysicalKey::Space: return L"";
            default: return keyBadgeLabel(k);
        }
    }

    static void drawKeyboard(Gdiplus::Graphics& g, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme, bool showLegends, bool marquee) {
        using namespace Gdiplus;
        Pt tl = KeyboardLayout::screenQuadPoint(-0.012f, -0.04f);
        Pt tr = KeyboardLayout::screenQuadPoint(1.012f, -0.04f);
        Pt bl = KeyboardLayout::screenQuadPoint(-0.016f, 1.04f);
        Pt br = KeyboardLayout::screenQuadPoint(1.016f, 1.04f);
        constexpr float depth = 4.5f;

        // Shadow
        {
            GraphicsPath sp;
            addQuadPoly(sp, {tl.x, tl.y + depth + 2}, {tr.x, tr.y + depth + 2},
                            {br.x, br.y + depth + 3}, {bl.x, bl.y + depth + 3});
            SolidBrush sb(Color(56, 0, 0, 0));
            g.FillPath(&sb, &sp);
        }

        Color baseCol, deckCol, keyCol, borderCol, accentCol;
        if (theme == KeyboardTheme::Graphite) {
            baseCol = Color(255, 23, 23, 26); deckCol = Color(255, 36, 36, 38);
            keyCol = Color(255, 69, 71, 77); borderCol = Color(242, 38, 38, 41); accentCol = keyCol;  // no accent keys
        } else if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = Color(255, 15, 15, 20); deckCol = Color(255, 26, 28, 33);
            keyCol = Color(255, 54, 56, 64); borderCol = Color(242, 28, 28, 33); accentCol = Color(255, 196, 240, 64);
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = Color(255, 64, 61, 56); deckCol = Color(255, 97, 92, 84);
            keyCol = Color(255, 230, 224, 209); borderCol = Color(216, 173, 166, 153); accentCol = Color(255, 224, 107, 61);
        } else {
            baseCol = Color(255, 26, 26, 31); deckCol = Color(255, 36, 36, 41);
            keyCol = Color(255, 247, 246, 240); borderCol = Color(190, 199, 196, 189); accentCol = Color(255, 247, 179, 41);
        }

        {
            GraphicsPath lp;
            addQuadPoly(lp, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
            SolidBrush bb(baseCol);
            Pen bp(baseCol, 2.5f); bp.SetLineJoin(LineJoinRound);
            g.FillPath(&bb, &lp);
            g.DrawPath(&bp, &lp);
        }
        GraphicsPath deckPath;
        addQuadPoly(deckPath, tl, tr, br, bl);
        {
            SolidBrush db(deckCol);
            Pen dp(deckCol, 2.0f); dp.SetLineJoin(LineJoinRound);
            g.FillPath(&db, &deckPath);
            g.DrawPath(&dp, &deckPath);
        }

        // MARK: - Marquee backlight
        // A comet of light sweeps diagonally under the keycaps: bright head, long fading tail.
        Color glowCol = eyePrimary(eyeTheme, 215);
        float mqHead = physics.marqueePhase - std::floor(physics.marqueePhase);
        // One full hue cycle per sweep, so the deck reads as RGB backlight rather than paint.
        Color mqCol = hsvColor(mqHead + 0.52f, 0.55f, 1.0f);
        auto mqWave = [mqHead, marquee](float t) {
            if (!marquee) return 0.0f;
            float d = mqHead - t;
            if (d < 0.0f) d += 1.0f;
            return std::exp(-d / 0.125f);
        };
        float bandDx = br.x - tl.x, bandDy = br.y - tl.y;
        float bandLen2 = bandDx * bandDx + bandDy * bandDy;
        if (marquee) {
            GraphicsState mqState = g.Save();
            g.SetClip(&deckPath);
            const int kN = 24;
            Color mqColors[kN];
            REAL mqPos[kN];
            for (int i = 0; i < kN; ++i) {
                float t = (float)i / (float)(kN - 1);
                mqPos[i] = t;
                mqColors[i] = Color((BYTE)std::clamp(mqWave(t) * 200.0f, 0.0f, 255.0f),
                                    mqCol.GetR(), mqCol.GetG(), mqCol.GetB());
            }
            LinearGradientBrush band(PointF(tl.x, tl.y), PointF(br.x, br.y), Color(0, 0, 0, 0), Color(0, 0, 0, 0));
            band.SetInterpolationColors(mqColors, mqPos, kN);
            g.FillPath(&band, &deckPath);

            // White-hot lamp at the head of the comet, clipped to the deck.
            Pt head = { tl.x + bandDx * mqHead, tl.y + bandDy * mqHead };
            float hr = 44.0f;
            GraphicsPath hp;
            hp.AddEllipse(head.x - hr, head.y - hr, hr * 2.0f, hr * 2.0f);
            PathGradientBrush hb(&hp);
            hb.SetCenterPoint(PointF(head.x, head.y));
            hb.SetCenterColor(Color(150, 240, 252, 255));
            Color hSurr[1] = { Color(0, mqCol.GetR(), mqCol.GetG(), mqCol.GetB()) };
            INT hCnt = 1;
            hb.SetSurroundColors(hSurr, &hCnt);
            g.FillPath(&hb, &hp);
            g.Restore(mqState);
        }

        constexpr float padU = 0.0028f;
        constexpr float padV = 0.014f;
        Pen keyBorderPen(borderCol, 0.65f);
        FontFamily kbFF(L"Segoe UI");
        StringFormat kbSF;
        kbSF.SetAlignment(StringAlignmentCenter);
        kbSF.SetLineAlignment(StringAlignmentCenter);

        for (const auto& cap : KeyboardLayout::keycaps()) {
            float glow = 0.0f;
            auto it = physics.keyGlow.find(cap.key);
            if (it != physics.keyGlow.end()) glow = it->second;

            // 180° rotated to face the cat
            float su0 = (1.0f - (cap.xUnits + cap.widthUnits)) + padU;
            float su1 = (1.0f - cap.xUnits) - padU;
            float sv0 = (1.0f - (cap.row + 1) / 5.0f) + padV;
            float sv1 = (1.0f - cap.row / 5.0f) - padV;

            float dy = glow * 1.9f;
            Pt kTL = KeyboardLayout::screenQuadPoint(su0, sv0); kTL.y += dy;
            Pt kTR = KeyboardLayout::screenQuadPoint(su1, sv0); kTR.y += dy;
            Pt kBR = KeyboardLayout::screenQuadPoint(su1, sv1); kBR.y += dy;
            Pt kBL = KeyboardLayout::screenQuadPoint(su0, sv1); kBL.y += dy;

            // Where this keycap sits along the marquee axis, and how bright the comet makes it.
            float kcx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25f;
            float kcy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25f;
            float kt = std::clamp(((kcx - tl.x) * bandDx + (kcy - tl.y) * bandDy) / bandLen2, 0.0f, 1.0f);
            float mq = mqWave(kt);

            GraphicsPath kp;
            addQuadPoly(kp, kTL, kTR, kBR, kBL);
            Color fill = cap.isAmberAccent ? accentCol : keyCol;
            if (mq > 0.015f) {
                // Head of the comet reads hot (near white), the trailing keys stay saturated.
                Color lit = blendColor(mqCol, Color(255, 255, 255, 255), mq * mq * 0.5f);
                fill = blendColor(fill, lit, std::clamp(mq * 0.88f, 0.0f, 0.9f));
            }
            if (glow > 0.02f) fill = blendColor(fill, glowCol, glow * 0.55f);

            SolidBrush kb(fill);
            g.FillPath(&kb, &kp);
            if (mq > 0.10f) {
                Pen litPen(Color((BYTE)std::clamp(mq * 215.0f, 0.0f, 215.0f),
                                 mqCol.GetR(), mqCol.GetG(), mqCol.GetB()), 1.0f);
                g.DrawPath(&litPen, &kp);
            } else {
                g.DrawPath(&keyBorderPen, &kp);
            }

            // Under-key bloom where the beam is hottest, so the light spills between caps.
            if (mq > 0.45f) {
                float bloom = (mq - 0.45f) / 0.55f;
                float br2 = 15.0f;
                GraphicsPath bp;
                bp.AddEllipse(kcx - br2, kcy - br2 * 0.7f, br2 * 2.0f, br2 * 1.4f);
                PathGradientBrush pb(&bp);
                pb.SetCenterPoint(PointF(kcx, kcy));
                pb.SetCenterColor(Color((BYTE)std::clamp(bloom * 135.0f, 0.0f, 135.0f),
                                        mqCol.GetR(), mqCol.GetG(), mqCol.GetB()));
                Color bSurr[1] = { Color(0, mqCol.GetR(), mqCol.GetG(), mqCol.GetB()) };
                INT bCnt = 1;
                pb.SetSurroundColors(bSurr, &bCnt);
                g.FillPath(&pb, &bp);
            }

            if (showLegends) {
                bool isSpace = (cap.key == PhysicalKey::Space);
                std::wstring legend = isSpace ? std::wstring(L"space") : keycapLegend(cap.key);
                if (!legend.empty()) {
                    float cx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25f;
                    float cy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25f;
                    if (isSpace) {
                        // The spacebar sits right under the cat: its legs hide the middle, and an
                        // unlabeled visible end looked like an odd blank patch beside the cat.
                        // Label the end that stays visible instead.
                        Pt sp = KeyboardLayout::screenQuadPoint(su0 + (su1 - su0) * 0.82f, (sv0 + sv1) * 0.5f);
                        cx = sp.x;
                        cy = sp.y + dy;
                    }
                    Color txtCol = (theme != KeyboardTheme::Graphite && (cap.isAmberAccent || theme != KeyboardTheme::MidnightHacker))
                        ? Color(198, 36, 36, 41)
                        : Color(210, 224, 230, 235);
                    SolidBrush tb(txtCol);
                    float fSz = legend.size() <= 1 ? 5.2f : 3.9f;
                    Font kf(&kbFF, fSz, FontStyleBold, UnitPixel);
                    GraphicsState ks = g.Save();
                    g.TranslateTransform(cx, cy);
                    // The keyboard is laid out 180° to face the cat, so the legends must be too
                    // (upside-down to the viewer, upright from the cat's seat). +3° follows the deck slope.
                    g.RotateTransform(180.0f + 3.0f);
                    g.DrawString(legend.c_str(), -1, &kf, RectF(-14.0f, -7.0f, 28.0f, 14.0f), &kbSF, &tb);
                    g.Restore(ks);
                }
            }
        }

        // Radiant backlit glow halos over pressed keys
        for (const auto& kv : physics.keyGlow) {
            if (kv.second <= 0.05f) continue;
            Pt c = KeyboardLayout::canvasPointForKey(kv.first);
            float r = 17.0f + kv.second * 6.0f;
            GraphicsPath gp;
            gp.AddEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
            PathGradientBrush pgb(&gp);
            pgb.SetCenterColor(Color((BYTE)(92 * kv.second), glowCol.GetR(), glowCol.GetG(), glowCol.GetB()));
            Color surr[1] = { Color(0, glowCol.GetR(), glowCol.GetG(), glowCol.GetB()) };
            INT cnt = 1;
            pgb.SetSurroundColors(surr, &cnt);
            g.FillPath(&pgb, &gp);
        }
    }

    // MARK: - Tail

    struct TailCurve {
        Gdiplus::PointF root, c1, c2, tip;
        float thickness;
        // Tail space: x = 0 (root) .. 100 (tip) along the curve, y = offset along the left-hand normal.
        Gdiplus::PointF at(SkinPoint p) const {
            float t = std::clamp(p.x / 100.0f, 0.0f, 1.0f);
            float u=1-t;
            float x=u*u*u*root.X+3*u*u*t*c1.X+3*u*t*t*c2.X+t*t*t*tip.X;
            float y=u*u*u*root.Y+3*u*u*t*c1.Y+3*u*t*t*c2.Y+t*t*t*tip.Y;
            float dx=3*u*u*(c1.X-root.X)+6*u*t*(c2.X-c1.X)+3*t*t*(tip.X-c2.X);
            float dy=3*u*u*(c1.Y-root.Y)+6*u*t*(c2.Y-c1.Y)+3*t*t*(tip.Y-c2.Y);
            float len=std::max(0.001f,std::hypot(dx,dy)),n=p.y*thickness;
            return Gdiplus::PointF(x+-dy/len*n,y+dx/len*n);
        }
    };
    static TailCurve tailCurve(float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin) {
        using namespace Gdiplus;
        float p = physics.tailPhase;
        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        float agility = std::clamp(physics.tailAgility.value, 0.0f, 1.0f);
        float wag1 = std::sin(p) * (8.0f + hunt * 5.5f + agility * 4.0f);
        float wag2 = std::cos(p * 1.3f) * (6.0f + hunt * 4.5f + agility * 3.5f);
        float huntDrop = hunt * 14.0f;
        float bw = skin.shape.bodyWidth, tl = skin.shape.tailLength;
        float shift = bw == 1.0f ? 0.0f : 34.0f * bw - 34.0f; // tail root stays on the (wider/narrower) hip

        float reach = std::clamp(physics.tailTyping.value, 0.0f, 1.0f);
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float flickScale = (1.0f - reach) * (1.0f - lieS);
        Pt flick = physics.tailFlick.point();
        float fx = flick.x * flickScale;
        float fy = flick.y * flickScale;
        float curl = physics.tailCurl.value * flickScale;

        TailCurve c;
        c.root = PointF(seatX + 34.0f + shift, seatY - 6.0f);
        c.c1 = PointF(seatX + 64.0f + shift + fx * 0.25f, seatY + 2.0f + wag1 * 0.35f + huntDrop * 0.4f + fy * 0.22f + curl * 0.35f);
        c.c2 = PointF(seatX + 66.0f + wag2 * 0.4f + shift + fx * 0.65f - curl * 0.45f, seatY - 26.0f + wag1 * 0.7f + huntDrop + fy * 0.60f - curl * 0.55f);
        c.tip = PointF(seatX + 91.0f + wag2 * 0.5f + shift + fx, seatY - 30.0f + wag1 * 0.85f + huntDrop + fy);
        for (PointF* q : {&c.c1, &c.c2, &c.tip}) { q->X = around(q->X, c.root.X, tl); q->Y = around(q->Y, c.root.Y, tl); }

        Pt target = physics.tailTip.point();
        c.c1.X += 12.0f * reach;
        c.c1.Y -= 18.0f * reach;
        c.c2.X += (target.x + 24.0f - c.c2.X) * reach;
        c.c2.Y += (target.y - 30.0f - c.c2.Y) * reach;
        c.tip.X += (target.x - c.tip.X) * reach;
        c.tip.Y += (target.y - c.tip.Y) * reach;

        // When curling up to sleep, tail sweeps smoothly around the front to wrap cozily around the cat ball!
        float sleepWag = std::sin(p * 0.6f) * 2.0f;
        c.root.X += (seatX + 40.0f + shift - c.root.X) * lieS;
        c.root.Y += (seatY - 2.0f - c.root.Y) * lieS;
        c.c1.X += (seatX + 54.0f + shift - c.c1.X) * lieS;
        c.c1.Y += (seatY + 13.0f - c.c1.Y) * lieS;
        c.c2.X += (seatX + 14.0f - c.c2.X) * lieS;
        c.c2.Y += (seatY + 17.0f - c.c2.Y) * lieS;
        c.tip.X += (seatX - 28.0f - c.tip.X) * lieS;
        c.tip.Y += (seatY + 8.0f + sleepWag - c.tip.Y) * lieS;

        c.thickness = skin.shape.tailThickness;
        return c;
    }

    static void drawTail(Gdiplus::Graphics& g, float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        using namespace Gdiplus;
        TailCurve c = tailCurve(seatX, seatY, physics, skin);
        float width = isDazed(skin) ? 13.0f : (buildOf(skin).tailWidth * skin.shape.tailThickness);

        if (outlineOnly) {
            Pen outPen(blackOutline(), width + BlackOutlineW * 2.0f);
            outPen.SetStartCap(LineCapRound); outPen.SetEndCap(LineCapRound); outPen.SetLineJoin(LineJoinRound);
            g.DrawBezier(&outPen, c.root, c.c1, c.c2, c.tip);
            return;
        }

        Pen tailPen(col(skin.palette.tail), width);
        tailPen.SetStartCap(LineCapRound);
        tailPen.SetEndCap(LineCapRound);
        tailPen.SetLineJoin(LineJoinRound);
        if (isTabby(skin)) {
            tabby::drawTail(g, tailPen, c.root, c.c1, c.c2, c.tip);
            return;
        }
        g.DrawBezier(&tailPen, c.root, c.c1, c.c2, c.tip);

        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        if (lieS > 0.35f) {
            float rAlpha = std::min(1.0f, (lieS - 0.35f) / 0.45f) * 0.72f;
            Color rc = col(skin.palette.crease);
            Pen rimPen(Color((BYTE)(rc.GetA() * rAlpha), rc.GetR(), rc.GetG(), rc.GetB()), 1.45f);
            rimPen.SetStartCap(LineCapRound); rimPen.SetEndCap(LineCapRound);
            g.DrawBezier(&rimPen,
                PointF(c.root.X - 4.0f, c.root.Y - 3.0f),
                PointF(c.c1.X - 6.0f, c.c1.Y - 5.5f),
                PointF(c.c2.X, c.c2.Y - 5.5f),
                PointF(c.tip.X + 2.0f, c.tip.Y - 4.5f));
        }
        if (skin.hasPartArt(SkinSpace::Tail) || skin.palette.tailTip.set) {
            GraphicsState state = g.Save();
            GraphicsPath outline; outline.AddBezier(c.root,c.c1,c.c2,c.tip); outline.Widen(&tailPen);
            g.SetClip(&outline, CombineModeIntersect);
            auto map = [&](SkinPoint p) { return c.at(p); };
            for (const SkinRegion& r : skin.regions) if (r.space == SkinSpace::Tail) drawRegion(g, r, map);
            if (skin.palette.tailTip.set) {
                // Last 22% of the tail: de Casteljau split at t = 0.78, stroked with the tail width.
                const float t = 0.78f, u = 1 - t;
                PointF a(u*c.root.X+t*c.c1.X, u*c.root.Y+t*c.c1.Y), b(u*c.c1.X+t*c.c2.X, u*c.c1.Y+t*c.c2.Y), q(u*c.c2.X+t*c.tip.X, u*c.c2.Y+t*c.tip.Y);
                PointF d(u*a.X+t*b.X, u*a.Y+t*b.Y), e(u*b.X+t*q.X, u*b.Y+t*q.Y);
                PointF s(u*d.X+t*e.X, u*d.Y+t*e.Y);
                Pen tipPen(col(skin.palette.tailTip), width + 2.0f);
                tipPen.SetStartCap(LineCapRound); tipPen.SetEndCap(LineCapRound);
                g.DrawBezier(&tipPen, s, e, q, c.tip);
            }
            for (const SkinMark& m : skin.marks) if (m.space == SkinSpace::Tail) drawMark(g, m, map, false);
            g.Restore(state);
        }
    }

    // MARK: - 3D-Projected Ears

    static void drawEarsLayer(Gdiplus::Graphics& g, Pt center, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        using namespace Gdiplus;
        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);
        if (skin.shape.headScale != 1.0f) g.ScaleTransform(skin.shape.headScale, skin.shape.headScale);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;
        float fold = physics.earFold.value;
        float twitch = physics.earTwitch.value * 0.045f;

        drawEar3D(g, true, yawRad, pitchRad, fold, twitch, skin, outlineOnly);
        drawEar3D(g, false, yawRad, pitchRad, fold, -twitch, skin, outlineOnly);
        g.Restore(st);
    }

    static void drawEar3D(Gdiplus::Graphics& g, bool isLeft, float yawRad, float pitchRad, float fold, float twitch, const SkinSpec& skin, bool outlineOnly = false) {
        using namespace Gdiplus;
        const Build& bd = buildOf(skin);
        float sign = isLeft ? -1.0f : 1.0f;
        if (isDazed(skin)) {
            if (!outlineOnly) drawDazedEar(g, sign, yawRad, pitchRad, fold, twitch);
            return;
        }
        float foldAngle = sign * (bd.earFold + fold * 0.34f) + twitch;
        float earLen = bd.earLen;
        float tipDx = std::sin(foldAngle) * earLen;
        float tipDy = -std::cos(foldAngle) * earLen;

        Pt innerBase = projectHead3D(sign * bd.earInX, bd.earInY, 0.0f, yawRad, pitchRad);
        Pt outerBase = projectHead3D(sign * bd.earOutX, bd.earOutY, -2.0f, yawRad, pitchRad);
        Pt tip3D     = projectHead3D(sign * bd.earTipX + tipDx, bd.earTipY + tipDy, -3.0f, yawRad, pitchRad);
        float es = skin.shape.earSize;
        if (es != 1.0f) { // grow / shrink around the middle of the ear base
            Pt m = { (innerBase.x + outerBase.x) * 0.5f, (innerBase.y + outerBase.y) * 0.5f };
            innerBase = { around(innerBase.x, m.x, es), around(innerBase.y, m.y, es) };
            outerBase = { around(outerBase.x, m.x, es), around(outerBase.y, m.y, es) };
            tip3D = { around(tip3D.x, m.x, es), around(tip3D.y, m.y, es) };
        }

        float ep = skin.shape.earPointiness;
        float tipSpread = bd.earTipSpread / ep;
        Pt tipL = { tip3D.x - sign * tipSpread, tip3D.y + 2.8f };
        Pt tipR = { tip3D.x + sign * tipSpread, tip3D.y + 3.2f };

        GraphicsPath ear;
        addQuadBezier(ear, innerBase, { (innerBase.x + tipL.x) * 0.5f - sign * 1.5f, (innerBase.y + tipL.y) * 0.5f - 2.0f }, tipL);
        addQuadBezier(ear, tipL, { tip3D.x, tip3D.y - 2.2f * ep }, tipR);
        addQuadBezier(ear, tipR, { (outerBase.x + tipR.x) * 0.5f + sign * 2.5f, (outerBase.y + tipR.y) * 0.5f - 2.0f }, outerBase);
        ear.CloseFigure();

        if (isTabby(skin)) {
            if (outlineOnly) return;
            LinearGradientBrush fur(PointF(tip3D.x, tip3D.y - 2), PointF(outerBase.x, outerBase.y + 2),
                                    tabby::dark(), tabby::oat());
            fur.SetWrapMode(WrapModeTileFlipXY);
            g.FillPath(&fur, &ear);
            auto earPoint = [&](float width, float height) {
                float x = innerBase.x + (outerBase.x - innerBase.x) * width;
                float y = innerBase.y + (outerBase.y - innerBase.y) * width;
                return PointF(x + (tip3D.x - x) * height, y + (tip3D.y - y) * height);
            };
            SolidBrush lining(Color(185, 151, 128));
            tabby::shape(g, earPoint, lining, {.29f, .1f},
                         {{.2f, .37f, .38f, .67f, .48f, .75f},
                          {.60f, .63f, .77f, .26f, .76f, .1f},
                          {.68f, .16f, .44f, .18f, .29f, .1f}});
            return;
        }
        if (outlineOnly) {
            Pen outPen(blackOutline(), BlackOutlineW * 2.0f);
            outPen.SetLineJoin(LineJoinRound); outPen.SetStartCap(LineCapRound); outPen.SetEndCap(LineCapRound);
            g.DrawPath(&outPen, &ear);
            return;
        }
        SolidBrush b(col(skin.palette.earOuter));
        g.FillPath(&b, &ear);

        if (skin.palette.earInner.set) {
            Pt inI  = { innerBase.x + (tip3D.x - innerBase.x) * 0.24f + sign * 3.0f, innerBase.y + (tip3D.y - innerBase.y) * 0.16f };
            Pt outI = { outerBase.x + (tip3D.x - outerBase.x) * 0.20f - sign * 3.8f, outerBase.y + (tip3D.y - outerBase.y) * 0.18f };
            Pt tipI = { (innerBase.x + outerBase.x) * 0.16f + tip3D.x * 0.68f, (innerBase.y + outerBase.y) * 0.14f + tip3D.y * 0.72f + 2.5f };
            GraphicsPath inner;
            addQuadBezier(inner, inI, { (inI.x + tipI.x) * 0.5f - sign * 0.8f, (inI.y + tipI.y) * 0.5f }, tipI);
            addQuadBezier(inner, tipI, { (outI.x + tipI.x) * 0.5f + sign * 1.2f, (outI.y + tipI.y) * 0.5f }, outI);
            inner.CloseFigure();
            SolidBrush ib(col(skin.palette.earInner));
            g.FillPath(&ib, &inner);
        }

        SkinSpace space = isLeft ? SkinSpace::LeftEar : SkinSpace::RightEar;
        if (skin.hasPartArt(space)) {
            // Ear space: (-50,0) inner base, (50,0) outer base, (0,-100) tip; mirrored per ear.
            Pt m = { (innerBase.x + outerBase.x) * 0.5f, (innerBase.y + outerBase.y) * 0.5f };
            auto map = [&](SkinPoint p) {
                float u = p.x / 100.0f, v = -p.y / 100.0f;
                return PointF(m.x + u * (outerBase.x - innerBase.x) + v * (tip3D.x - m.x), m.y + u * (outerBase.y - innerBase.y) + v * (tip3D.y - m.y));
            };
            GraphicsState cs = g.Save();
            g.SetClip(&ear, CombineModeIntersect);
            for (const SkinRegion& r : skin.regions) if (r.space == space) drawRegion(g, r, map);
            for (const SkinMark& mk : skin.marks) if (mk.space == space) drawMark(g, mk, map);
            g.Restore(cs);
        }
    }

    static void drawDazedEar(Gdiplus::Graphics& g, float sign, float yawRad, float pitchRad, float fold, float twitch) {
        using namespace Gdiplus;
        float foldAngle = sign * fold * 0.34f + twitch;
        float tipDx = std::sin(foldAngle) * 37.0f;
        float tipDy = (1.0f - std::cos(foldAngle)) * 37.0f;
        auto project = [&](float x, float y, float tipWeight) {
            Pt point = projectHead3D(sign * x + tipDx * tipWeight, y + tipDy * tipWeight, -3.0f * tipWeight, yawRad, pitchRad);
            return PointF(point.x, point.y);
        };

        PointF innerBase = project(5.0f, -25.0f, 0.0f);
        PointF tip = project(40.0f, -63.0f, 1.0f);
        PointF outerBase = project(42.5f, -5.0f, 0.0f);
        GraphicsPath ear;
        ear.AddBezier(innerBase, project(13.0f, -46.0f, 0.30f), project(31.0f, -62.0f, 0.90f), tip);
        ear.AddBezier(tip, project(54.0f, -66.0f, 0.95f), project(57.0f, -34.0f, 0.40f), outerBase);
        ear.CloseFigure();

        LinearGradientBrush fabric(tip, outerBase, rgb(0x777FBD), rgb(0x4D5892));
        fabric.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&fabric, &ear);
    }

    // MARK: - Unified C1-Continuous Cat Silhouette (+ regions, colour-point mask, marks)

    static void drawUnifiedSilhouette(Gdiplus::Graphics& g, float seatX, float seatY, Pt headCenter, float breath, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        using namespace Gdiplus;
        if (isDazed(skin)) {
            if (!outlineOnly) drawDazedSilhouette(g, seatX, seatY, headCenter, physics);
            return;
        }
        const Build& bd = buildOf(skin);
        const SkinPalette& pal = skin.palette;
        float yaw = physics.headYaw.value;
        float pitch = physics.headPitch.value;
        float hTilt = physics.headTilt.value;
        float bTilt = physics.bodyTilt.value;
        float hs = skin.shape.headScale, bw = skin.shape.bodyWidth;

        float cH = std::cos(hTilt), sH = std::sin(hTilt);
        auto H = [&](float lx, float ly) -> PointF {
            lx *= hs; ly *= hs;
            return PointF(headCenter.x + lx * cH - ly * sH, headCenter.y + lx * sH + ly * cH);
        };

        float cB = std::cos(bTilt), sB = std::sin(bTilt);
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        auto B = [&](float lx, float ly) -> PointF {
            float rx = lx * bw * sx, ry = ly * sy;
            return PointF(seatX + rx * cB - ry * sB, seatY + rx * sB + ry * cB);
        };

        float cheekShift = yaw * 3.0f;
        float crownDrop = pitch * 2.0f;
        float cheekHalfW = bd.cheekHalfW * skin.shape.cheekFullness;
        float crownCtrlW = bd.crownCtrlW;
        float crownBaseY = bd.crownBaseY;
        float cheekY = bd.cheekY;
        float cheekCtrlTopY = bd.cheekCtrlTopY;
        float neckPinchX = bd.neckPinchX;
        float neckCtrlY = bd.neckCtrlY;
        float hipHalfW = bd.hipHalfW;
        float waistCtrlW = bd.waistCtrlW;
        float waistCtrlY = bd.waistCtrlY;
        float botOuterW = bd.botOuterW;
        float botInnerW = bd.botInnerW;
        float botHalfW = bd.botHalfW;
        float leftCheekX = -cheekHalfW + cheekShift * 0.4f;
        float rightCheekX = cheekHalfW + cheekShift * 0.4f;

        PointF crown  = H(cheekShift * 0.2f, crownBaseY + crownDrop);
        PointF rCheek = H(rightCheekX, cheekY);
        PointF rHip   = B(hipHalfW, -14.0f);
        PointF rBot   = B(botHalfW, 12.0f);
        PointF lBot   = B(-botHalfW, 12.0f);
        PointF lHip   = B(-hipHalfW, -14.0f);
        PointF lCheek = H(leftCheekX, cheekY);

        GraphicsPath path;
        path.AddBezier(crown, H(crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop), H(rightCheekX + 1.5f, cheekCtrlTopY), rCheek);
        path.AddBezier(rCheek, H(rightCheekX - neckPinchX, neckCtrlY), B(waistCtrlW, waistCtrlY + breath * 0.5f), rHip);
        path.AddBezier(rHip, B(botOuterW, 5.0f), B(botInnerW, 12.0f), rBot);
        path.AddLine(rBot, lBot);
        path.AddBezier(lBot, B(-botInnerW, 12.0f), B(-botOuterW, 5.0f), lHip);
        path.AddBezier(lHip, B(-waistCtrlW, waistCtrlY + breath * 0.5f), H(leftCheekX + neckPinchX, neckCtrlY), lCheek);
        path.AddBezier(lCheek, H(leftCheekX - 1.5f, cheekCtrlTopY), H(-crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop), crown);
        path.CloseFigure();

        float stagger = skin.shape.pawStagger;
        float hindW = bd.hindW;
        float leftHindX = bw == 1.0f ? bd.hindLeftX : (bd.hindLeftX + hindW * 0.5f) * bw - hindW * 0.5f;
        float rightHindX = bw == 1.0f ? bd.hindRightX : (bd.hindRightX + hindW * 0.5f) * bw - hindW * 0.5f;
        float leftHindY = bd.hindLeftY;
        float leftHindH = bd.hindLeftH;
        float rightHindY = bd.hindLeftY + (bd.hindStaggerY - bd.hindLeftY) * stagger;
        float rightHindH = bd.hindLeftH + (bd.hindStaggerH - bd.hindLeftH) * stagger;
        bool rightTucked = physics.hindFootActive.value < 0.35f;

        if (outlineOnly) {
            Pen outPen(blackOutline(), BlackOutlineW * 2.0f);
            outPen.SetLineJoin(LineJoinRound); outPen.SetStartCap(LineCapRound); outPen.SetEndCap(LineCapRound);
            g.DrawPath(&outPen, &path);
            GraphicsState ps = g.Save();
            g.TranslateTransform(seatX, seatY);
            g.RotateTransform(bTilt * RadToDeg);
            g.ScaleTransform(sx, sy);
            g.DrawEllipse(&outPen, leftHindX, leftHindY, hindW, leftHindH);
            if (rightTucked) {
                g.DrawEllipse(&outPen, rightHindX, rightHindY, hindW, rightHindH);
            }
            g.Restore(ps);
            return;
        }

        if (isTabby(skin)) {
            auto face = [&](float x, float y) {
                float depth = std::clamp(29.f - .012f * x * x - .22f * std::abs(y + 2.f), 3.f, 28.f);
                Pt projected = projectHead3D(x, y, depth, yaw * .50f, pitch * .36f);
                return H(projected.x, projected.y);
            };
            tabby::drawBody(g, path, face, B, physics);
        } else if (isPatternedBuiltIn(skin)) {
            SolidBrush b(col(pal.body));
            g.FillPath(&b, &path);
            if (isOrangeWhite(skin)) {
                GraphicsPath join;
                join.AddBezier(lCheek, H(leftCheekX - 1.5f, cheekCtrlTopY), H(-crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop), crown);
                join.AddBezier(crown, H(crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop), H(rightCheekX + 1.5f, cheekCtrlTopY), rCheek);
                Pen fur(col(pal.earOuter), 1.0f); fur.SetStartCap(LineCapRound); fur.SetEndCap(LineCapRound);
                g.DrawPath(&fur, &join);
            }
            GraphicsState cs = g.Save();
            g.SetClip(&path, CombineModeIntersect);
            drawCoatMarks(g, seatX, seatY, headCenter, physics, isShorthair(skin) ? CatCoatTheme::Shorthair : CatCoatTheme::OrangeWhite);
            g.Restore(cs);
        } else {
            bool hasRegions = false, hasMarks = false;
            for (const SkinRegion& r : skin.regions) hasRegions |= r.space == SkinSpace::Head || r.space == SkinSpace::Skull || r.space == SkinSpace::Body;
            for (const SkinMark& m : skin.marks) hasMarks |= m.space == SkinSpace::Head || m.space == SkinSpace::Skull || m.space == SkinSpace::Body;
            auto headMap = [&](SkinPoint p) { return headPoint(p, false, skin, headCenter, physics); };
            auto skullMap = [&](SkinPoint p) { return headPoint(p, true, skin, headCenter, physics); };
            auto bodyMap = [&](SkinPoint p) { return bodyPoint(p, skin, seatX, seatY, physics); };

            if (!pal.bodyShade.set) {
                SolidBrush b(col(pal.body));
                g.FillPath(&b, &path);
            }
            if (pal.bodyShade.set || skin.colorpoint() || hasRegions || hasMarks) {
                GraphicsState cs = g.Save();
                g.SetClip(&path, CombineModeIntersect);

                if (pal.bodyShade.set) {
                    Color top = col(pal.body), bottom = col(pal.bodyShade);
                    LinearGradientBrush bodyGrad(PointF(seatX, headCenter.y - 54.0f), PointF(seatX, seatY + 38.0f), top, bottom);
                    bodyGrad.SetWrapMode(WrapModeTileFlipXY);
                    Color bodyCols[4] = { top, top, bottom, bottom };
                    REAL bodyPos[4] = { 0.0f, 0.22f, 0.80f, 1.0f };
                    bodyGrad.SetInterpolationColors(bodyCols, bodyPos, 4);
                    g.FillPath(&bodyGrad, &path);
                }

                // Curve regions: head/skull above the jaw line, body below it.
                for (const SkinRegion& r : skin.regions) {
                    if (r.space != SkinSpace::Head && r.space != SkinSpace::Skull && r.space != SkinSpace::Body) continue;
                    GraphicsState rs = g.Save();
                    GraphicsPath side;
                    jawSide(side, r.space != SkinSpace::Body, skin, headCenter, physics);
                    g.SetClip(&side, CombineModeIntersect);
                    if (r.space == SkinSpace::Body) drawRegion(g, r, bodyMap);
                    else if (r.space == SkinSpace::Skull) drawRegion(g, r, skullMap);
                    else drawRegion(g, r, headMap);
                    g.Restore(rs);
                }

                if (skin.colorpoint()) {
                    // 2.5D Rotating Seal-Point "挖煤" Face Mask!
                    const SkinColor& point = pal.points;
                    const SkinColor& mid = pal.pointsMid;
                    float yawRad = yaw * 0.50f;
                    float pitchRad = pitch * 0.36f;
                    Pt maskC = projectHead3D(0.0f, 3.0f, 27.0f, yawRad, pitchRad);
                    float maskSX = std::max(0.76f, std::cos(yawRad * 0.7f)) * 1.24f;
                    float maskSY = std::max(0.82f, std::cos(pitchRad * 0.7f)) * 0.88f;

                    GraphicsState ms = g.Save();
                    g.TranslateTransform(headCenter.x, headCenter.y);
                    g.RotateTransform(hTilt * RadToDeg);
                    if (hs != 1.0f) g.ScaleTransform(hs, hs);
                    g.TranslateTransform(maskC.x, maskC.y);
                    g.ScaleTransform(maskSX, maskSY);

                    GraphicsPath maskEllipse;
                    maskEllipse.AddEllipse(-31.5f, -31.5f, 63.0f, 63.0f);
                    PathGradientBrush maskBrush(&maskEllipse);
                    Color cols[4] = { withAlpha(mid, 0), withAlpha(mid, 158), withAlpha(point, 235), withAlpha(point, 255) };
                    REAL pos[4] = { 0.0f, 0.26f, 0.58f, 1.0f };
                    maskBrush.SetInterpolationColors(cols, pos, 4);
                    g.FillPath(&maskBrush, &maskEllipse);
                    g.Restore(ms);

                    Pt muzC = projectHead3D(0.0f, 7.5f, 28.0f, yawRad, pitchRad);
                    GraphicsState ms2 = g.Save();
                    g.TranslateTransform(headCenter.x, headCenter.y);
                    g.RotateTransform(hTilt * RadToDeg);
                    if (hs != 1.0f) g.ScaleTransform(hs, hs);
                    g.TranslateTransform(muzC.x, muzC.y);
                    g.ScaleTransform(std::max(0.76f, std::cos(yawRad * 0.7f)) * 1.04f, std::max(0.82f, std::cos(pitchRad * 0.7f)) * 0.76f);
                    GraphicsPath muzEllipse;
                    muzEllipse.AddEllipse(-19.5f, -19.5f, 39.0f, 39.0f);
                    PathGradientBrush muzBrush(&muzEllipse);
                    Color mCols[3] = { withAlpha(point, 0), withAlpha(point, 216), withAlpha(point, 255) };
                    REAL mPos[3] = { 0.0f, 0.50f, 1.0f };
                    muzBrush.SetInterpolationColors(mCols, mPos, 3);
                    g.FillPath(&muzBrush, &muzEllipse);
                    g.Restore(ms2);
                }

                for (const SkinMark& m : skin.marks) {
                    if (m.space == SkinSpace::Body) drawMark(g, m, bodyMap);
                    else if (m.space == SkinSpace::Skull) drawMark(g, m, skullMap);
                    else if (m.space == SkinSpace::Head) drawMark(g, m, headMap);
                }
                g.Restore(cs);
            }
            if (pal.outline.set) {
                // Ultra-soft plush rim (the Siamese cream body)
                Pen outPen(col(pal.outline), 1.0f);
                g.DrawPath(&outPen, &path);
            }
        }

        // Tucked hind paws at bottom
        GraphicsState ps = g.Save();
        g.TranslateTransform(seatX, seatY);
        g.RotateTransform(bTilt * RadToDeg);
        g.ScaleTransform(sx, sy);

        SolidBrush pawBrush(col(pal.paws));
        g.FillEllipse(&pawBrush, leftHindX, leftHindY, hindW, leftHindH);
        if (rightTucked) {
            g.FillEllipse(&pawBrush, rightHindX, rightHindY, hindW, rightHindH);
        }

        Pen rimPen(skin.colorpoint() ? withAlpha(pal.body, 140) : (isClassicBlack(skin) ? blackOutline(0) : creaseWith(pal.crease, 148)), 1.3f);
        rimPen.SetStartCap(LineCapRound); rimPen.SetEndCap(LineCapRound);
        g.DrawArc(&rimPen, leftHindX + hindW * 0.5f - 7.5f, leftHindY + leftHindH * 0.5f - 7.5f, 15.0f, 15.0f, 15.0f, 150.0f);
        if (rightTucked) {
            g.DrawArc(&rimPen, rightHindX + hindW * 0.5f - 7.5f, rightHindY + rightHindH * 0.5f - 7.5f, 15.0f, 15.0f, 15.0f, 150.0f);
        }
        if (pal.pawPads.set) {
            SolidBrush pad(col(pal.pawPads));
            for (int side = 0; side < (rightTucked ? 2 : 1); ++side) {
                float cx = (side ? rightHindX : leftHindX) + hindW * 0.5f;
                float cy = side ? rightHindY + rightHindH * 0.5f : leftHindY + leftHindH * 0.5f;
                g.FillEllipse(&pad, cx - 2.7f, cy + 1.6f - 1.9f, 5.4f, 3.8f);
                for (float tx : {-3.4f, 0.0f, 3.4f}) {
                    float ty = cy - 1.7f - (tx == 0.0f ? 0.8f : 0.0f);
                    g.FillEllipse(&pad, cx + tx - 1.25f, ty - 1.1f, 2.5f, 2.2f);
                }
            }
        }
        g.Restore(ps);
    }

    static void drawDazedPaw(Gdiplus::Graphics& g, Gdiplus::RectF bounds) {
        using namespace Gdiplus;
        LinearGradientBrush fur(bounds, rgb(0xFFF1CD), rgb(0xCCAC7F), LinearGradientModeVertical);
        fur.SetWrapMode(WrapModeTileFlipXY);
        g.FillEllipse(&fur, bounds);
        Pen edge(rgb(0x7E634A, 170), 0.9f);
        g.DrawEllipse(&edge, bounds);
    }

    static void drawDazedSword(Gdiplus::Graphics& g, float seatX, float seatY, const CatPhysics& physics) {
        using namespace Gdiplus;
        GraphicsState state = g.Save();
        g.TranslateTransform(seatX, seatY + (1.0f - physics.bodySquash.value) * 36.0f);
        g.RotateTransform(physics.bodyTilt.value * RadToDeg);

        GraphicsPath blade;
        blade.AddBezier(38.0f, -96.0f, 7.0f, -61.0f, -31.0f, -33.0f, -65.0f, -38.0f);
        blade.AddBezier(-65.0f, -38.0f, -62.0f, -24.0f, -51.0f, -21.0f, -37.0f, -25.0f);
        blade.AddBezier(-37.0f, -25.0f, -5.0f, -33.0f, 27.0f, -60.0f, 50.0f, -84.0f);
        blade.CloseFigure();
        LinearGradientBrush steel(PointF(-46.0f, -51.0f), PointF(-29.0f, -22.0f), rgb(0x8395B0), rgb(0xDDE7F2));
        steel.SetWrapMode(WrapModeTileFlipXY);
        Pen outline(rgb(0x232638), 3.6f);
        outline.SetLineJoin(LineJoinRound);
        g.FillPath(&steel, &blade);
        g.DrawPath(&outline, &blade);
        Pen bevel(rgb(0xF3F7FB), 2.1f);
        bevel.SetStartCap(LineCapRound);
        g.DrawBezier(&bevel, -59.0f, -33.5f, -34.0f, -24.0f, 12.0f, -53.0f, 43.0f, -87.0f);

        g.TranslateTransform(44.0f, -90.0f);
        g.RotateTransform(44.0f);
        GraphicsPath grip;
        addRoundedRect(grip, -7.5f, -32.0f, 15.0f, 32.0f, 3.6f);
        SolidBrush wrap(rgb(0x58617D));
        g.FillPath(&wrap, &grip);
        GraphicsPath bindings;
        SolidBrush tape(rgb(0xB5BED0));
        for (float y : {-27.0f, -19.0f, -11.0f}) {
            addQuadPoly(bindings, {-7.5f, y - 2.0f}, {7.5f, y + 1.0f}, {7.5f, y + 4.3f}, {-7.5f, y + 1.3f});
        }
        g.FillPath(&tape, &bindings);
        g.DrawPath(&outline, &grip);

        GraphicsPath guard;
        addRoundedRect(guard, -13.5f, -3.8f, 27.0f, 7.6f, 2.5f);
        SolidBrush brass(rgb(0xE2C58B));
        g.FillPath(&brass, &guard);
        g.DrawPath(&outline, &guard);
        g.Restore(state);
    }

    static void drawDazedSilhouette(Gdiplus::Graphics& g, float seatX, float seatY, Pt head, const CatPhysics& physics) {
        using namespace Gdiplus;
        GraphicsState body = g.Save();
        g.TranslateTransform(seatX, seatY);
        g.RotateTransform(physics.bodyTilt.value * RadToDeg);
        float squash = physics.bodySquash.value;
        g.ScaleTransform(1.0f + (1.0f - squash) * 0.55f, squash);

        drawDazedPaw(g, RectF(-34.0f, -2.0f, 19.0f, 18.0f));
        if (physics.hindFootActive.value < 0.35f) {
            drawDazedPaw(g, RectF(15.0f, -2.0f, 19.0f, 18.0f));
        }

        GraphicsPath belly;
        belly.AddBezier(-42.0f, -19.0f, -46.0f, -5.0f, -41.0f, 11.5f, 0.0f, 11.5f);
        belly.AddBezier(0.0f, 11.5f, 41.0f, 11.5f, 46.0f, -5.0f, 42.0f, -19.0f);
        belly.CloseFigure();
        LinearGradientBrush bellyFur(PointF(0.0f, -16.0f), PointF(0.0f, 12.0f), rgb(0xF4DFB8), rgb(0xC8A77A));
        bellyFur.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&bellyFur, &belly);

        GraphicsPath tunic;
        tunic.AddBezier(-21.0f, -65.0f, -34.0f, -56.0f, -44.0f, -35.0f, -44.0f, -17.0f);
        tunic.AddBezier(-44.0f, -17.0f, -37.0f, -2.0f, 37.0f, -2.0f, 44.0f, -17.0f);
        tunic.AddBezier(44.0f, -17.0f, 44.0f, -35.0f, 34.0f, -56.0f, 21.0f, -65.0f);
        tunic.CloseFigure();
        LinearGradientBrush cloth(PointF(-16.0f, -65.0f), PointF(12.0f, -5.0f), rgb(0x555F9B), rgb(0x252E58));
        cloth.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&cloth, &tunic);
        Pen hem(rgb(0x7C86BC, 155), 1.1f);
        g.DrawBezier(&hem, -39.0f, -15.0f, -28.0f, -6.0f, 28.0f, -6.0f, 39.0f, -15.0f);
        g.Restore(body);

        GraphicsState hoodState = g.Save();
        g.TranslateTransform(head.x, head.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);

        GraphicsPath hood;
        hood.AddBezier(0.0f, -33.0f, 28.0f, -35.0f, 48.0f, -16.0f, 48.0f, 9.0f);
        hood.AddBezier(48.0f, 9.0f, 48.0f, 32.0f, 25.0f, 41.0f, 0.0f, 41.0f);
        hood.AddBezier(0.0f, 41.0f, -25.0f, 41.0f, -48.0f, 32.0f, -48.0f, 9.0f);
        hood.AddBezier(-48.0f, 9.0f, -48.0f, -16.0f, -28.0f, -35.0f, 0.0f, -33.0f);
        hood.CloseFigure();
        LinearGradientBrush fabric(PointF(-16.0f, -35.0f), PointF(15.0f, 42.0f), rgb(0x6871B4), rgb(0x2F386D));
        fabric.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&fabric, &hood);
        g.Restore(hoodState);
    }

    // MARK: - 3rd Key Hind Foot

    static void drawHindFoot(Gdiplus::Graphics& g, float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin) {
        Pt hip = { seatX + 26.0f * skin.shape.bodyWidth, seatY + 2.0f };
        Pt target = physics.hindFoot.point();
        Pt mid = { (hip.x + target.x) * 0.5f + 10.0f, (hip.y + target.y) * 0.5f - 8.0f };
        drawLimb(g, hip, mid, target, 15.0f, 12.0f, true, 0.85f * physics.hindFootActive.value, skin);
    }

    // MARK: - 2.5D Eyes & Whiskers

    static void drawHeadFeatures(Gdiplus::Graphics& g, Pt center, const CatPhysics& physics, EyeColorTheme eyeTheme, const SkinSpec& skin) {
        using namespace Gdiplus;
        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y);
        g.RotateTransform(physics.headTilt.value * RadToDeg);
        if (skin.shape.headScale != 1.0f) g.ScaleTransform(skin.shape.headScale, skin.shape.headScale);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;

        if (isDazed(skin)) {
            drawDazedFace(g, physics, eyeTheme, yawRad, pitchRad, skin);
            g.Restore(st);
            return;
        }

        drawEye3D(g, true, yawRad, pitchRad, physics, eyeTheme, skin);
        drawEye3D(g, false, yawRad, pitchRad, physics, eyeTheme, skin);
        if (skin.palette.nose.set) {
            Pt nose = projectHead3D(0.0f, 8.2f, 29.0f, yawRad, pitchRad);
            GraphicsPath np;
            addQuadBezier(np, { nose.x - 2.3f, nose.y - 1.1f }, { nose.x, nose.y - 1.6f }, { nose.x + 2.3f, nose.y - 1.1f });
            addQuadBezier(np, { nose.x + 2.3f, nose.y - 1.1f }, { nose.x + 1.8f, nose.y + 0.6f }, { nose.x, nose.y + 1.5f });
            addQuadBezier(np, { nose.x, nose.y + 1.5f }, { nose.x - 1.8f, nose.y + 0.6f }, { nose.x - 2.3f, nose.y - 1.1f });
            np.CloseFigure();
            SolidBrush nb(col(skin.palette.nose));
            g.FillPath(&nb, &np);
        }
        if (skin.palette.mouth.set && physics.yawnAmount < 0.15f) {
            Pt mL   = projectHead3D(-4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            Pt mM   = projectHead3D(0.0f, 11.2f, 28.8f, yawRad, pitchRad);
            Pt mR   = projectHead3D(4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            GraphicsPath mp;
            addQuadBezier(mp, mL, { (mL.x + mM.x) * 0.5f, mL.y + 1.6f }, mM);
            addQuadBezier(mp, mM, { (mM.x + mR.x) * 0.5f, mR.y + 1.6f }, mR);
            Pen mpen(col(skin.palette.mouth), 1.05f);
            mpen.SetStartCap(LineCapRound); mpen.SetEndCap(LineCapRound);
            g.DrawPath(&mpen, &mp);
        }
        if (isPatternedBuiltIn(skin)) drawPatternedMuzzle(g, yawRad, pitchRad);
        if (physics.yawnAmount > 0.03f) {
            float yawn = std::clamp(physics.yawnAmount, 0.0f, 1.0f);
            Pt mouthCenter = projectHead3D(0.0f, 16.2f + yawn * 2.2f, 30.2f, yawRad, pitchRad);
            drawYawnMouth(g, mouthCenter, yawn, skin);
        }
        drawWhiskers3D(g, yawRad, pitchRad, physics, skin);

        float anger = std::clamp(physics.angerIntensity.value, 0.0f, 1.0f);
        if (anger > 0.25f) {
            float aAlpha = std::min(1.0f, (anger - 0.25f) / 0.35f);
            // Sweat drop slides down the head and fades out, then restarts
            float phase = std::fmod(physics.time * 0.9f, 1.0f);
            float fade = std::min(1.0f, (1.0f - phase) / 0.3f);
            float a = aAlpha * fade;
            Pt pt = projectHead3D(28.0f, -22.0f, 24.0f, yawRad, pitchRad);
            GraphicsState as = g.Save();
            g.TranslateTransform(pt.x, pt.y + phase * 6.0f);
            GraphicsPath drop;
            drop.AddBezier(0.0f, -6.5f, 0.8f, -3.5f, 3.6f, -1.0f, 3.6f, 2.0f);
            drop.AddBezier(3.6f, 2.0f, 3.6f, 5.8f, -3.6f, 5.8f, -3.6f, 2.0f);
            drop.AddBezier(-3.6f, 2.0f, -3.6f, -1.0f, -0.8f, -3.5f, 0.0f, -6.5f);
            drop.CloseFigure();
            SolidBrush dropFill(Color((BYTE)(235 * a), 133, 204, 255));
            g.FillPath(&dropFill, &drop);
            Pen dropEdge(Color((BYTE)(242 * a), 61, 138, 224), 1.2f);
            g.DrawPath(&dropEdge, &drop);
            SolidBrush shine(Color((BYTE)(217 * a), 255, 255, 255));
            g.FillEllipse(&shine, -2.2f, 0.0f, 1.6f, 2.6f);
            g.Restore(as);
        }
        g.Restore(st);
    }

    static void drawDazedFace(Gdiplus::Graphics& g, const CatPhysics& physics, EyeColorTheme eyeTheme, float yawRad, float pitchRad, const SkinSpec& skin) {
        using namespace Gdiplus;
        Pt face = projectHead3D(0.0f, 4.0f, 22.0f, yawRad * 0.72f, pitchRad * 0.78f);
        g.TranslateTransform(face.x, face.y);
        g.ScaleTransform(std::cos(yawRad), std::cos(pitchRad));

        GraphicsPath opening;
        opening.AddBezier(0.0f, -22.0f, 17.0f, -23.0f, 32.0f, -9.0f, 32.0f, 8.0f);
        opening.AddBezier(32.0f, 8.0f, 32.0f, 22.0f, 17.0f, 27.5f, 0.0f, 27.5f);
        opening.AddBezier(0.0f, 27.5f, -17.0f, 27.5f, -32.0f, 22.0f, -32.0f, 8.0f);
        opening.AddBezier(-32.0f, 8.0f, -32.0f, -9.0f, -17.0f, -23.0f, 0.0f, -22.0f);
        opening.CloseFigure();
        LinearGradientBrush fur(PointF(-7.0f, -23.0f), PointF(8.0f, 29.0f), rgb(0xFFF4D4), rgb(0xDDBF91));
        fur.SetWrapMode(WrapModeTileFlipXY);
        g.FillPath(&fur, &opening);
        Pen binding(rgb(0x34355D), 1.9f);
        g.DrawPath(&binding, &opening);

        GraphicsPath colorPoint;
        colorPoint.AddEllipse(-21.0f, -8.0f, 42.0f, 34.0f);
        PathGradientBrush pointFur(&colorPoint);
        pointFur.SetCenterPoint(PointF(0.0f, 11.0f));
        Color tones[] = {rgb(0x8D5B44, 0), rgb(0x8D5B44, 5), rgb(0x8D5B44, 155), rgb(0x8D5B44, 220)};
        REAL stops[] = {0.0f, 0.17f, 0.38f, 1.0f};
        pointFur.SetInterpolationColors(tones, stops, 4);
        g.FillPath(&pointFur, &colorPoint);

        SolidBrush cheeks(rgb(0xCF8173, 94));
        g.FillEllipse(&cheeks, -23.0f, 10.0f, 9.0f, 4.0f);
        g.FillEllipse(&cheeks, 14.0f, 10.0f, 9.0f, 4.0f);

        Color ink = rgb(0x282333);
        Pen expression(ink, 1.7f);
        expression.SetStartCap(LineCapRound);
        expression.SetEndCap(LineCapRound);
        expression.SetLineJoin(LineJoinRound);
        if (physics.yawnAmount > 0.03f) {
            float yawn = std::clamp(physics.yawnAmount, 0.0f, 1.0f);
            GraphicsState mouthState = g.Save();
            // Keep the opening inside the hood and in the same plane as the rest of the face.
            g.TranslateTransform(0.0f, 17.5f + yawn * 2.0f);
            g.ScaleTransform(1.0f, 0.75f);
            drawYawnMouth(g, {0.0f, 0.0f}, yawn, skin);
            g.Restore(mouthState);
        } else {
            GraphicsPath mouth;
            mouth.AddBezier(-6.5f, 12.0f, -5.9f, 17.0f, -1.8f, 17.5f, 0.0f, 13.4f);
            mouth.AddBezier(0.0f, 13.4f, 1.8f, 17.5f, 5.9f, 17.0f, 6.5f, 12.0f);
            g.DrawPath(&expression, &mouth);
        }
        SolidBrush nose(ink);
        g.FillEllipse(&nose, -1.9f, 10.1f, 3.8f, 2.8f);

        bool happy = physics.isHappyClosedEyes();
        bool squint = physics.isBlissfulSquint();
        if (happy || squint) {
            for (float x : {-11.8f, 11.8f}) {
                GraphicsPath lid;
                addQuadBezier(lid, {x - 7.2f, 2.0f}, {x, happy ? 7.0f : 4.0f}, {x + 7.2f, 2.0f});
                g.DrawPath(&expression, &lid);
            }
            return;
        }

        float open = 1.0f - std::clamp(physics.effectiveEyeClosure(), 0.0f, 1.0f);
        if (open <= 0.025f) return;
        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        Pt gaze = physics.eyeGaze.point();
        float gazeX = std::clamp(gaze.x, -1.0f, 1.0f) * (1.35f - hunt * 0.65f);
        float gazeY = std::clamp(gaze.y, -1.0f, 1.0f) * (1.1f - hunt * 0.55f);
        float radius = 8.5f + hunt * 1.0f;
        float pupilRadius = 6.25f + hunt * 1.35f;
        SolidBrush eyeRim(ink);
        SolidBrush iris(blendColor(rgb(0xFFF8E5), eyePrimary(eyeTheme), 0.20f));
        SolidBrush pupil(rgb(0x191825));
        SolidBrush glint(rgb(0xFFF9EA));
        for (float x : {-11.8f, 11.8f}) {
            GraphicsState eye = g.Save();
            g.TranslateTransform(x, 1.0f);
            g.ScaleTransform(1.0f, open);
            g.FillEllipse(&eyeRim, -radius, -radius, 2.0f * radius, 2.0f * radius);
            float irisRadius = radius - 1.25f;
            g.FillEllipse(&iris, -irisRadius, -irisRadius, 2.0f * irisRadius, 2.0f * irisRadius);
            g.FillEllipse(&pupil, gazeX - pupilRadius, gazeY - pupilRadius, 2.0f * pupilRadius, 2.0f * pupilRadius);
            if (hunt > 0.25f) {
                float shine = 1.1f + hunt * 0.6f;
                g.FillEllipse(&glint, gazeX - 3.4f, gazeY - 4.2f, 2.0f * shine, 2.0f * shine);
            }
            g.Restore(eye);
        }
    }

    static void drawYawnMouth(Gdiplus::Graphics& g, Pt center, float y, const SkinSpec& skin) {
        using namespace Gdiplus;
        float rx = 3.2f + y * 5.8f;
        float ry = 1.6f + y * 7.4f;

        GraphicsState st = g.Save();
        g.TranslateTransform(center.x, center.y);

        GraphicsPath mouth;
        mouth.AddBezier(0.0f, -ry * 0.92f, rx * 0.68f, -ry * 0.92f, rx * 1.05f, -ry * 0.42f, rx, ry * 0.08f);
        mouth.AddBezier(rx, ry * 0.08f, rx * 0.95f, ry * 0.72f, rx * 0.50f, ry, 0.0f, ry);
        mouth.AddBezier(0.0f, ry, -rx * 0.50f, ry, -rx * 0.95f, ry * 0.72f, -rx, ry * 0.08f);
        mouth.AddBezier(-rx, ry * 0.08f, -rx * 1.05f, -ry * 0.42f, -rx * 0.68f, -ry * 0.92f, 0.0f, -ry * 0.92f);
        mouth.CloseFigure();

        SolidBrush bgBrush(Color(250, 199, 48, 64));
        g.FillPath(&bgBrush, &mouth);

        GraphicsState cs = g.Save();
        g.SetClip(&mouth, CombineModeIntersect);

        SolidBrush shadowBrush(Color(140, 140, 26, 41));
        g.FillEllipse(&shadowBrush, -rx * 0.65f, -ry * 0.72f, rx * 1.30f, ry * 0.92f);

        if (y > 0.22f) {
            float tAlpha = std::min(1.0f, (y - 0.22f) / 0.35f);
            SolidBrush tongueBrush(Color((BYTE)(235 * tAlpha), 242, 122, 140));
            GraphicsPath tongue;
            addQuadBezier(tongue, { -rx * 0.62f, ry * 0.55f }, { -rx * 0.32f, ry * 0.16f }, { 0.0f, ry * 0.36f });
            addQuadBezier(tongue, { 0.0f, ry * 0.36f }, { rx * 0.32f, ry * 0.16f }, { rx * 0.62f, ry * 0.55f });
            addQuadBezier(tongue, { rx * 0.62f, ry * 0.55f }, { 0.0f, ry * 1.25f }, { -rx * 0.62f, ry * 0.55f });
            tongue.CloseFigure();
            g.FillPath(&tongueBrush, &tongue);
        }

        if (y > 0.28f) {
            float fScale = std::min(1.0f, (y - 0.28f) / 0.32f);
            SolidBrush fangBrush(Color(250, 255, 252, 245));
            float uBaseY = -ry * 0.80f;
            float uLen = (2.6f + y * 2.2f) * fScale;
            float uHW = (1.15f + y * 0.55f) * fScale;
            for (float sign : { -1.0f, 1.0f }) {
                float fx = sign * rx * 0.46f;
                PointF pts[3] = { { fx - uHW, uBaseY - 0.8f }, { fx + uHW, uBaseY - 0.8f }, { fx - sign * 0.25f, uBaseY + uLen } };
                g.FillPolygon(&fangBrush, pts, 3);
            }
            float lBaseY = ry * 0.84f;
            float lLen = (1.6f + y * 1.3f) * fScale;
            float lHW = (0.85f + y * 0.35f) * fScale;
            for (float sign : { -1.0f, 1.0f }) {
                float fx = sign * rx * 0.36f;
                PointF pts[3] = { { fx - lHW, lBaseY + 0.8f }, { fx + lHW, lBaseY + 0.8f }, { fx - sign * 0.15f, lBaseY - lLen } };
                g.FillPolygon(&fangBrush, pts, 3);
            }
        }
        g.Restore(cs);

        bool darkRim = skin.colorpoint() || (!isDazed(skin) && skin.palette.body.luminance() < 0.25);
        Pen rimPen(darkRim ? Color(216, 107, 20, 31) : Color(184, 143, 56, 66), 1.0f);
        g.DrawPath(&rimPen, &mouth);
        g.Restore(st);
    }

    static void drawPatternedMuzzle(Gdiplus::Graphics& g, float yawRad, float pitchRad) {
        using namespace Gdiplus;
        const Pt local[] = {{-2.90f, 8.50f}, {-1.80f, 8.10f}, {1.80f, 8.10f}, {2.90f, 8.50f}, {3.20f, 8.90f}, {0.80f, 12.10f}, {0.00f, 12.30f}, {-0.80f, 12.10f}, {-3.20f, 8.90f}, {-2.90f, 8.50f}, {-5.90f, 14.40f}, {-4.70f, 18.00f}, {-1.40f, 18.00f}, {0.00f, 14.30f}, {1.40f, 18.00f}, {4.70f, 18.00f}, {5.90f, 14.40f}, {0.00f, 12.00f}};
        Pt p[18];
        for (int i=0;i<18;++i) p[i]=projectHead3D(local[i].x,local[i].y,30.0f,yawRad,pitchRad);
        GraphicsPath nose;
        nose.AddBezier(p[0].x,p[0].y,p[1].x,p[1].y,p[2].x,p[2].y,p[3].x,p[3].y);
        nose.AddBezier(p[3].x,p[3].y,p[4].x,p[4].y,p[5].x,p[5].y,p[6].x,p[6].y);
        nose.AddBezier(p[6].x,p[6].y,p[7].x,p[7].y,p[8].x,p[8].y,p[9].x,p[9].y);
        nose.CloseFigure(); SolidBrush ink(Color(255,217,143,155)); g.FillPath(&ink,&nose);
        GraphicsPath mouth;
        mouth.AddLine(p[17].x,p[17].y,p[13].x,p[13].y); mouth.StartFigure();
        mouth.AddBezier(p[10].x,p[10].y,p[11].x,p[11].y,p[12].x,p[12].y,p[13].x,p[13].y);
        mouth.AddBezier(p[13].x,p[13].y,p[14].x,p[14].y,p[15].x,p[15].y,p[16].x,p[16].y);
        Pen pen(Color(199,139,140,134),1.15f); pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound); pen.SetLineJoin(LineJoinRound); g.DrawPath(&pen,&mouth);
    }

    static void drawEye3D(Gdiplus::Graphics& g, bool isLeft, float yawRad, float pitchRad, const CatPhysics& physics, EyeColorTheme eyeTheme, const SkinSpec& skin) {
        using namespace Gdiplus;
        float sign = isLeft ? -1.0f : 1.0f;
        bool customIris = skin.palette.iris.set; // a skin's iris overrides the eye-colour menu
        Color eyeCol = customIris ? col(skin.palette.iris) : eyePrimary(eyeTheme);
        Pt proj = projectHead3D(sign * 17.2f, -2.0f, 27.5f, yawRad, pitchRad);

        float eyeNormal = yawRad + sign * 0.34f;
        float foreX = std::clamp(std::cos(eyeNormal) / std::cos(0.34f), 0.64f, 1.06f);
        float foreY = std::clamp(std::cos(pitchRad * 0.85f), 0.78f, 1.03f);

        GraphicsState st = g.Save();
        g.TranslateTransform(proj.x, proj.y);
        g.ScaleTransform(foreX, foreY);

        // Mode 1: Happy closed glowing curved line eyes (petting head!)
        // Mode 0: Dizzy spinning spiral eyes (@ @) after 10s of rapid mouse shaking
        float dizzy = std::clamp(physics.dizzyIntensity.value, 0.0f, 1.0f);
        if (dizzy > 0.28f) {
            SolidBrush bg(Color(242, 250, 250, 245));
            g.FillEllipse(&bg, -11.5f, -11.0f, 23.0f, 22.0f);
            Pen sp(Color(242, 36, 36, 46), 1.85f);
            sp.SetStartCap(LineCapRound); sp.SetEndCap(LineCapRound);
            PointF pts[43];
            float spin = physics.dizzyPhase * (isLeft ? 1.0f : -1.0f);
            for (int i = 0; i <= 42; ++i) {
                float t = (float)i / 42.0f;
                float theta = spin + t * 3.1415926f * 5.2f;
                float rad = 1.0f + t * 8.6f;
                pts[i] = PointF(std::cos(theta) * rad, std::sin(theta) * rad);
            }
            g.DrawLines(&sp, pts, 43);
            g.Restore(st);
            return;
        }

        if (physics.isHappyClosedEyes()) {
            GraphicsPath hp;
            addQuadBezier(hp, {-11.5f, 1.0f}, {sign * 1.5f, 3.4f}, {11.5f, 1.0f});
            Pen p(isTabby(skin) ? tabby::dark() : eyeCol, 2.6f);
            p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);
            g.DrawPath(&p, &hp);
            g.Restore(st);
            return;
        }

        // Mode 2: Blissful half-squint glowing slits (petting body!)
        if (physics.isBlissfulSquint()) {
            GraphicsPath sp;
            addQuadBezier(sp, {-11.5f, -0.5f}, {0.0f, 0.6f}, {11.5f, -0.5f});
            addQuadBezier(sp, {11.5f, -0.5f}, {0.0f, 4.8f}, {-11.5f, -0.5f});
            sp.CloseFigure();
            SolidBrush sb(isTabby(skin) ? tabby::dark() : eyeCol);
            g.FillPath(&sb, &sp);
            g.Restore(st);
            return;
        }

        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        g.RotateTransform(sign * (0.06f * (1.0f - hunt * 0.6f)) * RadToDeg);

        bool siameseBlue = !customIris && eyeTheme == EyeColorTheme::SiameseBlue;
        bool isSiameseEye = buildOf(skin).sleekEyes && siameseBlue;
        float rx = isSiameseEye ? (11.4f + hunt * 1.3f) : (12.0f + hunt * 1.1f);
        float ry = isSiameseEye ? (9.8f + hunt * 2.0f) : (10.8f + hunt * 1.8f);

        GraphicsPath eyeClip;
        eyeClip.AddEllipse(-rx, -ry, rx * 2.0f, ry * 2.0f);
        GraphicsState cs = g.Save();
        g.SetClip(&eyeClip, CombineModeIntersect);
        // Leave covered pixels untouched so the underlying face remains seamless.
        float closure = std::clamp(physics.effectiveEyeClosure(), 0.0f, 1.0f);
        if (closure >= 1.0f) {
            g.Restore(cs);
            g.Restore(st);
            return;
        }
        RectF opening(-rx - 2.0f, -ry + 2.0f * ry * closure,
                      2.0f * rx + 4.0f, 2.0f * ry * (1.0f - closure));
        if (closure > 0.0f) g.SetClip(opening, CombineModeIntersect);

        SolidBrush irisBrush(eyeCol);
        g.FillRectangle(&irisBrush, -rx - 1.0f, -ry - 1.0f, rx * 2.0f + 2.0f, ry * 2.0f + 2.0f);
        if (siameseBlue) {
            LinearGradientBrush irisGrad(PointF(0.0f, -ry - 1.0f), PointF(0.0f, ry + 1.0f), Color(255, 143, 196, 230), Color(255, 219, 241, 252));
            irisGrad.SetWrapMode(WrapModeTileFlipXY);
            Color iCols[3] = { Color(255, 143, 196, 230), Color(255, 181, 222, 245), Color(255, 219, 241, 252) };
            REAL iPos[3] = { 0.0f, 0.55f, 1.0f };
            irisGrad.SetInterpolationColors(iCols, iPos, 3);
            g.FillRectangle(&irisGrad, -rx - 1.0f, -ry - 1.0f, rx * 2.0f + 2.0f, ry * 2.0f + 2.0f);
        }

        if (!customIris && eyeTheme == EyeColorTheme::OliveGold) {
            LinearGradientBrush iris(PointF(0, -ry - 1), PointF(0, ry + 1),
                                     blendColor(eyeCol, tabby::brown(), .39f),
                                     blendColor(eyeCol, Color(250, 237, 189), .31f));
            g.FillRectangle(&iris, -rx - 1.f, -ry - 1.f, 2 * rx + 2, 2 * ry + 2);
            Pen eyeRim(tabby::dark(), 1.5f);
            g.DrawEllipse(&eyeRim, -rx + .4f, -ry + .4f, 2 * rx - .8f, 2 * ry - .8f);
        }

        // Pupil morphs smoothly from vertical slit -> HUGE round black boba pupil!
        Pt gaze = physics.eyeGaze.point();
        float px = gaze.x * (4.2f - hunt * 1.2f);
        float py = gaze.y * (3.6f - hunt * 1.0f);
        float pScale = physics.pupilScale.value;
        float normalW = 5.0f * pScale;
        float normalH = 13.5f * std::min(1.12f, pScale);
        float huntDiam = 18.8f;
        float pupilW = normalW + (huntDiam - normalW) * hunt;
        float pupilH = normalH + (huntDiam - normalH) * hunt;

        // Reference iris colors, blended only in a narrow ring around the moving pupil.
        if (!customIris && eyeTheme == EyeColorTheme::ReferenceHazel) {
            for (int i = 0; i <= 24; ++i) {
                float t = i / 24.0f, mix = t*t*(3-2*t), rim = (1-t)*2.2f;
                SolidBrush ring(Color(255, (BYTE)std::lround(218-77*mix),
                    (BYTE)std::lround(178-33*mix), (BYTE)std::lround(87+41*mix)));
                g.FillEllipse(&ring, px-pupilW*0.5f-rim, py-pupilH*0.5f-rim,
                    pupilW+2*rim, pupilH+2*rim);
            }
        }
        SolidBrush pupilBrush(catBlack());
        g.FillEllipse(&pupilBrush, px - pupilW * 0.5f, py - pupilH * 0.5f, pupilW, pupilH);

        // Tiny crisp white sparkle glint as boba hunting pupils dilate (and delicate catchlight on Siamese blue eyes!)
        if (hunt > 0.25f) {
            BYTE ga = (BYTE)(std::min(1.0f, (hunt - 0.25f) / 0.55f) * 235.0f);
            float gr = 2.1f + hunt * 0.9f;
            SolidBrush gb(Color(ga, 255, 255, 255));
            g.FillEllipse(&gb, px - pupilW * 0.22f - gr * 0.5f, py - pupilH * 0.22f - gr * 0.5f, gr * 2.0f, gr * 2.0f);
        } else if (siameseBlue) {
            float gr = 1.65f;
            SolidBrush gb(Color(220, 255, 255, 255));
            g.FillEllipse(&gb, px - 2.9f - gr, py - 3.6f - gr, gr * 2.0f, gr * 2.0f);
        }

        g.Restore(cs);

        // Angry furrowed V-brow slant over eyes when typing super fast
        float anger = std::clamp(physics.angerIntensity.value, 0.0f, 1.0f);
        if (anger > 0.08f) {
            float drop = anger * 8.2f;
            PointF browPts[4] = {
                PointF(-rx - 2.0f, -ry - 2.0f),
                PointF( rx + 2.0f, -ry - 2.0f),
                PointF( rx + 2.0f, -ry + (isLeft ? drop : 1.0f)),
                PointF(-rx - 2.0f, -ry + (isLeft ? 1.0f : drop))
            };
            Color browCol = skin.colorpoint() ? col(skin.palette.points)
                          : (isDazed(skin) ? rgb(0x4E5792)
                          : (isTabby(skin) ? tabby::brown() : col(skin.palette.body)));
            SolidBrush browBrush(browCol);
            g.FillPolygon(&browBrush, browPts, 4);
        }

        g.Restore(st);
    }

    static void drawWhiskers3D(Gdiplus::Graphics& g, float yawRad, float pitchRad, const CatPhysics& physics, const SkinSpec& skin) {
        using namespace Gdiplus;
        float wiggle = std::sin(physics.time * 4.5f) * (std::max(physics.petHeadIntensity, physics.petBodyIntensity) * 1.2f);
        Pen pen(col(skin.palette.whiskers), 1.05f);
        pen.SetStartCap(LineCapRound); pen.SetEndCap(LineCapRound);

        const float rows[3][3] = {
            { 10.5f, 36.5f, 8.8f + wiggle },
            { 14.2f, 37.8f, 14.2f + wiggle * 0.5f },
            { 18.0f, 35.5f, 19.6f - wiggle * 0.4f }
        };

        for (float sign : {-1.0f, 1.0f}) {
            for (int i = 0; i < 3; ++i) {
                Pt p0 = projectHead3D(sign * 24.0f, rows[i][0], 23.0f, yawRad, pitchRad);
                Pt p2 = projectHead3D(sign * rows[i][1], rows[i][2], 14.0f, yawRad, pitchRad);
                Pt ctrl = { (p0.x + p2.x) * 0.5f, (p0.y + p2.y) * 0.5f - 0.8f };
                GraphicsPath wp;
                addQuadBezier(wp, p0, ctrl, p2);
                g.DrawPath(&pen, &wp);
            }
        }
    }

    // MARK: - Decorations (anchored PNG stickers)

    // Minimal read-only IStream over the decoded PNG bytes (avoids linking shlwapi/ole32 just for this).
    class MemoryStream : public IStream {
    public:
        explicit MemoryStream(const std::vector<uint8_t>& bytes) : data_(bytes) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override {
            static const GUID unknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
            static const GUID stream = {0x0000000c, 0x0000, 0x0000, {0xC0, 0, 0, 0, 0, 0, 0, 0x46}};
            static const GUID sequential = {0x0c733a30, 0x2a1c, 0x11ce, {0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d}};
            if (!out) return E_POINTER;
            if (std::memcmp(&riid, &unknown, sizeof(GUID)) == 0 || std::memcmp(&riid, &stream, sizeof(GUID)) == 0 ||
                std::memcmp(&riid, &sequential, sizeof(GUID)) == 0) { *out = this; AddRef(); return S_OK; }
            *out = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }
        ULONG STDMETHODCALLTYPE Release() override { ULONG r = --refs_; if (r == 0) delete this; return r; }
        HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* read) override {
            size_t n = std::min<size_t>(cb, data_.size() - std::min(pos_, data_.size()));
            if (n) std::memcpy(pv, data_.data() + pos_, n);
            pos_ += n;
            if (read) *read = (ULONG)n;
            return n == cb ? S_OK : S_FALSE;
        }
        HRESULT STDMETHODCALLTYPE Write(const void*, ULONG, ULONG*) override { return STG_E_ACCESSDENIED; }
        HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* newPos) override {
            long long base = origin == STREAM_SEEK_SET ? 0 : origin == STREAM_SEEK_CUR ? (long long)pos_ : (long long)data_.size();
            long long p = base + move.QuadPart;
            if (p < 0) return STG_E_INVALIDFUNCTION;
            pos_ = (size_t)p;
            if (newPos) newPos->QuadPart = (ULONGLONG)pos_;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Commit(DWORD) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE Revert() override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Stat(STATSTG* st, DWORD) override {
            if (!st) return E_POINTER;
            std::memset(st, 0, sizeof(STATSTG));
            st->type = STGTY_STREAM;
            st->cbSize.QuadPart = (ULONGLONG)data_.size();
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Clone(IStream**) override { return E_NOTIMPL; }
    private:
        virtual ~MemoryStream() = default;
        const std::vector<uint8_t>& data_;
        size_t pos_ = 0;
        ULONG refs_ = 1;
    };

    static Gdiplus::Bitmap* decorationBitmap(const SkinDecoration& d) {
        using namespace Gdiplus;
        if (!d.nativeImage) {
            Bitmap* copy = nullptr;
            MemoryStream* stream = new MemoryStream(d.png);
            {
                Bitmap decoded(stream);
                if (decoded.GetLastStatus() == Ok && decoded.GetWidth() > 0) {
                    // Copy into a standalone 32bpp bitmap so nothing keeps referencing the stream.
                    copy = new Bitmap((INT)decoded.GetWidth(), (INT)decoded.GetHeight(), PixelFormat32bppPARGB);
                    Graphics cg(copy);
                    cg.DrawImage(&decoded, 0, 0, (INT)decoded.GetWidth(), (INT)decoded.GetHeight());
                }
            }
            stream->Release();
            // An undecodable PNG is remembered as an empty bitmap so it is not retried every frame.
            d.nativeImage = std::shared_ptr<void>(copy ? (void*)copy : (void*)new Bitmap(1, 1, PixelFormat32bppPARGB),
                                                  [](void* p) { delete (Bitmap*)p; });
            if (!copy) return nullptr;
        }
        Bitmap* bmp = (Bitmap*)d.nativeImage.get();
        return bmp->GetWidth() > 1 || d.pixelWidth == 1 ? bmp : nullptr;
    }

    static void drawDecorations(Gdiplus::Graphics& g, float seatX, float seatY, Pt head, const CatPhysics& physics, const SkinSpec& skin) {
        using namespace Gdiplus;
        float yawRad = physics.headYaw.value * 0.50f, pitchRad = physics.headPitch.value * 0.36f;
        for (const SkinDecoration& d : skin.decorations) {
            Bitmap* img = decorationBitmap(d);
            if (!img) continue;
            PointF at;
            float rot = 0, kx = 1, ky = 1;
            switch (d.anchor) {
                case SkinAnchorPoint::HeadTop:
                case SkinAnchorPoint::Face: {
                    float baseY = d.anchor == SkinAnchorPoint::HeadTop ? -30.0f : 4.0f;
                    at = headPoint({d.offsetX, baseY + d.offsetY}, false, skin, head, physics);
                    rot = physics.headTilt.value;
                    kx = std::max(0.55f, std::cos(yawRad)) * skin.shape.headScale;
                    ky = std::max(0.70f, std::cos(pitchRad)) * skin.shape.headScale;
                    break;
                }
                case SkinAnchorPoint::Neck:
                    at = bodyPoint({d.offsetX, -64.0f + d.offsetY}, skin, seatX, seatY, physics);
                    rot = physics.bodyTilt.value;
                    break;
                case SkinAnchorPoint::TailTip: {
                    TailCurve c = tailCurve(seatX, seatY, physics, skin);
                    at = PointF(c.tip.X + d.offsetX, c.tip.Y + d.offsetY);
                    break;
                }
            }
            float scale = 0.5f * d.scale; // stickers are authored at 2x: 1 image px = 0.5 canvas units
            float w = (float)img->GetWidth(), h = (float)img->GetHeight();
            GraphicsState st = g.Save();
            g.TranslateTransform(at.X, at.Y);
            g.RotateTransform(rot * RadToDeg + d.rotation);
            g.ScaleTransform(scale * kx, scale * ky);
            g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            g.DrawImage(img, RectF(-w * 0.5f, -h * 0.5f, w, h));
            g.Restore(st);
        }
    }

    // MARK: - Front Paws

    static void drawFrontPaws(Gdiplus::Graphics& g, float seatX, float seatY, float breath, const CatPhysics& physics, const SkinSpec& skin) {
        Pt lPos = physics.leftPaw.point();
        Pt rPos = physics.rightPaw.point();

        float lDist = std::hypot(lPos.x - CatPhysics::LeftPawRest.x, lPos.y - CatPhysics::LeftPawRest.y);
        float rDist = std::hypot(rPos.x - CatPhysics::RightPawRest.x, rPos.y - CatPhysics::RightPawRest.y);

        const Build& bd = buildOf(skin);
        float bw = skin.shape.bodyWidth;
        float headLift = bd.headLift;
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float idleTopY = (seatY - bd.idleTopDY) + lieS * 24.0f;
        float idleTopX = bd.idleTopX * bw - lieS * 5.0f;
        float idleBotX = bd.idleBotX * bw - lieS * 4.5f;
        float idleBotY = seatY + 12.0f - lieS * 4.0f;
        float shX = bd.shoulderX * bw;
        float limbStartW = bd.limbStartW;
        float limbEndW = bd.limbEndW;

        auto shoulder = [&](float x) -> Pt {
            float sy=physics.bodySquash.value*(1.0f-lieS*0.42f), sx=(1+(1-physics.bodySquash.value)*0.55f)*(1.0f+lieS*0.03f);
            float c=std::cos(physics.bodyTilt.value), s=std::sin(physics.bodyTilt.value);
            float floor=126+headLift+physics.headOffsetY.value+physics.headPitch.value*4.8f+breath*1.1f+42;
            return {seatX+x*sx*c+58*sy*s, std::max(seatY+x*sx*s-58*sy*c,floor)};
        };
        Pt leftShoulder=shoulder(-shX), rightShoulder=shoulder(shX);

        if (lDist < 6.0f && !physics.leftPawOnMouse && !physics.leftPawTyping) {
            if (lieS < 0.78f) drawIdleFrontLeg(g, { seatX - idleTopX, idleTopY }, { seatX - idleBotX, idleBotY }, true, skin);
        } else {
            Pt mid = { (leftShoulder.x + lPos.x) * 0.5f - (physics.leftPawOnMouse ? 8.0f : 4.0f),
                       (leftShoulder.y + lPos.y) * 0.5f - 6.0f };
            drawLimb(g, leftShoulder, mid, lPos, limbStartW, limbEndW, false, 0.78f, skin, true);
        }

        if (rDist < 6.0f && !physics.rightPawTyping) {
            if (lieS < 0.78f) drawIdleFrontLeg(g, { seatX + idleTopX, idleTopY }, { seatX + idleBotX, idleBotY }, false, skin);
        } else {
            Pt mid = { (rightShoulder.x + rPos.x) * 0.5f + 4.0f, (rightShoulder.y + rPos.y) * 0.5f - 6.0f };
            drawLimb(g, rightShoulder, mid, rPos, limbStartW, limbEndW, true, 0.78f, skin, true);
        }
    }

    static void drawIdleFrontLeg(Gdiplus::Graphics& g, Pt top, Pt bottom, bool isLeft, const SkinSpec& skin) {
        using namespace Gdiplus;
        const SkinPalette& pal = skin.palette;
        float sign = isLeft ? -1.0f : 1.0f;
        if (isDazed(skin)) {
            Pt shoulder = {top.x + sign * 4.0f, top.y - 10.0f};
            Pt elbow = {top.x + sign * 11.0f, top.y + 1.0f};
            Pt paw = {top.x + sign * 10.0f, top.y + 15.0f};
            drawLimb(g, shoulder, elbow, paw, 19.0f, 14.0f, !isLeft, 0.78f, skin, true);
            return;
        }

        if (skin.colorpoint()) {
            GraphicsPath lp;
            addRoundedRect(lp, bottom.x - 7.5f, top.y, 15.0f, bottom.y - top.y + 5.0f, 6.5f);
            float pad = 10.0f;
            float span = std::max(1.0f, (bottom.y + 5.0f) - top.y);
            float total = span + pad * 2.0f;
            LinearGradientBrush grad(PointF(bottom.x, top.y - pad), PointF(bottom.x, bottom.y + 5.0f + pad), withAlpha(pal.body, 0), withAlpha(pal.points, 255));
            grad.SetWrapMode(WrapModeTileFlipXY);
            Color cols[5] = { withAlpha(pal.body, 0), withAlpha(pal.body, 0), withAlpha(pal.pointsMid, 190), withAlpha(pal.points, 255), withAlpha(pal.points, 255) };
            REAL pos[5] = { 0.0f, pad / total, (pad + span * 0.50f) / total, (pad + span * 0.82f) / total, 1.0f };
            grad.SetInterpolationColors(cols, pos, 5);
            g.FillPath(&grad, &lp);
        } else {
            // Smooth shoulder-fade leg fill so palette.legs shows cleanly on idle sitting legs too
            // ("idleLegs": "legs"; the built-in coats use "chest" and keep their approved look)
            if (skin.idleLegsTinted) {
                GraphicsPath legShape;
                addQuadBezier(legShape, { top.x + sign * 8.5f, top.y }, { top.x + sign * 9.5f, (top.y + bottom.y) * 0.5f }, { bottom.x + sign * 7.5f, bottom.y - 1.0f });
                addQuadBezier(legShape, { bottom.x + sign * 7.5f, bottom.y - 1.0f }, { bottom.x + sign * 7.0f, bottom.y + 5.2f }, { bottom.x + sign * 0.8f, bottom.y + 5.2f });
                addQuadBezier(legShape, { bottom.x + sign * 0.8f, bottom.y + 5.2f }, { bottom.x - sign * 6.2f, bottom.y + 5.2f }, { bottom.x - sign * 6.0f, bottom.y - 1.0f });
                addQuadBezier(legShape, { bottom.x - sign * 6.0f, bottom.y - 1.0f }, { top.x - sign * 6.5f, (top.y + bottom.y) * 0.5f }, { top.x - sign * 7.0f, top.y + 4.0f });
                legShape.CloseFigure();
                float pad = 10.0f;
                float span = std::max(1.0f, (bottom.y + 5.0f) - top.y);
                float total = span + pad * 2.0f;
                LinearGradientBrush legGrad(PointF(bottom.x, top.y - pad), PointF(bottom.x, bottom.y + 5.0f + pad), withAlpha(pal.legs, 0), col(pal.legs));
                legGrad.SetWrapMode(WrapModeTileFlipXY);
                Color legCols[4] = { withAlpha(pal.legs, 0), withAlpha(pal.legs, 0), col(pal.legs), col(pal.legs) };
                REAL legPos[4] = { 0.0f, (pad + span * 0.04f) / total, (pad + span * 0.32f) / total, 1.0f };
                legGrad.SetInterpolationColors(legCols, legPos, 4);
                g.FillPath(&legGrad, &legShape);
            }

            if (isClassicBlack(skin)) {
                Pen outPen(blackOutline(), BlackOutlineW * 1.8f);
                outPen.SetStartCap(LineCapRound); outPen.SetEndCap(LineCapRound);
                g.DrawArc(&outPen, bottom.x - 7.2f, bottom.y - 8.7f, 14.4f, 14.4f, 10.0f, 160.0f);
            }

            SolidBrush cuff(col(pal.paws));
            g.FillRectangle(&cuff, bottom.x-7.5f, bottom.y-13.0f, 15.0f, 13.0f);
            g.FillEllipse(&cuff, bottom.x-7.5f, bottom.y-5.0f, 15.0f, 10.0f);
        }
        Pen p(isClassicBlack(skin) ? blackOutline(190) : col(pal.crease), 1.45f);
        p.SetStartCap(LineCapRound); p.SetEndCap(LineCapRound);

        GraphicsPath p1;
        addQuadBezier(p1, { top.x + sign * 8.5f, top.y }, { top.x + sign * 9.5f, (top.y + bottom.y) * 0.5f }, { bottom.x + sign * 7.5f, bottom.y - 1.0f });
        g.DrawPath(&p, &p1);

        GraphicsPath p2;
        addQuadBezier(p2, { top.x - sign * 7.0f, top.y + 4.0f }, { top.x - sign * 6.5f, (top.y + bottom.y) * 0.5f }, { bottom.x - sign * 6.0f, bottom.y - 1.0f });
        g.DrawPath(&p, &p2);
    }

    static void drawDazedArm(Gdiplus::Graphics& g, Gdiplus::GraphicsPath& arm, const std::vector<Gdiplus::PointF>& left,
        const std::vector<Gdiplus::PointF>& right, Gdiplus::RectF paw) {
        using namespace Gdiplus;
        SolidBrush cloth(rgb(0x465189));
        g.FillPath(&cloth, &arm);

        constexpr float sleeveLength = 15.0f;
        float distance = 0.0f;
        for (size_t i = 1; i < left.size(); ++i) {
            float dx = (left[i].X + right[i].X - left[i - 1].X - right[i - 1].X) * 0.5f;
            float dy = (left[i].Y + right[i].Y - left[i - 1].Y - right[i - 1].Y) * 0.5f;
            float segment = std::hypot(dx, dy);
            // End the cuff at the same distance from the shoulder as the arm reaches.
            if (distance + segment < sleeveLength && i < left.size() - 1) {
                distance += segment;
                continue;
            }

            float t = std::clamp((sleeveLength - distance) / std::max(segment, 0.001f), 0.0f, 1.0f);
            auto interpolate = [](PointF from, PointF to, float fraction) {
                return PointF(from.X + (to.X - from.X) * fraction, from.Y + (to.Y - from.Y) * fraction);
            };
            PointF leftCuff = interpolate(left[i - 1], left[i], t);
            PointF rightCuff = interpolate(right[i - 1], right[i], t);
            PointF leftLip = interpolate(rightCuff, leftCuff, 1.23f);
            PointF rightLip = interpolate(leftCuff, rightCuff, 1.23f);
            auto offsetAlongArm = [dx, dy, segment](PointF point, float offset) {
                float scale = offset / std::max(segment, 0.001f);
                return PointF(point.X + dx * scale, point.Y + dy * scale);
            };
            PointF leftFront = offsetAlongArm(leftLip, 4.0f);
            PointF rightFront = offsetAlongArm(rightLip, 4.0f);
            GraphicsPath flare;
            flare.AddLine(offsetAlongArm(leftCuff, -8.0f), leftLip);
            flare.AddBezier(leftLip, leftFront, rightFront, rightLip);
            flare.AddLine(rightLip, offsetAlongArm(rightCuff, -8.0f));
            flare.CloseFigure();
            g.FillPath(&cloth, &flare);
            GraphicsPath opening;
            opening.AddBezier(leftLip, leftFront, rightFront, rightLip);
            opening.AddBezier(rightLip, offsetAlongArm(rightLip, -3.5f), offsetAlongArm(leftLip, -3.5f), leftLip);
            opening.CloseFigure();
            SolidBrush lining(rgb(0x252D54));
            g.FillPath(&lining, &opening);

            std::vector<PointF> forearm{leftCuff};
            forearm.insert(forearm.end(), left.begin() + i, left.end());
            for (size_t j = right.size(); j > i; --j) forearm.push_back(right[j - 1]);
            forearm.push_back(rightCuff);

            SolidBrush fur(rgb(0xE3C99F));
            g.FillPolygon(&fur, forearm.data(), static_cast<INT>(forearm.size()));
            drawDazedPaw(g, paw);
            Pen seam(rgb(0x8790C8), 1.25f);
            seam.SetStartCap(LineCapRound);
            seam.SetEndCap(LineCapRound);
            g.DrawBezier(&seam, leftLip, leftFront, rightFront, rightLip);
            break;
        }
    }

    static void drawLimb(Gdiplus::Graphics& g, Pt start, Pt ctrl, Pt end, float startW, float endW, bool outlineOnLeft,
        float outlineAlpha, const SkinSpec& skin, bool frontPaw = false) {
        using namespace Gdiplus;
        const SkinPalette& pal = skin.palette;
        constexpr int steps = 14;
        std::vector<PointF> leftEdge, rightEdge;
        leftEdge.reserve(steps + 1);
        rightEdge.reserve(steps + 1);

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / (float)steps;
            Pt p = quadPoint(start, ctrl, end, t);
            Pt tan = quadTangent(start, ctrl, end, t);
            float len = std::max(0.001f, std::hypot(tan.x, tan.y));
            float nx = -tan.y / len, ny = tan.x / len;
            float hw = (startW + (endW - startW) * t) * 0.5f;
            leftEdge.push_back(PointF(p.x + nx * hw, p.y + ny * hw));
            rightEdge.push_back(PointF(p.x - nx * hw, p.y - ny * hw));
        }

        std::vector<PointF> poly = leftEdge;
        for (int i = steps; i >= 0; --i) poly.push_back(rightEdge[i]);

        GraphicsPath limbPath;
        limbPath.AddLines(poly.data(), (INT)poly.size());
        Pt tangent=quadTangent(start,ctrl,end,0);
        float length=std::max(0.001f,std::hypot(tangent.x,tangent.y));
        Pt cap = {start.x - tangent.x / length * startW * 0.5f, start.y - tangent.y / length * startW * 0.5f};
        addQuadBezier(limbPath, {rightEdge[0].X,rightEdge[0].Y}, cap, {leftEdge[0].X,leftEdge[0].Y});
        limbPath.CloseFigure();

        float pawR = endW * 0.58f;
        RectF pawRect(end.x - pawR, end.y - pawR * 0.85f, pawR * 2.0f, pawR * 1.7f);
        const SkinColor& lineCol = pal.outline.set ? pal.outline : pal.crease;

        if (isDazed(skin)) {
            pawRect.Inflate(pawR * 0.28f, pawR * 0.46f);
            if (frontPaw) {
                drawDazedArm(g, limbPath, leftEdge, rightEdge, pawRect);
            } else {
                SolidBrush fur(rgb(0xE3C99F));
                g.FillPath(&fur, &limbPath);
                drawDazedPaw(g, pawRect);
            }
            return;
        }

        if (isClassicBlack(skin)) {
            Pen outPen(blackOutline(), BlackOutlineW * 2.0f);
            outPen.SetStartCap(LineCapRound); outPen.SetEndCap(LineCapRound); outPen.SetLineJoin(LineJoinRound);
            if ((int)leftEdge.size() > 3) {
                g.DrawLines(&outPen, leftEdge.data() + 2, (INT)leftEdge.size() - 2);
                g.DrawLines(&outPen, rightEdge.data() + 2, (INT)rightEdge.size() - 2);
            }
            g.DrawEllipse(&outPen, pawRect);
        }
        if (isTabby(skin)) {
            tabby::drawLimb(g, limbPath, pawRect, start, ctrl, end, startW, endW);
        } else if (!skin.colorpoint()) {
            Color legs = col(pal.legs);
            SolidBrush b(legs);
            if (pal.belly.set && !pal.belly.sameAs(pal.legs)) {
                // White chest fading into the leg colour along the first 30% of the arm.
                Color chest = col(pal.belly);
                LinearGradientBrush gradient(PointF(start.x,start.y),PointF(end.x,end.y),chest,legs);
                gradient.SetWrapMode(WrapModeTileFlipXY);
                Color colors[3]={chest,legs,legs};
                REAL positions[3]={0,0.30f,1};gradient.SetInterpolationColors(colors,positions,3);
                g.FillPath(&gradient,&limbPath);
            } else g.FillPath(&b, &limbPath);
            if (pal.paws.sameAs(pal.legs)) g.FillEllipse(&b, pawRect);
            else {
                std::vector<PointF> points; int first = (int)(steps*0.72f);
                for (int i=first; i<=steps; ++i) points.push_back(leftEdge[i]);
                for (int i=steps; i>=first; --i) points.push_back(rightEdge[i]);
                SolidBrush cuff(col(pal.paws));
                g.FillPolygon(&cuff, points.data(), (INT)points.size()); g.FillEllipse(&cuff, pawRect);
            }
        } else {
            // Pad gradient endpoints beyond limb geometry + use WrapModeTileFlipXY so GDI+
            // NEVER wraps t < 0 shoulder corners around to t = 1.0 dark espresso!
            float dx = end.x - start.x, dy = end.y - start.y;
            float span = std::hypot(dx, dy);
            float ux = (span > 1.0f) ? (dx / span) : 0.0f;
            float uy = (span > 1.0f) ? (dy / span) : 1.0f;
            span = std::max(1.0f, span);
            float padStart = startW + 18.0f;
            float padEnd = endW + 18.0f;
            float total = padStart + span + padEnd;

            PointF gStart(start.x - ux * padStart, start.y - uy * padStart);
            PointF gEnd(end.x + ux * padEnd, end.y + uy * padEnd);

            LinearGradientBrush grad(gStart, gEnd, withAlpha(pal.body, 0), withAlpha(pal.points, 255));
            grad.SetWrapMode(WrapModeTileFlipXY);
            Color cols[6] = {
                withAlpha(pal.body, 0),
                withAlpha(pal.body, 0),
                withAlpha(pal.body, 255),
                withAlpha(pal.pointsMid, 255),
                withAlpha(pal.points, 255),
                withAlpha(pal.points, 255)
            };
            REAL pos[6] = {
                0.0f,
                padStart / total,
                (padStart + span * 0.16f) / total,
                (padStart + span * 0.48f) / total,
                (padStart + span) / total,
                1.0f
            };
            grad.SetInterpolationColors(cols, pos, 6);
            g.FillPath(&grad, &limbPath);

            SolidBrush pawBrush(col(pal.paws));
            g.FillEllipse(&pawBrush, pawRect);

            const auto& outerEdge = outlineOnLeft ? rightEdge : leftEdge;
            Pen op(withAlpha(lineCol, 184), 1.25f);
            op.SetStartCap(LineCapRound); op.SetEndCap(LineCapRound); op.SetLineJoin(LineJoinRound);
            g.DrawLines(&op, outerEdge.data() + 3, (INT)outerEdge.size() - 4);
        }

        const auto& hiEdge = outlineOnLeft ? leftEdge : rightEdge;
        int startIdx = 2;
        int endIdx = (int)((hiEdge.size() - 1) * 0.68f);
        if (endIdx > startIdx && !isClassicBlack(skin)) {
            Color sc = skin.colorpoint()
                ? withAlpha(lineCol, (BYTE)(outlineAlpha * 199))
                : creaseWith(pal.crease, (BYTE)(outlineAlpha * 229));
            Pen hp(sc, 1.35f);
            hp.SetStartCap(LineCapRound); hp.SetEndCap(LineCapRound); hp.SetLineJoin(LineJoinRound);
            g.DrawLines(&hp, hiEdge.data() + startIdx, endIdx - startIdx + 1);
        }
    }

    // MARK: - Floating Mechanical Keycaps

    static void drawFloatingKeycaps(Gdiplus::Graphics& g, const CatPhysics& physics) {
        using namespace Gdiplus;
        FontFamily ff(L"Segoe UI");
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        for (const auto& b : physics.floatingKeycaps) {
            float prog = b.age / b.lifetime;
            float scale = 1.0f;
            if (b.age < 0.09f) {
                scale = 0.72f + 0.38f * std::sin((b.age / 0.09f) * 3.14159f * 0.65f);
            } else if (b.age < 0.18f) {
                scale = 1.10f - 0.10f * ((b.age - 0.09f) / 0.09f);
            }

            float alphaF = prog < 0.62f ? 1.0f : std::max(0.0f, 1.0f - (prog - 0.62f) / 0.38f);
            BYTE a = (BYTE)(alphaF * 255.0f);
            if (a < 4) continue;

            size_t len = b.label.size();
            bool hasWide = !b.label.empty() && b.label[0] > 0x7F;
            float w = (len <= 1) ? 23.0f : (len == 2 ? (hasWide ? 34.0f : 30.0f) : 38.0f);
            float h = 22.0f;

            GraphicsState st = g.Save();
            g.TranslateTransform(b.position.x, b.position.y);
            g.RotateTransform(b.rotation * RadToDeg);
            g.ScaleTransform(scale, scale);

            GraphicsPath basePath;
            addRoundedRect(basePath, -w * 0.5f, -h * 0.5f + 2.5f, w, h, 5.5f);
            SolidBrush baseBrush(Color((BYTE)(alphaF * 242), 38, 38, 43));
            g.FillPath(&baseBrush, &basePath);

            GraphicsPath facePath;
            addRoundedRect(facePath, -w * 0.5f, -h * 0.5f, w, h, 5.2f);
            SolidBrush faceBrush(Color(a, 249, 248, 243));
            Pen facePen(Color(a, 41, 41, 46), 1.45f);
            g.FillPath(&faceBrush, &facePath);
            g.DrawPath(&facePen, &facePath);

            Font font(&ff, len <= 1 ? 11.0f : 9.2f, FontStyleBold, UnitPixel);
            SolidBrush txtBrush(Color(a, 31, 31, 36));
            g.DrawString(b.label.c_str(), -1, &font, RectF(-w * 0.5f, -h * 0.5f, w, h), &sf, &txtBrush);

            g.Restore(st);
        }
    }

    // MARK: - Petting Sparkles & Sleep Z

    static void drawSparkles(Gdiplus::Graphics& g, const CatPhysics& physics, EyeColorTheme eyeTheme) {
        using namespace Gdiplus;
        Color baseCol = eyePrimary(eyeTheme);
        FontFamily ff(L"Segoe UI");

        for (const auto& sp : physics.sparkles) {
            float p = std::min(1.0f, sp.age / sp.lifetime);
            BYTE a = (BYTE)(std::sin(p * 3.14159f) * 215.0f);
            if (a < 4) continue;

            Color c(a, baseCol.GetR(), baseCol.GetG(), baseCol.GetB());
            if (sp.isSleepZ) {
                Font f(&ff, 11.0f + p * 3.0f, FontStyleBold, UnitPixel);
                SolidBrush sb(c);
                g.DrawString(L"z", -1, &f, PointF(sp.position.x, sp.position.y), &sb);
            } else {
                float r = 3.5f + p * 1.5f;
                GraphicsState st = g.Save();
                g.TranslateTransform(sp.position.x, sp.position.y);
                GraphicsPath star;
                addQuadBezier(star, {0, -r}, {0.8f, -0.8f}, {r, 0});
                addQuadBezier(star, {r, 0}, {0.8f, 0.8f}, {0, r});
                addQuadBezier(star, {0, r}, {-0.8f, 0.8f}, {-r, 0});
                addQuadBezier(star, {-r, 0}, {-0.8f, -0.8f}, {0, -r});
                star.CloseFigure();
                SolidBrush sb(c);
                g.FillPath(&sb, &star);
                g.Restore(st);
            }
        }
    }
};
