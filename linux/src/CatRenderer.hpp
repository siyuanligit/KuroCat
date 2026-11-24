#pragma once
#include <cairo.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include "CatPhysics.hpp"
#include "../../shared/SkinSpec.hpp"

enum class CatHitZone {
    None,
    Head,
    Body
};

struct RGBA {
    double r, g, b, a;
};

// The four built-in coats expressed as SkinSpecs. Colours are the exact doubles the Cairo renderer
// always used, so built-ins render pixel-identically to the pre-SkinSpec renderer; the exported
// templates in skins/*.kurocat carry the same colours rounded to #RRGGBB(AA).
inline const SkinSpec& builtInSkin(CatCoatTheme coat) {
    static const std::vector<SkinSpec> skins = [] {
        const SkinColor black = SkinColor::rgba(0.085, 0.085, 0.095);
        const SkinColor mint = SkinColor::rgba(0.82, 0.92, 0.89, 0.76);
        const SkinColor whiskerWhite = SkinColor::rgba(0.88, 0.90, 0.92, 0.68);
        const SkinColor milk = SkinColor::hex(0xF2EEE4);
        auto tailRings = [](unsigned int rgb) {
            std::vector<SkinMark> rings;
            for (float x : {50.0f, 78.0f}) {
                SkinMark m;
                m.space = SkinSpace::Tail; m.stroke = true; m.width = 3.5f; m.color = SkinColor::hex(rgb, 0.50);
                m.points = {{x, 8.0f}, {x, -8.0f}};
                rings.push_back(m);
            }
            return rings;
        };
        std::vector<SkinSpec> v(5);
        SkinSpec& k = v[0];
        k.id = "builtin:0"; k.name = "经典小黑猫"; k.builtIn = true;
        k.palette.body = k.palette.legs = k.palette.paws = k.palette.earOuter = k.palette.tail = black;
        k.palette.whiskers = whiskerWhite; k.palette.crease = mint;

        SkinSpec& s = v[1];
        s.id = "builtin:1"; s.name = "挖煤暹罗猫"; s.builtIn = true; s.shape.build = SkinBuild::Sleek;
        const SkinColor point = SkinColor::rgba(0.208, 0.149, 0.122);
        s.palette.body = SkinColor::rgba(0.965, 0.937, 0.894);
        s.palette.bodyShade = SkinColor::rgba(0.918, 0.859, 0.784);
        s.palette.points = s.palette.paws = s.palette.earOuter = s.palette.tail = s.palette.legs = point;
        s.palette.pointsMid = SkinColor::rgba(0.416, 0.314, 0.255);
        s.palette.earInner = SkinColor::rgba(0.365, 0.275, 0.228);
        s.palette.outline = SkinColor::rgba(0.68, 0.59, 0.50, 0.42);
        s.palette.crease = SkinColor::rgba(0.553, 0.471, 0.404, 0.54);
        s.palette.whiskers = SkinColor::rgba(0.478, 0.388, 0.325, 0.62);
        s.palette.nose = SkinColor::rgba(0.14, 0.09, 0.08, 0.95);
        s.palette.mouth = SkinColor::rgba(0.78, 0.66, 0.56, 0.55);

        SkinSpec& h = v[2];
        h.id = "builtin:2"; h.name = "美短（银虎斑加白）"; h.builtIn = true;
        h.palette.body = h.palette.legs = h.palette.earOuter = h.palette.tail = SkinColor::hex(0xC2C5C1);
        h.palette.belly = h.palette.paws = milk;
        h.palette.stripes = SkinColor::hex(0x55595A);
        h.palette.whiskers = SkinColor::hex(0x8D8170, 0.68); h.palette.crease = SkinColor::hex(0x8B8171, 0.60);
        h.headSurface = SkinHeadSurface::Plane; h.shape.pawStagger = 0;
        h.marks = skinMarksFromCoat(shorthairMarks());
        for (auto& m : tailRings(0x55595A)) h.marks.push_back(m);

        SkinSpec& o = v[3];
        o.id = "builtin:3"; o.name = "橘白猫"; o.builtIn = true;
        o.palette.body = milk;
        o.palette.legs = o.palette.earOuter = o.palette.tail = SkinColor::hex(0xD8AA73);
        o.palette.belly = o.palette.paws = milk;
        o.palette.stripes = SkinColor::hex(0xA57543);
        o.palette.whiskers = SkinColor::hex(0x8D8170, 0.68); o.palette.crease = SkinColor::hex(0x8B8171, 0.60);
        o.headSurface = SkinHeadSurface::Dome; o.shape.pawStagger = 0;
        o.marks = skinMarksFromCoat(orangeWhiteMarks());
        for (auto& m : tailRings(0xA57543)) o.marks.push_back(m);
        // Solid British Blue (dark gray, no markings); eyes come from the eye-colour menu (paired: warm amber).
        SkinSpec& g = v[4];
        g.id = "builtin:4"; g.name = "英短蓝猫（灰毛橘黄瞳）"; g.builtIn = true;
        g.palette.body = g.palette.legs = g.palette.paws = g.palette.earOuter = g.palette.tail = SkinColor::hex(0x3A3F47);
        g.palette.earInner = SkinColor::hex(0xB9A4A4); g.palette.nose = SkinColor::hex(0x9C7F82);
        g.palette.whiskers = SkinColor::hex(0xD8DCE0, 0.68); g.palette.crease = SkinColor::hex(0x181B20, 0.75);
        g.headSurface = SkinHeadSurface::Dome; g.shape.pawStagger = 0;
        for (SkinSpec& b : v) b.idleLegsTinted = false;
        return v;
    }();
    int i = std::clamp((int)coat, 0, 4);
    return skins[(size_t)i];
}

class CatRenderer {
public:
    static CatHitZone hitTest(Pt p, const CatPhysics& physics, const SkinSpec& skin) {
        const Build& b = buildOf(skin);
        const SkinShape& shape = skin.shape;
        float cx = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        Pt headCenter = { cx + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f,
                          126.0f + b.headLift + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f };

        // Petting zones follow the reshaped head (size + cheeks) and body (width).
        float hdx = (p.x - headCenter.x) / (44.0f * shape.headScale * shape.cheekFullness);
        float hdy = (p.y - headCenter.y) / (38.0f * shape.headScale);
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

    static void draw(cairo_t* cr, const CatPhysics& physics, const CatSettings& settings) {
        draw(cr, physics, settings, builtInSkin(settings.coatTheme));
    }

    static void draw(cairo_t* cr, const CatPhysics& physics, const CatSettings& settings, const SkinSpec& skin) {
        cairo_save(cr);
        if (settings.flipHorizontal) {
            cairo_translate(cr, CatPhysics::CanvasWidth, 0.0);
            cairo_scale(cr, -1.0, 1.0);
        }
        cairo_set_antialias(cr, CAIRO_ANTIALIAS_BEST);

        float breath = std::sin(physics.time * 2.1f) * 1.6f;
        float scruff = std::clamp(physics.scruffDrag.value, 0.0f, 1.0f);
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float dizzy = std::clamp(physics.dizzyIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float dizzyFallRaw = std::clamp((dizzy - 0.50f) / 0.50f, 0.0f, 1.0f);
        float dizzyFall = dizzyFallRaw * dizzyFallRaw * (3.0f - 2.0f * dizzyFallRaw);
        float preWobble = std::clamp((dizzy - 0.12f) / 0.38f, 0.0f, 1.0f) * (1.0f - dizzyFall);

        float seatX = CatPhysics::SeatAnchor.x + physics.bodyShiftX.value;
        float seatY = CatPhysics::SeatAnchor.y - scruff * 22.0f + lieS * 10.0f;
        float fallAngle = -dizzyFall * 1.46f + std::sin(physics.time * 9.5f) * 0.16f * preWobble;
        float fallShiftX = dizzyFall * 56.0f;
        float fallShiftY = dizzyFall * 20.0f;

        Pt headCenter = {
            seatX + physics.bodyTilt.value * 36.0f + physics.headYaw.value * 7.5f - lieS * 4.0f,
            126.0f + buildOf(skin).headLift - scruff * 22.0f + physics.headOffsetY.value + physics.headPitch.value * 4.8f + breath * 1.1f + lieS * 60.0f
        };

        if (physics.isTabletActive) {
            drawTablet(cr, physics, settings.keyboardTheme, settings.eyeTheme);
            if (!physics.leftPawOnMouse || scruff > 0.25f || dizzy > 0.35f || lie > 0.45f) {
                drawParkedStylus(cr, settings.keyboardTheme, settings.eyeTheme);
            }
        } else {
            drawMouse(cr, physics, settings.keyboardTheme);
        }
        drawKeyboard(cr, physics, settings.keyboardTheme, settings.eyeTheme, settings.showKeyLegends, settings.keyboardMarquee);
        cairo_save(cr);
        if (dizzy > 0.01f) {
            cairo_translate(cr, seatX + fallShiftX, seatY + fallShiftY);
            cairo_rotate(cr, fallAngle);
            cairo_translate(cr, -seatX, -seatY);
        }

        bool drawBlackRim = isClassicBlack(skin);
        bool tailInFront = (physics.tailTyping.value >= 0.5f) || (lieS >= 0.45f);
        if (drawBlackRim) {
            if (!tailInFront) drawTail(cr, seatX, seatY, physics, skin, true);
            drawEarsLayer(cr, headCenter, physics, skin, true);
            drawUnifiedSilhouette(cr, seatX, seatY, headCenter, breath, physics, skin, true);
        }
        if (!tailInFront) drawTail(cr, seatX, seatY, physics, skin);
        drawEarsLayer(cr, headCenter, physics, skin);
        drawUnifiedSilhouette(cr, seatX, seatY, headCenter, breath, physics, skin);

        if (physics.hindFootActive.value > 0.03f) {
            drawHindFoot(cr, seatX, seatY, physics, skin);
        }

        drawHeadFeatures(cr, headCenter, physics, settings.eyeTheme, skin);
        if (!skin.decorations.empty()) drawDecorations(cr, seatX, seatY, headCenter, physics, skin);
        drawFrontPaws(cr, seatX, seatY, breath, physics, skin);

        if (physics.isTabletActive && physics.leftPawOnMouse && scruff <= 0.25f && dizzy <= 0.35f && lie <= 0.45f) {
            drawHeldStylus(cr, physics, settings.keyboardTheme, settings.eyeTheme);
        }

        if (tailInFront) {
            if (drawBlackRim) drawTail(cr, seatX, seatY, physics, skin, true);
            drawTail(cr, seatX, seatY, physics, skin);
        }
        cairo_restore(cr);

        // Upright orbiting golden dizzy stars floating right above the head (appear FIRST before falling!)
        if (dizzy > 0.10f) {
            double relX = headCenter.x - seatX;
            double relY = headCenter.y - seatY;
            double worldHeadX = seatX + fallShiftX + relX * std::cos(fallAngle) - relY * std::sin(fallAngle);
            double worldHeadY = seatY + fallShiftY + relX * std::sin(fallAngle) + relY * std::cos(fallAngle);
            double dAlpha = std::min(1.0, (dizzy - 0.10) / 0.22);
            double starLift = 42.0 + dizzyFall * 10.0;
            cairo_save(cr);
            cairo_translate(cr, worldHeadX, worldHeadY - starLift);
            for (int i = 0; i < 3; ++i) {
                double ang = physics.dizzyPhase * 0.85 + i * (PI * 2.0 / 3.0);
                double sx = std::cos(ang) * 26.0;
                double sy = std::sin(ang) * 6.5;
                double r = 4.4 * (0.75 + 0.25 * std::sin(ang));
                cairo_save(cr);
                cairo_translate(cr, sx, sy);
                cairo_rotate(cr, physics.dizzyPhase + i);
                cairo_set_source_rgba(cr, 1.0, 0.86, 0.22, 0.96 * dAlpha);
                cairo_new_path(cr);
                for (int k = 0; k < 8; ++k) {
                    double rad = (k % 2 == 0) ? r : r * 0.42;
                    double a = k * PI * 0.25;
                    if (k == 0) cairo_move_to(cr, std::cos(a) * rad, std::sin(a) * rad);
                    else cairo_line_to(cr, std::cos(a) * rad, std::sin(a) * rad);
                }
                cairo_close_path(cr);
                cairo_fill(cr);
                cairo_restore(cr);
            }
            cairo_restore(cr);
        }

        if (!physics.floatingKeycaps.empty()) {
            drawFloatingKeycaps(cr, physics);
        }
        drawSparkles(cr, physics, settings.eyeTheme);
        cairo_restore(cr);

        if (settings.isEditMode) {
            drawEditOverlay(cr, settings.isEnglish, settings.scale);
        }
    }

    static void drawEditOverlay(cairo_t* cr, bool isEnglish, float scale) {
        cairo_save(cr);
        roundedRect(cr, 10.0, 10.0, 340.0, 300.0, 12.0);
        cairo_set_source_rgba(cr, 0.16, 0.56, 0.98, 0.07);
        cairo_fill_preserve(cr);
        double dashes[2] = { 6.0, 4.0 };
        cairo_set_dash(cr, dashes, 2, 0.0);
        cairo_set_source_rgba(cr, 0.20, 0.62, 1.0, 0.92);
        cairo_set_line_width(cr, 1.8);
        cairo_stroke(cr);
        cairo_set_dash(cr, nullptr, 0, 0.0);

        constexpr Pt corners[4] = { {10.0f, 10.0f}, {350.0f, 10.0f}, {10.0f, 310.0f}, {350.0f, 310.0f} };
        for (const auto& c : corners) {
            cairo_new_path(cr);
            cairo_arc(cr, c.x, c.y, 6.5, 0.0, PI * 2.0);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.14, 0.52, 0.96, 1.0);
            cairo_set_line_width(cr, 2.0);
            cairo_stroke(cr);
        }

        auto drawBtn = [&](double x, double y, double w, double h, const char* txt, bool primary) {
            roundedRect(cr, x, y, w, h, 11.0);
            if (primary) cairo_set_source_rgba(cr, 0.16, 0.54, 0.96, 0.95);
            else cairo_set_source_rgba(cr, 0.14, 0.15, 0.19, 0.88);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.28);
            cairo_set_line_width(cr, 0.9);
            cairo_stroke(cr);

            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 10.5);
            cairo_text_extents_t te = {};
            cairo_text_extents(cr, txt, &te);
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_move_to(cr, x + (w - te.width) * 0.5 - te.x_bearing, y + (h - te.height) * 0.5 - te.y_bearing);
            cairo_show_text(cr, txt);
        };

        drawBtn(94.0, 12.0, 82.0, 24.0, isEnglish ? "Flip" : "翻转", false);
        drawBtn(184.0, 12.0, 82.0, 24.0, isEnglish ? "Done" : "完成", true);

        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.0f%%", scale * 100.0f);
        roundedRect(cr, 158.0, 290.0, 44.0, 16.0, 6.0);
        cairo_set_source_rgba(cr, 0.12, 0.13, 0.16, 0.82);
        cairo_fill(cr);
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 9.5);
        cairo_text_extents_t te = {};
        cairo_text_extents(cr, buf, &te);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cr, 180.0 - te.width * 0.5 - te.x_bearing, 298.0 - te.height * 0.5 - te.y_bearing);
        cairo_show_text(cr, buf);
        cairo_restore(cr);
    }

private:
    static constexpr double PI = 3.14159265358979323846;

    static constexpr RGBA catBlack      = { 0.085, 0.085, 0.095, 1.0 };
    static constexpr double BlackOutlineW = 1.6;
    static constexpr RGBA blackOutline  = { 1.0, 1.0, 1.0, 0.94 };

    static bool isClassicBlack(const SkinSpec& skin) { return skin.builtIn && skin.id == "builtin:0"; }
    static bool isBuiltInShorthair(const SkinSpec& skin) { return skin.builtIn && skin.id == "builtin:2"; }
    static bool isBuiltInOrangeWhite(const SkinSpec& skin) { return skin.builtIn && skin.id == "builtin:3"; }
    static bool isBuiltInPatterned(const SkinSpec& skin) { return isBuiltInShorthair(skin) || isBuiltInOrangeWhite(skin); }
    static RGBA rgb(unsigned int hex, double a = 1.0) { return {((hex >> 16) & 255) / 255.0, ((hex >> 8) & 255) / 255.0, (hex & 255) / 255.0, a}; }

    // Base skeleton per SkinBuild; SkinShape multipliers are applied on top (all exact no-ops at 1).
    struct Build {
        float headLift;
        float cheekHalfW, crownCtrlW, crownBaseY, cheekY, cheekCtrlTopY, neckPinchX, neckCtrlY;
        float hipHalfW, waistCtrlW, waistCtrlY, botOuterW, botInnerW, botHalfW;
        float earFold, earLen, earInX, earInY, earOutX, earOutY, earTipX, earTipY, earTipSpread;
        double tailWidth;
        double hindCX, hindRX, hindLeftCY, hindLeftRY, hindStaggerCY, hindStaggerRY;
        float idleTopDY, idleTopX, idleBotX, shoulderX, limbStartW, limbEndW;
        bool sleekEyes;
    };
    static const Build& buildOf(const SkinSpec& skin) {
        static const Build standard = { 0.0f,
            42.5f, 26.0f, -30.0f, 5.0f, -14.0f, 2.5f, 24.0f,
            49.0f, 51.5f, -48.0f, 47.0f, 36.0f, 24.0f,
            0.06f, 32.0f, 9.0f, -16.0f, 35.0f, -5.0f, 24.5f, -20.0f, 3.8f,
            11.0,
            26.5, 8.5, 8.5, 5.5, 10.75, 6.25,
            30.0f, 18.0f, 16.0f, 20.0f, 19.0f, 13.5f,
            false };
        static const Build sleek = { -3.0f,
            42.5f, 25.0f, -29.5f, 6.5f, -13.0f, 6.5f, 27.5f,
            44.5f, 41.0f, -50.0f, 42.0f, 32.0f, 22.0f,
            0.14f, 29.5f, 10.0f, -16.5f, 35.5f, -4.5f, 25.0f, -18.5f, 4.2f,
            9.5,
            23.25, 7.75, 9.5, 6.0, 10.75, 6.25,
            31.0f, 16.5f, 15.0f, 18.5f, 17.0f, 12.5f,
            true };
        return skin.shape.build == SkinBuild::Sleek ? sleek : standard;
    }

    // Scale v around c by k; returns v untouched when k == 1 so default shapes stay bit-identical.
    static float around(float v, float c, float k) { return k == 1.0f ? v : c + (v - c) * k; }
    static double around(double v, double c, double k) { return k == 1.0 ? v : c + (v - c) * k; }

    static void setCol(cairo_t* cr, RGBA c, double alphaMul = 1.0) {
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alphaMul);
    }
    static void setCol(cairo_t* cr, const SkinColor& c, double alphaMul = 1.0) {
        cairo_set_source_rgba(cr, c.r, c.g, c.b, c.a * alphaMul);
    }

    // Head-local point -> canvas, on the skin's head surface (dome / plane) or flat skull space.
    static Pt headPoint(SkinPoint p, bool skull, const SkinSpec& skin, Pt head, const CatPhysics& physics) {
        float depth = skin.headSurface == SkinHeadSurface::Dome ? 29.6f*std::sqrt(std::max(0.0f,1-p.x*p.x/(46*46)-p.y*p.y/(40*40))) : 27.5f;
        Pt q = skull ? Pt{p.x,p.y} : projectHead3D(p.x,p.y,depth,physics.headYaw.value*0.50f,physics.headPitch.value*0.36f);
        float hs = skin.shape.headScale;
        q.x *= hs; q.y *= hs;
        float c=std::cos(physics.headTilt.value),s=std::sin(physics.headTilt.value);
        return {head.x+q.x*c-q.y*s,head.y+q.x*s+q.y*c};
    }
    static Pt bodyPoint(SkinPoint p, const SkinSpec& skin, float seatX, float seatY, const CatPhysics& physics) {
        float bw = skin.shape.bodyWidth;
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        float c=std::cos(physics.bodyTilt.value),s=std::sin(physics.bodyTilt.value);
        return {seatX+p.x*bw*sx*c-p.y*sy*s,seatY+p.x*bw*sx*s+p.y*sy*c};
    }
    static Pt coatMarkPoint(CoatPoint p, bool headSurface, bool skull, bool isOrangeWhite, float seatX, float seatY, Pt head, const CatPhysics& physics, bool whiteSurface = false) {
        if (headSurface) {
            float radius = std::max(0.0f, 1.0f - p.x * p.x / (46.0f * 46.0f) - p.y * p.y / (40.0f * 40.0f));
            float surface = whiteSurface ? radius * radius * (3.0f - 2.0f * radius) : std::sqrt(radius);
            float depth = isOrangeWhite ? 29.6f * surface : 27.5f;
            Pt q = skull ? Pt{p.x, p.y} : projectHead3D(p.x, p.y, depth, physics.headYaw.value * 0.50f, physics.headPitch.value * 0.36f);
            float c = std::cos(physics.headTilt.value), s = std::sin(physics.headTilt.value);
            return { head.x + q.x * c - q.y * s, head.y + q.x * s + q.y * c };
        }
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        float c = std::cos(physics.bodyTilt.value), s = std::sin(physics.bodyTilt.value);
        return { seatX + p.x * sx * c - p.y * sy * s, seatY + p.x * sx * s + p.y * sy * c };
    }
    static void drawBuiltInCoatMarks(cairo_t* cr, float seatX, float seatY, Pt head, const CatPhysics& physics, bool isOrangeWhite) {
        const auto& marks = isOrangeWhite ? orangeWhiteMarks() : shorthairMarks();
        const CoatMark* bib = nullptr;
        for (const auto& mark : marks) if (mark.head && !mark.skull && mark.rgb == 0xF2EEE4) {
            for (const auto& p : mark.points) if (std::abs(p.x) > 46.0f) { bib = &mark; break; }
            if (bib) break;
        }
        float overlap = std::min(1.0f, std::abs(physics.headYaw.value) * std::max(0.0f, physics.headPitch.value));
        float coverage = overlap * overlap * (3.0f - 2.0f * overlap);
        for (const auto& mark : marks) {
            if (mark.clipToBib && coverage == 0.0f) continue;
            cairo_save(cr);
            if (mark.clipToBib && bib) {
                cairo_new_path(cr); bool first = true;
                for (const auto& point : bib->points) {
                    Pt p = coatMarkPoint(point, true, false, isOrangeWhite, seatX, seatY, head, physics, true);
                    if (first) { cairo_move_to(cr, p.x, p.y); first = false; } else cairo_line_to(cr, p.x, p.y);
                }
                cairo_close_path(cr); cairo_clip(cr);
            }
            cairo_new_path(cr);
            bool first = true;
            for (auto point : mark.points) {
                if (mark.clipToBib) {
                    float rim = std::min(1.0f, std::max(0.0f, (point.y - 20.0f) / 6.0f));
                    point.x *= 1.0f - 0.5f * rim * (1.0f - coverage);
                    if (point.y > 20.0f) point.y = 20.0f + (point.y - 20.0f) * (0.75f + 0.25f * coverage);
                }
                Pt p = coatMarkPoint(point, mark.head, mark.skull, isOrangeWhite, seatX, seatY, head, physics, mark.rgb == 0xF2EEE4);
                if (first) { cairo_move_to(cr, p.x, p.y); first = false; } else cairo_line_to(cr, p.x, p.y);
            }
            setCol(cr, rgb(mark.rgb, mark.alpha));
            if (mark.width == 0.0f) { cairo_close_path(cr); cairo_fill(cr); }
            else { cairo_set_line_width(cr, mark.width); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND); cairo_stroke(cr); }
            cairo_restore(cr);
        }
    }
    template <class Map>
    static void tracePolyline(cairo_t* cr, const std::vector<SkinPoint>& pts, Map map) {
        cairo_new_path(cr);
        bool first = true;
        for (const SkinPoint& point : pts) {
            auto p = map(point);
            if (first) { cairo_move_to(cr, p.x, p.y); first = false; } else cairo_line_to(cr, p.x, p.y);
        }
    }
    // Strokes get round caps, except tail-space strokes (bands around the tail) which are cut flat.
    template <class Map>
    static void drawMark(cairo_t* cr, const SkinMark& mark, Map map, cairo_line_cap_t cap = CAIRO_LINE_CAP_ROUND) {
        tracePolyline(cr, mark.points, map);
        setCol(cr, mark.color);
        if (!mark.stroke) { cairo_close_path(cr); cairo_fill(cr); }
        else { cairo_set_line_width(cr,mark.width);cairo_set_line_cap(cr,cap);cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);cairo_stroke(cr); }
    }
    template <class Map>
    static void drawRegion(cairo_t* cr, const SkinRegion& region, Map map) {
        tracePolyline(cr, region.flat, map);
        cairo_close_path(cr);
        setCol(cr, region.fill);
        cairo_fill(cr);
    }
    // The head (above) or body (below) side of the jaw line, following the head like head marks do.
    static void jawSide(cairo_t* cr, bool headSide, const SkinSpec& skin, Pt head, const CatPhysics& physics) {
        tracePolyline(cr, skinJawSide(headSide, skin.shape.cheekFullness),
                      [&](SkinPoint p) { return headPoint(p, false, skin, head, physics); });
        cairo_close_path(cr);
    }

    static RGBA eyePrimary(EyeColorTheme t) {
        switch (t) {
            case EyeColorTheme::NeonChartreuse: return { 0.79, 0.95, 0.25, 1.0 };
            case EyeColorTheme::SiameseBlue:    return { 0.69, 0.855, 0.95, 1.0 };
            case EyeColorTheme::WarmAmber:      return { 0.98, 0.76, 0.22, 1.0 };
            case EyeColorTheme::IceCyan:        return { 0.45, 0.92, 0.96, 1.0 };
            case EyeColorTheme::ReferenceHazel: return {218.0/255,178.0/255,87.0/255,1};
            case EyeColorTheme::SoftAmber: return {217.0/255,173.0/255,85.0/255,1};
        }
        return { 0.79, 0.95, 0.25, 1.0 };
    }

    static RGBA blend(RGBA a, RGBA b, double t) {
        t = std::clamp(t, 0.0, 1.0);
        return { a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t };
    }

    // Hue wheel helper for the keyboard marquee (one full hue cycle per sweep).
    static RGBA hsvColor(double h, double s, double v) {
        h = h - std::floor(h);
        double c = v * s;
        double x = c * (1.0 - std::fabs(std::fmod(h * 6.0, 2.0) - 1.0));
        double m = v - c, r = 0, g = 0, b = 0;
        switch ((int)(h * 6.0) % 6) {
            case 0: r = c; g = x; break;
            case 1: r = x; g = c; break;
            case 2: g = c; b = x; break;
            case 3: g = x; b = c; break;
            case 4: r = x; b = c; break;
            default: r = c; b = x; break;
        }
        return { r + m, g + m, b + m, 1.0 };
    }

    static void quadCurveTo(cairo_t* cr, double x0, double y0, double cx, double cy, double x2, double y2) {
        double c1x = x0 + (2.0 / 3.0) * (cx - x0);
        double c1y = y0 + (2.0 / 3.0) * (cy - y0);
        double c2x = x2 + (2.0 / 3.0) * (cx - x2);
        double c2y = y2 + (2.0 / 3.0) * (cy - y2);
        cairo_curve_to(cr, c1x, c1y, c2x, c2y, x2, y2);
    }

    static void roundedRect(cairo_t* cr, double x, double y, double w, double h, double r) {
        r = std::min(r, std::min(w * 0.5, h * 0.5));
        cairo_new_sub_path(cr);
        cairo_arc(cr, x + w - r, y + r, r, -PI * 0.5, 0);
        cairo_arc(cr, x + w - r, y + h - r, r, 0, PI * 0.5);
        cairo_arc(cr, x + r, y + h - r, r, PI * 0.5, PI);
        cairo_arc(cr, x + r, y + r, r, PI, PI * 1.5);
        cairo_close_path(cr);
    }

    static void fillEllipse(cairo_t* cr, double cx, double cy, double rx, double ry) {
        cairo_save(cr);
        cairo_translate(cr, cx, cy);
        cairo_scale(cr, rx, ry);
        cairo_arc(cr, 0, 0, 1.0, 0, PI * 2.0);
        cairo_restore(cr);
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

    static void drawMouse(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme) {
        Pt center = { CatPhysics::MouseBaseCenter.x + physics.mouseOffset.point().x,
                      CatPhysics::MouseBaseCenter.y + physics.mouseOffset.point().y };
        float pressShift = std::max(physics.mousePressLeft.value, physics.mousePressRight.value) * 1.6f;

        cairo_save(cr);
        cairo_translate(cr, center.x, center.y + pressShift);
        cairo_rotate(cr, 0.22);

        // Shadow
        cairo_new_path(cr);
        fillEllipse(cr, 0, 4.0, 13.5, 15.5);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.24);
        cairo_fill(cr);

        RGBA bodyCol, borderCol, wheelCol;
        if (theme == KeyboardTheme::Graphite) {
            bodyCol = {0.27, 0.28, 0.30, 1.0}; borderCol = {0.10, 0.10, 0.11, 1.0}; wheelCol = {0.40, 0.41, 0.43, 1.0};  // wheel: same grey, a step lighter
        } else if (theme == KeyboardTheme::MidnightHacker) {
            bodyCol = {0.20, 0.21, 0.24, 1.0}; borderCol = {0.08, 0.08, 0.10, 1.0}; wheelCol = {0.78, 0.95, 0.25, 1.0};
        } else if (theme == KeyboardTheme::RetroCream) {
            bodyCol = {0.89, 0.87, 0.82, 1.0}; borderCol = {0.20, 0.19, 0.18, 1.0}; wheelCol = {0.88, 0.42, 0.24, 1.0};
        } else {
            bodyCol = {0.955, 0.950, 0.925, 1.0}; borderCol = {0.14, 0.14, 0.16, 1.0}; wheelCol = {0.96, 0.69, 0.16, 1.0};
        }

        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, 13.5, 16.5);
        setCol(cr, bodyCol);
        cairo_fill_preserve(cr);

        if (physics.mousePressLeft.value > 0.05f || physics.mousePressRight.value > 0.05f) {
            cairo_save(cr);
            cairo_clip(cr);
            if (physics.mousePressLeft.value > 0.05f) {
                if (theme == KeyboardTheme::Graphite) cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.38 * physics.mousePressLeft.value);
                else cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 0.38 * physics.mousePressLeft.value);
                cairo_rectangle(cr, 0, 0, 14, 17);
                cairo_fill(cr);
            }
            if (physics.mousePressRight.value > 0.05f) {
                if (theme == KeyboardTheme::Graphite) cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.38 * physics.mousePressRight.value);
                else cairo_set_source_rgba(cr, 0.79, 0.95, 0.25, 0.38 * physics.mousePressRight.value);
                cairo_rectangle(cr, -14, 0, 14, 17);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        } else {
            cairo_new_path(cr);
        }

        // Front button split seam pointing away from cat (bottom)
        setCol(cr, borderCol, 0.32);
        cairo_set_line_width(cr, 1.05);
        cairo_move_to(cr, 0, 16.0);
        cairo_line_to(cr, 0, 1.5);
        cairo_stroke(cr);

        // Amber scroll wheel
        roundedRect(cr, -1.9, 2.5, 3.8, 8.5, 1.9);
        setCol(cr, wheelCol);
        cairo_fill(cr);

        // Outer border
        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, 13.5, 16.5);
        setCol(cr, borderCol);
        cairo_set_line_width(cr, 1.7);
        cairo_stroke(cr);

        cairo_restore(cr);
    }

    // MARK: - Mini 2.5D Graphics Tablet & Stylus

    static void drawTablet(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        Pt tl = CatPhysics::tabletQuadPoint(0.0f, 0.0f);
        Pt tr = CatPhysics::tabletQuadPoint(1.0f, 0.0f);
        Pt bl = CatPhysics::tabletQuadPoint(0.0f, 1.0f);
        Pt br = CatPhysics::tabletQuadPoint(1.0f, 1.0f);
        constexpr float depth = 3.6f;

        RGBA baseCol, bodyCol, padCol, keyCol, accentCol;
        if (theme == KeyboardTheme::Graphite) {
            baseCol = {0.08,0.08,0.09,1}; bodyCol = {0.17,0.17,0.18,1};
            padCol = {0.22,0.22,0.24,1}; keyCol = {0.27,0.28,0.30,1}; accentCol = keyCol;
        } else if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = {0.05,0.05,0.07,1}; bodyCol = {0.11,0.12,0.15,1};
            padCol = {0.16,0.17,0.21,1}; keyCol = {0.24,0.25,0.30,1}; accentCol = {0.78,0.95,0.25,1};
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = {0.22,0.21,0.19,1}; bodyCol = {0.33,0.31,0.28,1};
            padCol = {0.42,0.39,0.35,1}; keyCol = {0.54,0.50,0.45,1}; accentCol = {0.88,0.42,0.24,1};
        } else {
            baseCol = {0.10,0.10,0.12,1}; bodyCol = {0.17,0.18,0.21,1};
            padCol = {0.23,0.24,0.28,1}; keyCol = {0.30,0.32,0.37,1}; accentCol = {0.96,0.69,0.16,1};
        }

        cairo_save(cr);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        // Shadow
        drawQuadPath(cr, {tl.x, tl.y+depth+2.0f}, {tr.x, tr.y+depth+2.0f}, {br.x, br.y+depth+2.6f}, {bl.x, bl.y+depth+2.6f});
        cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
        cairo_fill_preserve(cr); cairo_set_line_width(cr, 3.0); cairo_stroke(cr);
        // Lower 3D slab
        drawQuadPath(cr, {tl.x, tl.y+depth}, {tr.x, tr.y+depth}, {br.x, br.y+depth}, {bl.x, bl.y+depth});
        setCol(cr, baseCol);
        cairo_fill_preserve(cr); cairo_set_line_width(cr, 2.5); cairo_stroke(cr);
        // Top body
        drawQuadPath(cr, tl, tr, br, bl);
        setCol(cr, bodyCol);
        cairo_fill_preserve(cr); cairo_set_line_width(cr, 2.0); cairo_stroke(cr);
        drawQuadPath(cr, tl, tr, br, bl);
        cairo_set_source_rgba(cr, 1, 1, 1, 0.14);
        cairo_set_line_width(cr, 0.9); cairo_stroke(cr);

        // Active pad
        Pt pTL = CatPhysics::tabletQuadPoint(0.21f, 0.11f);
        Pt pTR = CatPhysics::tabletQuadPoint(0.94f, 0.11f);
        Pt pBR = CatPhysics::tabletQuadPoint(0.94f, 0.89f);
        Pt pBL = CatPhysics::tabletQuadPoint(0.21f, 0.89f);
        drawQuadPath(cr, pTL, pTR, pBR, pBL);
        setCol(cr, padCol);
        cairo_fill(cr);

        // Corner L-brackets
        cairo_set_source_rgba(cr, 1, 1, 1, 0.34);
        cairo_set_line_width(cr, 0.95);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
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
            cairo_move_to(cr, ph.x, ph.y); cairo_line_to(cr, pt.x, pt.y); cairo_line_to(cr, pv.x, pv.y);
        }
        cairo_stroke(cr);

        // LED + 4 ExpressKeys
        RGBA eyeCol = eyePrimary(eyeTheme);
        RGBA ledCol = physics.tabletPenPress.value > 0.15f ? eyeCol : accentCol;
        Pt ledPt = CatPhysics::tabletQuadPoint(0.105f, 0.14f);
        cairo_set_source_rgba(cr, ledCol.r, ledCol.g, ledCol.b, 0.38);
        cairo_arc(cr, ledPt.x, ledPt.y, 3.6, 0, PI * 2.0); cairo_fill(cr);
        setCol(cr, ledCol);
        cairo_arc(cr, ledPt.x, ledPt.y, 1.35, 0, PI * 2.0); cairo_fill(cr);

        setCol(cr, keyCol);
        for (float kv : { 0.28f, 0.44f, 0.60f, 0.76f }) {
            drawQuadPath(cr,
                CatPhysics::tabletQuadPoint(0.06f, kv - 0.05f),
                CatPhysics::tabletQuadPoint(0.15f, kv - 0.05f),
                CatPhysics::tabletQuadPoint(0.15f, kv + 0.05f),
                CatPhysics::tabletQuadPoint(0.06f, kv + 0.05f));
            cairo_fill(cr);
        }

        // Fading ink trail
        if (physics.tabletTrail.size() >= 2) {
            cairo_save(cr);
            drawQuadPath(cr, pTL, pTR, pBR, pBL);
            cairo_clip(cr);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            for (size_t i = 1; i < physics.tabletTrail.size(); ++i) {
                const auto& a = physics.tabletTrail[i - 1];
                const auto& b = physics.tabletTrail[i];
                if (std::hypot(b.point.x - a.point.x, b.point.y - a.point.y) > 22.0f) continue;
                double life = std::max(0.0, 1.0 - b.age / 0.72);
                if (life <= 0.01) continue;
                cairo_set_source_rgba(cr, eyeCol.r, eyeCol.g, eyeCol.b, life * 0.88);
                cairo_set_line_width(cr, 1.2 + life * 1.4);
                cairo_move_to(cr, a.point.x, a.point.y);
                cairo_line_to(cr, b.point.x, b.point.y);
                cairo_stroke(cr);
            }
            cairo_restore(cr);
        }

        // Nib contact glow
        if (physics.leftPawOnMouse && physics.tabletPenPress.value > 0.08f) {
            Pt nib = physics.tabletNib.point();
            double r = 7.0 + physics.tabletPenPress.value * 3.5;
            cairo_pattern_t* pat = cairo_pattern_create_radial(nib.x, nib.y, 0, nib.x, nib.y, r);
            cairo_pattern_add_color_stop_rgba(pat, 0.0, eyeCol.r, eyeCol.g, eyeCol.b, 0.45 * physics.tabletPenPress.value);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, eyeCol.r, eyeCol.g, eyeCol.b, 0.0);
            cairo_set_source(cr, pat);
            cairo_arc(cr, nib.x, nib.y, r, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }
        cairo_restore(cr);
    }

    static void drawStylusBody(cairo_t* cr, double len, KeyboardTheme theme, EyeColorTheme eyeTheme, float pressed) {
        RGBA barrelCol, gripCol, ringCol;
        if (theme == KeyboardTheme::Graphite) {
            barrelCol = {0.18,0.18,0.20,1}; gripCol = {0.27,0.28,0.30,1}; ringCol = {0.40,0.41,0.43,1};
        } else if (theme == KeyboardTheme::MidnightHacker) {
            barrelCol = {0.13,0.14,0.17,1}; gripCol = {0.22,0.24,0.29,1}; ringCol = {0.78,0.95,0.25,1};
        } else if (theme == KeyboardTheme::RetroCream) {
            barrelCol = {0.28,0.26,0.24,1}; gripCol = {0.42,0.39,0.35,1}; ringCol = {0.88,0.42,0.24,1};
        } else {
            barrelCol = {0.18,0.19,0.23,1}; gripCol = {0.28,0.30,0.35,1}; ringCol = {0.96,0.69,0.16,1};
        }
        auto tracePen = [&]() {
            cairo_new_path(cr);
            cairo_move_to(cr, 0.0, 0.0);
            cairo_line_to(cr, 7.5, -2.4);
            cairo_line_to(cr, len - 3.0, -2.6);
            quadCurveTo(cr, len - 3.0, -2.6, len, -2.4, len, 0.0);
            quadCurveTo(cr, len, 0.0, len, 2.4, len - 3.0, 2.6);
            cairo_line_to(cr, 7.5, 2.4);
            cairo_close_path(cr);
        };
        tracePen();
        cairo_set_source_rgba(cr, 0.96, 0.96, 0.96, 0.80);
        cairo_set_line_width(cr, 1.6);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_stroke(cr);

        tracePen();
        setCol(cr, barrelCol);
        cairo_fill(cr);

        cairo_save(cr);
        tracePen();
        cairo_clip(cr);
        setCol(cr, gripCol);
        cairo_rectangle(cr, 7.5, -3.0, 11.5, 6.0); cairo_fill(cr);
        setCol(cr, ringCol);
        cairo_rectangle(cr, 19.0, -3.0, 2.4, 6.0); cairo_fill(cr);
        if (pressed > 0.15f) setCol(cr, eyePrimary(eyeTheme));
        else cairo_set_source_rgba(cr, 0.94, 0.94, 0.94, 1.0);
        cairo_rectangle(cr, -0.5, -1.5, 3.4, 3.0); cairo_fill(cr);
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.24);
        cairo_rectangle(cr, 8.0, -1.9, std::max(1.0, len - 12.0), 1.1); cairo_fill(cr);
        cairo_restore(cr);

        roundedRect(cr, 11.0, -3.1, 6.0, 1.4, 0.7);
        cairo_set_source_rgba(cr, 0.78, 0.78, 0.78, 0.90);
        cairo_fill(cr);
    }

    static void drawParkedStylus(cairo_t* cr, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        Pt nib = CatPhysics::tabletQuadPoint(0.88f, -0.02f);
        Pt tail = CatPhysics::tabletQuadPoint(0.18f, -0.02f);
        double ang = std::atan2(tail.y - nib.y, tail.x - nib.x);
        double len = std::hypot(tail.x - nib.x, tail.y - nib.y);

        cairo_save(cr);
        cairo_translate(cr, nib.x, nib.y + 2.2);
        cairo_rotate(cr, ang);
        cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
        cairo_rectangle(cr, 2.0, -2.0, len - 4.0, 4.5);
        cairo_fill(cr);
        cairo_restore(cr);

        cairo_save(cr);
        cairo_translate(cr, nib.x, nib.y);
        cairo_rotate(cr, ang);
        drawStylusBody(cr, len, theme, eyeTheme, 0.0f);
        cairo_restore(cr);
    }

    static void drawHeldStylus(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme) {
        Pt lPos = physics.leftPaw.point();
        float press = std::clamp(physics.tabletPenPress.value, 0.0f, 1.0f);
        Pt nib = { lPos.x - 5.5f, lPos.y + 7.0f + press * 1.4f };
        double angle = -2.18 + physics.tabletPenTilt.value;

        cairo_save(cr);
        cairo_translate(cr, nib.x, nib.y);
        cairo_rotate(cr, angle);
        drawStylusBody(cr, 35.0, theme, eyeTheme, press);
        cairo_restore(cr);
    }

    // MARK: - Mini 65% Mechanical Keyboard (180° facing the cat)

    static void drawQuadPath(cairo_t* cr, Pt tl, Pt tr, Pt br, Pt bl) {
        cairo_new_path(cr);
        cairo_move_to(cr, tl.x, tl.y);
        cairo_line_to(cr, tr.x, tr.y);
        cairo_line_to(cr, br.x, br.y);
        cairo_line_to(cr, bl.x, bl.y);
        cairo_close_path(cr);
    }

    static std::string keycapLegend(PhysicalKey k) {
        switch (k) {
            case PhysicalKey::Escape: return "ESC";
            case PhysicalKey::Tab: return "⇥";
            case PhysicalKey::CapsLock: return "⇪";
            case PhysicalKey::LeftShift:
            case PhysicalKey::RightShift: return "⇧";
            case PhysicalKey::LeftControl:
            case PhysicalKey::RightControl: return "Ctrl";
            case PhysicalKey::LeftOption:
            case PhysicalKey::RightOption: return "Alt";
            case PhysicalKey::LeftCommand:
            case PhysicalKey::RightCommand: return "Super";
            case PhysicalKey::Backspace: return "⌫";
            case PhysicalKey::Enter: return "↵";
            case PhysicalKey::Space: return "";
            default: return keyBadgeLabel(k);
        }
    }

    static void drawKeyboard(cairo_t* cr, const CatPhysics& physics, KeyboardTheme theme, EyeColorTheme eyeTheme, bool showLegends, bool marquee) {
        Pt tl = KeyboardLayout::screenQuadPoint(-0.012f, -0.04f);
        Pt tr = KeyboardLayout::screenQuadPoint(1.012f, -0.04f);
        Pt bl = KeyboardLayout::screenQuadPoint(-0.016f, 1.04f);
        Pt br = KeyboardLayout::screenQuadPoint(1.016f, 1.04f);
        constexpr float depth = 4.5f;

        drawQuadPath(cr, {tl.x, tl.y + depth + 2}, {tr.x, tr.y + depth + 2},
                         {br.x, br.y + depth + 3}, {bl.x, bl.y + depth + 3});
        cairo_set_source_rgba(cr, 0, 0, 0, 0.22);
        cairo_fill(cr);

        RGBA baseCol, deckCol, keyCol, borderCol, accentCol;
        if (theme == KeyboardTheme::Graphite) {
            baseCol = {0.09, 0.09, 0.10, 1}; deckCol = {0.14, 0.14, 0.15, 1};
            keyCol = {0.27, 0.28, 0.30, 1}; borderCol = {0.15, 0.15, 0.16, 0.95}; accentCol = keyCol;  // no accent keys
        } else if (theme == KeyboardTheme::MidnightHacker) {
            baseCol = {0.06, 0.06, 0.08, 1}; deckCol = {0.10, 0.11, 0.13, 1};
            keyCol = {0.21, 0.22, 0.25, 1}; borderCol = {0.11, 0.11, 0.13, 0.95}; accentCol = {0.77, 0.94, 0.25, 1};
        } else if (theme == KeyboardTheme::RetroCream) {
            baseCol = {0.25, 0.24, 0.22, 1}; deckCol = {0.38, 0.36, 0.33, 1};
            keyCol = {0.90, 0.88, 0.82, 1}; borderCol = {0.68, 0.65, 0.60, 0.85}; accentCol = {0.88, 0.42, 0.24, 1};
        } else {
            baseCol = {0.10, 0.10, 0.12, 1}; deckCol = {0.14, 0.14, 0.16, 1};
            keyCol = {0.968, 0.964, 0.942, 1}; borderCol = {0.78, 0.77, 0.74, 0.75}; accentCol = {0.97, 0.70, 0.16, 1};
        }

        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        drawQuadPath(cr, {tl.x, tl.y + depth}, {tr.x, tr.y + depth}, {br.x, br.y + depth}, {bl.x, bl.y + depth});
        setCol(cr, baseCol);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 2.5);
        cairo_stroke(cr);

        drawQuadPath(cr, tl, tr, br, bl);
        setCol(cr, deckCol);
        cairo_fill_preserve(cr);
        cairo_set_line_width(cr, 2.0);
        cairo_stroke(cr);

        constexpr float padU = 0.0028f;
        constexpr float padV = 0.014f;
        RGBA glowCol = eyePrimary(eyeTheme);

        // Marquee backlight: a comet of light sweeps diagonally under the keycaps.
        float mqHead = physics.marqueePhase - std::floor(physics.marqueePhase);
        RGBA mqCol = hsvColor(mqHead + 0.52f, 0.55f, 1.0f);
        auto mqWave = [mqHead, marquee](float t) {
            if (!marquee) return 0.0f;
            float d = mqHead - t;
            if (d < 0.0f) d += 1.0f;
            return std::exp(-d / 0.125f);
        };
        float bandDx = br.x - tl.x, bandDy = br.y - tl.y;
        float bandLen2 = bandDx * bandDx + bandDy * bandDy;
        if (marquee) {
            cairo_save(cr);
            drawQuadPath(cr, tl, tr, br, bl);
            cairo_clip(cr);
            cairo_pattern_t* band = cairo_pattern_create_linear(tl.x, tl.y, br.x, br.y);
            for (int i = 0; i < 24; ++i) {
                float t = (float)i / 23.0f;
                cairo_pattern_add_color_stop_rgba(band, t, mqCol.r, mqCol.g, mqCol.b,
                                                  std::min(1.0f, mqWave(t) * 0.80f));
            }
            cairo_set_source(cr, band);
            cairo_paint(cr);
            cairo_pattern_destroy(band);
            // White-hot lamp at the head of the comet.
            Pt head = {tl.x + bandDx * mqHead, tl.y + bandDy * mqHead};
            double hr = 44.0;
            cairo_pattern_t* lamp = cairo_pattern_create_radial(head.x, head.y, 0, head.x, head.y, hr);
            cairo_pattern_add_color_stop_rgba(lamp, 0, 0.94, 0.99, 1.0, 0.58);
            cairo_pattern_add_color_stop_rgba(lamp, 1, mqCol.r, mqCol.g, mqCol.b, 0.0);
            cairo_set_source(cr, lamp);
            cairo_arc(cr, head.x, head.y, hr, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(lamp);
            cairo_restore(cr);
        }

        for (const auto& cap : KeyboardLayout::keycaps()) {
            float glow = 0.0f;
            auto it = physics.keyGlow.find(cap.key);
            if (it != physics.keyGlow.end()) glow = it->second;

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
            double kcx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25;
            double kcy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25;
            float kt = std::clamp((float)(((kcx - tl.x) * bandDx + (kcy - tl.y) * bandDy) / bandLen2), 0.0f, 1.0f);
            float mq = mqWave(kt);

            drawQuadPath(cr, kTL, kTR, kBR, kBL);
            RGBA fill = cap.isAmberAccent ? accentCol : keyCol;
            if (mq > 0.015f) {
                // Head of the comet reads hot (near white), the trailing keys stay saturated.
                RGBA lit = blend(mqCol, {1.0, 1.0, 1.0, 1.0}, mq * mq * 0.5f);
                fill = blend(fill, lit, std::min(0.9f, mq * 0.88f));
            }
            if (glow > 0.02f) fill = blend(fill, glowCol, glow * 0.55);

            setCol(cr, fill);
            cairo_fill_preserve(cr);
            if (mq > 0.10f) {
                cairo_set_source_rgba(cr, mqCol.r, mqCol.g, mqCol.b, std::min(0.84f, mq * 0.84f));
                cairo_set_line_width(cr, 1.0);
            } else {
                setCol(cr, borderCol);
                cairo_set_line_width(cr, 0.65);
            }
            cairo_stroke(cr);

            // Under-key bloom where the beam is hottest, so the light spills between caps.
            if (mq > 0.45f) {
                double bloom = (mq - 0.45) / 0.55;
                cairo_save(cr);
                cairo_translate(cr, kcx, kcy);
                cairo_scale(cr, 15.0, 10.5);
                cairo_pattern_t* pb = cairo_pattern_create_radial(0, 0, 0, 0, 0, 1);
                cairo_pattern_add_color_stop_rgba(pb, 0, mqCol.r, mqCol.g, mqCol.b, 0.53 * bloom);
                cairo_pattern_add_color_stop_rgba(pb, 1, mqCol.r, mqCol.g, mqCol.b, 0.0);
                cairo_set_source(cr, pb);
                cairo_arc(cr, 0, 0, 1, 0, PI * 2.0);
                cairo_fill(cr);
                cairo_pattern_destroy(pb);
                cairo_restore(cr);
            }

            if (showLegends) {
                bool isSpace = (cap.key == PhysicalKey::Space);
                std::string legend = isSpace ? std::string("space") : keycapLegend(cap.key);
                if (!legend.empty()) {
                    double cx = (kTL.x + kTR.x + kBR.x + kBL.x) * 0.25;
                    double cy = (kTL.y + kTR.y + kBR.y + kBL.y) * 0.25;
                    if (isSpace) {
                        // The spacebar sits right under the cat: its legs hide the middle, and an
                        // unlabeled visible end looked like an odd blank patch beside the cat.
                        // Label the end that stays visible instead.
                        Pt sp = KeyboardLayout::screenQuadPoint(su0 + (su1 - su0) * 0.82f, (sv0 + sv1) * 0.5f);
                        cx = sp.x;
                        cy = sp.y + dy;
                    }
                    if (theme != KeyboardTheme::Graphite && (cap.isAmberAccent || theme != KeyboardTheme::MidnightHacker)) {
                        cairo_set_source_rgba(cr, 0.14, 0.14, 0.16, 0.78);
                    } else {
                        cairo_set_source_rgba(cr, 0.88, 0.90, 0.92, 0.82);
                    }
                    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                    cairo_set_font_size(cr, legend.size() <= 1 ? 5.2 : 3.8);
                    cairo_text_extents_t ext;
                    cairo_text_extents(cr, legend.c_str(), &ext);
                    cairo_save(cr);
                    cairo_translate(cr, cx, cy);
                    // Keyboard faces the cat (180°), so legends read upright from the cat's seat.
                    cairo_rotate(cr, 3.14159265358979 + 0.052);
                    cairo_move_to(cr, -(ext.width * 0.5 + ext.x_bearing), -(ext.height * 0.5 + ext.y_bearing));
                    cairo_show_text(cr, legend.c_str());
                    cairo_restore(cr);
                }
            }
        }

        // Backlit keycap glow halos
        for (const auto& kv : physics.keyGlow) {
            if (kv.second <= 0.05f) continue;
            Pt c = KeyboardLayout::canvasPointForKey(kv.first);
            double r = 17.0 + kv.second * 6.0;
            cairo_pattern_t* pat = cairo_pattern_create_radial(c.x, c.y, 0, c.x, c.y, r);
            cairo_pattern_add_color_stop_rgba(pat, 0.0, glowCol.r, glowCol.g, glowCol.b, 0.36 * kv.second);
            cairo_pattern_add_color_stop_rgba(pat, 1.0, glowCol.r, glowCol.g, glowCol.b, 0.0);
            cairo_set_source(cr, pat);
            cairo_arc(cr, c.x, c.y, r, 0, PI * 2.0);
            cairo_fill(cr);
            cairo_pattern_destroy(pat);
        }
    }

    // MARK: - Tail

    // Tail centreline (canvas space, doubles exactly as the pre-SkinSpec renderer computed them).
    struct TailCurve {
        double x0, y0, x1, y1, x2, y2, x3, y3, thickness;
        struct P { double x, y; };
        // Tail space: x = 0 (root) .. 100 (tip) along the curve, y = offset along the left-hand normal.
        P at(SkinPoint p) const {
            double t = std::clamp((double)p.x / 100.0, 0.0, 1.0);
            double u=1-t,x=u*u*u*x0+3*u*u*t*x1+3*u*t*t*x2+t*t*t*x3;
            double y=u*u*u*y0+3*u*u*t*y1+3*u*t*t*y2+t*t*t*y3;
            double dx=3*u*u*(x1-x0)+6*u*t*(x2-x1)+3*t*t*(x3-x2);
            double dy=3*u*u*(y1-y0)+6*u*t*(y2-y1)+3*t*t*(y3-y2);
            double len=std::max(0.001,std::hypot(dx,dy)),n=(double)p.y*thickness;
            return {x+-dy/len*n,y+dx/len*n};
        }
    };
    static TailCurve tailCurve(float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin) {
        float p = physics.tailPhase;
        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        float agility = std::clamp(physics.tailAgility.value, 0.0f, 1.0f);
        float wag1 = std::sin(p) * (8.0f + hunt * 5.5f + agility * 4.0f);
        float wag2 = std::cos(p * 1.3f) * (6.0f + hunt * 4.5f + agility * 3.5f);
        float huntDrop = hunt * 14.0f;
        double reach = std::clamp(physics.tailTyping.value, 0.0f, 1.0f);
        double lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        double lieS = lie * lie * (3.0 - 2.0 * lie);
        double flickScale = (1.0 - reach) * (1.0 - lieS);
        Pt flick = physics.tailFlick.point();
        double fx = flick.x * flickScale;
        double fy = flick.y * flickScale;
        double curl = physics.tailCurl.value * flickScale;
        Pt target = physics.tailTip.point();

        double bw = skin.shape.bodyWidth;
        double rootShift = 34.0 * bw - 34.0; // tail root stays on the (wider/narrower) hip
        double tl = skin.shape.tailLength;
        TailCurve c;
        c.x0 = seatX + 34.0 * bw;                                              c.y0 = seatY - 6.0;
        c.x1 = seatX + 64.0 + rootShift + 12.0 * reach + fx * 0.25;            c.y1 = seatY + 2.0 + wag1 * 0.35 + huntDrop * 0.4 - 18.0 * reach + fy * 0.22 + curl * 0.35;
        c.x2 = seatX + 66.0 + wag2 * 0.4 + rootShift + fx * 0.65 - curl * 0.45; c.y2 = seatY - 26.0 + wag1 * 0.7 + huntDrop + fy * 0.60 - curl * 0.55;
        c.x3 = seatX + 91.0 + wag2 * 0.5 + rootShift + fx;                     c.y3 = seatY - 30.0 + wag1 * 0.85 + huntDrop + fy;
        c.x1 = around(c.x1, c.x0, tl); c.y1 = around(c.y1, c.y0, tl);
        c.x2 = around(c.x2, c.x0, tl); c.y2 = around(c.y2, c.y0, tl);
        c.x3 = around(c.x3, c.x0, tl); c.y3 = around(c.y3, c.y0, tl);
        c.x2 += (target.x + 24.0 - c.x2) * reach; c.y2 += (target.y - 30.0 - c.y2) * reach;
        c.x3 += (target.x - c.x3) * reach;        c.y3 += (target.y - c.y3) * reach;
        double sleepWag = std::sin(p * 0.6f) * 2.0;
        c.x0 += (seatX + 40.0 * bw - c.x0) * lieS; c.y0 += (seatY - 2.0 - c.y0) * lieS;
        c.x1 += (seatX + 54.0 * bw - c.x1) * lieS; c.y1 += (seatY + 13.0 - c.y1) * lieS;
        c.x2 += (seatX + 14.0 * bw - c.x2) * lieS; c.y2 += (seatY + 17.0 - c.y2) * lieS;
        c.x3 += (seatX - 28.0 * bw - c.x3) * lieS; c.y3 += (seatY + 8.0 + sleepWag - c.y3) * lieS;
        c.thickness = skin.shape.tailThickness;
        return c;
    }

    static void drawTail(cairo_t* cr, float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        TailCurve c = tailCurve(seatX, seatY, physics, skin);
        bool art = !outlineOnly && (skin.hasPartArt(SkinSpace::Tail) || skin.palette.tailTip.set);
        double width = buildOf(skin).tailWidth * skin.shape.tailThickness;
        double lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        double lieS = lie * lie * (3.0 - 2.0 * lie);

        cairo_save(cr);
        if (art) cairo_push_group(cr);
        if (outlineOnly) setCol(cr, blackOutline);
        else setCol(cr, skin.palette.tail);
        cairo_set_line_width(cr, outlineOnly ? (width + BlackOutlineW * 2.0) : width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
        cairo_move_to(cr, c.x0, c.y0);
        cairo_curve_to(cr, c.x1, c.y1, c.x2, c.y2, c.x3, c.y3);
        cairo_stroke(cr);
        if (art) {
            // Everything below lands only on the tail's own pixels.
            cairo_set_operator(cr,CAIRO_OPERATOR_ATOP);
            auto map = [&](SkinPoint p) { return c.at(p); };
            for (const SkinRegion& r : skin.regions) if (r.space == SkinSpace::Tail) drawRegion(cr, r, map);
            if (skin.palette.tailTip.set) {
                // Last 22% of the tail: de Casteljau split at t = 0.78, stroked with the tail width.
                const double t = 0.78, u = 1 - t;
                double ax = u*c.x0+t*c.x1, ay = u*c.y0+t*c.y1, bx = u*c.x1+t*c.x2, by = u*c.y1+t*c.y2, qx = u*c.x2+t*c.x3, qy = u*c.y2+t*c.y3;
                double dx = u*ax+t*bx, dy = u*ay+t*by, ex = u*bx+t*qx, ey = u*by+t*qy;
                double sx = u*dx+t*ex, sy = u*dy+t*ey;
                cairo_new_path(cr);
                cairo_move_to(cr, sx, sy);
                cairo_curve_to(cr, ex, ey, qx, qy, c.x3, c.y3);
                setCol(cr, skin.palette.tailTip);
                cairo_set_line_width(cr, width + 2.0);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_stroke(cr);
            }
            for (const SkinMark& m : skin.marks) if (m.space == SkinSpace::Tail) drawMark(cr, m, map, CAIRO_LINE_CAP_BUTT);
            cairo_pop_group_to_source(cr);cairo_set_operator(cr,CAIRO_OPERATOR_OVER);cairo_paint(cr);
        }
        if (!outlineOnly && lieS > 0.35) {
            double rAlpha = std::min(1.0, (lieS - 0.35) / 0.45) * 0.72;
            setCol(cr, skin.palette.crease, rAlpha);
            cairo_set_line_width(cr, 1.45);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, c.x0 - 4.0, c.y0 - 3.0);
            cairo_curve_to(cr, c.x1 - 6.0, c.y1 - 5.5, c.x2, c.y2 - 5.5, c.x3 + 2.0, c.y3 - 4.5);
            cairo_stroke(cr);
        }
        cairo_restore(cr);
    }

    // MARK: - 3D-Projected Ears

    static void drawEarsLayer(cairo_t* cr, Pt center, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        cairo_save(cr);
        cairo_translate(cr, center.x, center.y);
        cairo_rotate(cr, physics.headTilt.value);
        if (skin.shape.headScale != 1.0f) cairo_scale(cr, skin.shape.headScale, skin.shape.headScale);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;
        float fold = physics.earFold.value;
        float twitch = physics.earTwitch.value * 0.045f;

        drawEar3D(cr, true, yawRad, pitchRad, fold, twitch, skin, outlineOnly);
        drawEar3D(cr, false, yawRad, pitchRad, fold, -twitch, skin, outlineOnly);
        cairo_restore(cr);
    }

    static void drawEar3D(cairo_t* cr, bool isLeft, float yawRad, float pitchRad, float fold, float twitch, const SkinSpec& skin, bool outlineOnly = false) {
        const Build& b = buildOf(skin);
        float sign = isLeft ? -1.0f : 1.0f;
        float foldAngle = sign * (b.earFold + fold * 0.34f) + twitch;
        float earLen = b.earLen;
        float tipDx = std::sin(foldAngle) * earLen;
        float tipDy = -std::cos(foldAngle) * earLen;

        Pt inB  = projectHead3D(sign * b.earInX, b.earInY, 0.0f, yawRad, pitchRad);
        Pt outB = projectHead3D(sign * b.earOutX, b.earOutY, -2.0f, yawRad, pitchRad);
        Pt tip  = projectHead3D(sign * b.earTipX + tipDx, b.earTipY + tipDy, -3.0f, yawRad, pitchRad);
        float es = skin.shape.earSize;
        if (es != 1.0f) { // grow / shrink around the middle of the ear base
            Pt m = { (inB.x + outB.x) * 0.5f, (inB.y + outB.y) * 0.5f };
            inB = { around(inB.x, m.x, es), around(inB.y, m.y, es) };
            outB = { around(outB.x, m.x, es), around(outB.y, m.y, es) };
            tip = { around(tip.x, m.x, es), around(tip.y, m.y, es) };
        }
        float ep = skin.shape.earPointiness;
        float tipSpread = b.earTipSpread / ep;
        Pt tipL = { tip.x - sign * tipSpread, tip.y + 2.8f };
        Pt tipR = { tip.x + sign * tipSpread, tip.y + 3.2f };

        auto earPath = [&]() {
            cairo_new_path(cr);
            cairo_move_to(cr, inB.x, inB.y);
            quadCurveTo(cr, inB.x, inB.y, (inB.x + tipL.x) * 0.5 - sign * 1.5, (inB.y + tipL.y) * 0.5 - 2.0, tipL.x, tipL.y);
            quadCurveTo(cr, tipL.x, tipL.y, tip.x, tip.y - 2.2 * ep, tipR.x, tipR.y);
            quadCurveTo(cr, tipR.x, tipR.y, (outB.x + tipR.x) * 0.5 + sign * 2.5, (outB.y + tipR.y) * 0.5 - 2.0, outB.x, outB.y);
            cairo_close_path(cr);
        };
        earPath();
        if (outlineOnly) {
            setCol(cr, blackOutline);
            cairo_set_line_width(cr, BlackOutlineW * 2.0);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_stroke(cr);
            return;
        }
        setCol(cr, skin.palette.earOuter);
        cairo_fill(cr);

        if (skin.palette.earInner.set) {
            Pt inI  = { inB.x + (tip.x - inB.x) * 0.24f + sign * 3.0f, inB.y + (tip.y - inB.y) * 0.16f };
            Pt outI = { outB.x + (tip.x - outB.x) * 0.20f - sign * 3.8f, outB.y + (tip.y - outB.y) * 0.18f };
            Pt tipI = { (inB.x + outB.x) * 0.16f + tip.x * 0.68f, (inB.y + outB.y) * 0.14f + tip.y * 0.72f + 2.5f };
            cairo_new_path(cr);
            cairo_move_to(cr, inI.x, inI.y);
            quadCurveTo(cr, inI.x, inI.y, (inI.x + tipI.x) * 0.5 - sign * 0.8, (inI.y + tipI.y) * 0.5, tipI.x, tipI.y);
            quadCurveTo(cr, tipI.x, tipI.y, (outI.x + tipI.x) * 0.5 + sign * 1.2, (outI.y + tipI.y) * 0.5, outI.x, outI.y);
            cairo_close_path(cr);
            setCol(cr, skin.palette.earInner);
            cairo_fill(cr);
        }

        SkinSpace space = isLeft ? SkinSpace::LeftEar : SkinSpace::RightEar;
        if (skin.hasPartArt(space)) {
            // Ear space: (-50,0) inner base, (50,0) outer base, (0,-100) tip; mirrored per ear.
            Pt m = { (inB.x + outB.x) * 0.5f, (inB.y + outB.y) * 0.5f };
            auto map = [&](SkinPoint p) -> Pt {
                float u = p.x / 100.0f, v = -p.y / 100.0f;
                return { m.x + u * (outB.x - inB.x) + v * (tip.x - m.x), m.y + u * (outB.y - inB.y) + v * (tip.y - m.y) };
            };
            cairo_save(cr);
            earPath();
            cairo_clip(cr);
            for (const SkinRegion& r : skin.regions) if (r.space == space) drawRegion(cr, r, map);
            for (const SkinMark& mk : skin.marks) if (mk.space == space) drawMark(cr, mk, map);
            cairo_restore(cr);
        }
    }

    // MARK: - Unified C1-Continuous Cat Silhouette (+ regions, colour-point mask, marks)

    static void drawUnifiedSilhouette(cairo_t* cr, float seatX, float seatY, Pt headCenter, float breath, const CatPhysics& physics, const SkinSpec& skin, bool outlineOnly = false) {
        const Build& b = buildOf(skin);
        const SkinPalette& pal = skin.palette;
        float yaw = physics.headYaw.value;
        float pitch = physics.headPitch.value;
        float hTilt = physics.headTilt.value;
        float bTilt = physics.bodyTilt.value;
        float hs = skin.shape.headScale, bw = skin.shape.bodyWidth;

        float cH = std::cos(hTilt), sH = std::sin(hTilt);
        auto H = [&](float lx, float ly) -> Pt {
            lx *= hs; ly *= hs;
            return { headCenter.x + lx * cH - ly * sH, headCenter.y + lx * sH + ly * cH };
        };

        float cB = std::cos(bTilt), sB = std::sin(bTilt);
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float sy = physics.bodySquash.value * (1.0f - lieS * 0.42f);
        float sx = (1.0f + (1.0f - physics.bodySquash.value) * 0.55f) * (1.0f + lieS * 0.03f);
        auto B = [&](float lx, float ly) -> Pt {
            float rx = lx * bw * sx, ry = ly * sy;
            return { seatX + rx * cB - ry * sB, seatY + rx * sB + ry * cB };
        };

        float cheekShift = yaw * 3.0f;
        float crownDrop = pitch * 2.0f;
        float cheekHalfW = b.cheekHalfW * skin.shape.cheekFullness;
        float crownCtrlW = b.crownCtrlW;
        float crownBaseY = b.crownBaseY;
        float cheekY = b.cheekY;
        float cheekCtrlTopY = b.cheekCtrlTopY;
        float neckPinchX = b.neckPinchX;
        float neckCtrlY = b.neckCtrlY;
        float hipHalfW = b.hipHalfW;
        float waistCtrlW = b.waistCtrlW;
        float waistCtrlY = b.waistCtrlY;
        float botOuterW = b.botOuterW;
        float botInnerW = b.botInnerW;
        float botHalfW = b.botHalfW;
        float lCX = -cheekHalfW + cheekShift * 0.4f;
        float rCX =  cheekHalfW + cheekShift * 0.4f;

        Pt crown  = H(cheekShift * 0.2f, crownBaseY + crownDrop);
        Pt rCheek = H(rCX, cheekY);
        Pt rHip   = B(hipHalfW, -14.0f);
        Pt rBot   = B(botHalfW, 12.0f);
        Pt lBot   = B(-botHalfW, 12.0f);
        Pt lHip   = B(-hipHalfW, -14.0f);
        Pt lCheek = H(lCX, cheekY);

        auto buildPath = [&]() {
            Pt c1, c2;
            cairo_new_path(cr);
            cairo_move_to(cr, crown.x, crown.y);
            c1 = H(crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop); c2 = H(rCX + 1.5f, cheekCtrlTopY);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rCheek.x, rCheek.y);
            c1 = H(rCX - neckPinchX, neckCtrlY); c2 = B(waistCtrlW, waistCtrlY + breath * 0.5f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rHip.x, rHip.y);
            c1 = B(botOuterW, 5.0f); c2 = B(botInnerW, 12.0f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rBot.x, rBot.y);
            cairo_line_to(cr, lBot.x, lBot.y);
            c1 = B(-botInnerW, 12.0f); c2 = B(-botOuterW, 5.0f);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, lHip.x, lHip.y);
            c1 = B(-waistCtrlW, waistCtrlY + breath * 0.5f); c2 = H(lCX + neckPinchX, neckCtrlY);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, lCheek.x, lCheek.y);
            c1 = H(lCX - 1.5f, cheekCtrlTopY); c2 = H(-crownCtrlW + cheekShift * 0.3f, crownBaseY + crownDrop);
            cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, crown.x, crown.y);
            cairo_close_path(cr);
        };

        if (outlineOnly) {
            buildPath();
            setCol(cr, blackOutline);
            cairo_set_line_width(cr, BlackOutlineW * 2.0);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_stroke(cr);
            cairo_save(cr);
            cairo_translate(cr, seatX, seatY);
            cairo_rotate(cr, bTilt);
            cairo_scale(cr, sx, sy);
            double stagger = skin.shape.pawStagger;
            double leftHindCX = -b.hindCX * bw;
            double rightHindCX = b.hindCX * bw;
            double rightHindCY = b.hindLeftCY + (b.hindStaggerCY - b.hindLeftCY) * stagger;
            double rightHindRY = b.hindLeftRY + (b.hindStaggerRY - b.hindLeftRY) * stagger;
            cairo_new_path(cr);
            fillEllipse(cr, leftHindCX, b.hindLeftCY, b.hindRX, b.hindLeftRY);
            cairo_stroke(cr);
            if (physics.hindFootActive.value < 0.35f) {
                cairo_new_path(cr);
                fillEllipse(cr, rightHindCX, rightHindCY, b.hindRX, rightHindRY);
                cairo_stroke(cr);
            }
            cairo_restore(cr);
            return;
        }

        bool hasRegions = false, hasMarks = false;
        for (const SkinRegion& r : skin.regions) hasRegions |= r.space == SkinSpace::Head || r.space == SkinSpace::Skull || r.space == SkinSpace::Body;
        for (const SkinMark& m : skin.marks) hasMarks |= m.space == SkinSpace::Head || m.space == SkinSpace::Skull || m.space == SkinSpace::Body;
        auto headMap = [&](SkinPoint p) { return headPoint(p, false, skin, headCenter, physics); };
        auto skullMap = [&](SkinPoint p) { return headPoint(p, true, skin, headCenter, physics); };
        auto bodyMap = [&](SkinPoint p) { return bodyPoint(p, skin, seatX, seatY, physics); };

        if (!pal.bodyShade.set) {
            buildPath();
            setCol(cr, pal.body);
            cairo_fill(cr);
            if (isBuiltInOrangeWhite(skin)) {
                cairo_save(cr); cairo_new_path(cr); cairo_move_to(cr, lCheek.x, lCheek.y);
                Pt c1 = H(lCX - 1.5f, -14.0f), c2 = H(-26.0f + cheekShift * 0.3f, -30.0f + crownDrop);
                cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, crown.x, crown.y);
                c1 = H(26.0f + cheekShift * 0.3f, -30.0f + crownDrop); c2 = H(rCX + 1.5f, -14.0f);
                cairo_curve_to(cr, c1.x, c1.y, c2.x, c2.y, rCheek.x, rCheek.y);
                setCol(cr, pal.earOuter); cairo_set_line_width(cr, 1.0); cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND); cairo_stroke(cr); cairo_restore(cr);
            }
        }
        if (pal.bodyShade.set || skin.colorpoint() || hasRegions || hasMarks || isBuiltInPatterned(skin)) {
            cairo_save(cr);
            buildPath();
            cairo_clip(cr);

            if (pal.bodyShade.set) {
                cairo_pattern_t* bg = cairo_pattern_create_linear(seatX, headCenter.y - 34.0, seatX, seatY + 18.0);
                cairo_pattern_add_color_stop_rgba(bg, 0.15, pal.body.r, pal.body.g, pal.body.b, 1.0);
                cairo_pattern_add_color_stop_rgba(bg, 1.0,  pal.bodyShade.r, pal.bodyShade.g, pal.bodyShade.b, 1.0);
                cairo_set_source(cr, bg);
                cairo_paint(cr);
                cairo_pattern_destroy(bg);
            }

            // Curve regions: head/skull above the jaw line, body below it.
            for (const SkinRegion& r : skin.regions) {
                if (r.space != SkinSpace::Head && r.space != SkinSpace::Skull && r.space != SkinSpace::Body) continue;
                cairo_save(cr);
                jawSide(cr, r.space != SkinSpace::Body, skin, headCenter, physics);
                cairo_clip(cr);
                if (r.space == SkinSpace::Body) drawRegion(cr, r, bodyMap);
                else if (r.space == SkinSpace::Skull) drawRegion(cr, r, skullMap);
                else drawRegion(cr, r, headMap);
                cairo_restore(cr);
            }

            if (skin.colorpoint()) {
                // 2.5D Rotating Seal-Point "挖煤" Face Mask (no center hole!)
                const SkinColor& point = pal.points;
                const SkinColor& mid = pal.pointsMid;
                float yawRad = yaw * 0.50f;
                float pitchRad = pitch * 0.36f;
                Pt maskC = projectHead3D(0.0f, 3.0f, 27.0f, yawRad, pitchRad);
                double maskSX = std::max(0.76, std::cos((double)yawRad * 0.7)) * 1.24;
                double maskSY = std::max(0.82, std::cos((double)pitchRad * 0.7)) * 0.88;

                cairo_save(cr);
                cairo_translate(cr, headCenter.x, headCenter.y);
                cairo_rotate(cr, hTilt);
                if (hs != 1.0f) cairo_scale(cr, hs, hs);
                cairo_translate(cr, maskC.x, maskC.y);
                cairo_scale(cr, maskSX, maskSY);

                cairo_pattern_t* mp = cairo_pattern_create_radial(0, 0, 0.0, 0, 0, 31.5);
                cairo_pattern_add_color_stop_rgba(mp, 0.0,  point.r, point.g, point.b, 1.0);
                cairo_pattern_add_color_stop_rgba(mp, 0.42, point.r, point.g, point.b, 0.92);
                cairo_pattern_add_color_stop_rgba(mp, 0.74, mid.r, mid.g, mid.b, 0.62);
                cairo_pattern_add_color_stop_rgba(mp, 1.0,  mid.r, mid.g, mid.b, 0.0);
                cairo_set_source(cr, mp);
                cairo_arc(cr, 0, 0, 31.5, 0, PI * 2.0);
                cairo_fill(cr);
                cairo_pattern_destroy(mp);
                cairo_restore(cr);

                Pt muzC = projectHead3D(0.0f, 7.5f, 28.0f, yawRad, pitchRad);
                cairo_save(cr);
                cairo_translate(cr, headCenter.x, headCenter.y);
                cairo_rotate(cr, hTilt);
                if (hs != 1.0f) cairo_scale(cr, hs, hs);
                cairo_translate(cr, muzC.x, muzC.y);
                cairo_scale(cr, std::max(0.76, std::cos((double)yawRad * 0.7)) * 1.04, std::max(0.82, std::cos((double)pitchRad * 0.7)) * 0.76);
                cairo_pattern_t* mp2 = cairo_pattern_create_radial(0, 0, 0.0, 0, 0, 19.5);
                cairo_pattern_add_color_stop_rgba(mp2, 0.0,  point.r, point.g, point.b, 1.0);
                cairo_pattern_add_color_stop_rgba(mp2, 0.50, point.r, point.g, point.b, 0.85);
                cairo_pattern_add_color_stop_rgba(mp2, 1.0,  point.r, point.g, point.b, 0.0);
                cairo_set_source(cr, mp2);
                cairo_arc(cr, 0, 0, 19.5, 0, PI * 2.0);
                cairo_fill(cr);
                cairo_pattern_destroy(mp2);
                cairo_restore(cr);
            }

            if (isBuiltInPatterned(skin)) {
                drawBuiltInCoatMarks(cr, seatX, seatY, headCenter, physics, isBuiltInOrangeWhite(skin));
            } else {
                for (const SkinMark& m : skin.marks) {
                    if (m.space == SkinSpace::Body) drawMark(cr, m, bodyMap);
                    else if (m.space == SkinSpace::Skull) drawMark(cr, m, skullMap);
                    else if (m.space == SkinSpace::Head) drawMark(cr, m, headMap);
                }
            }
            cairo_restore(cr);
        }
        if (pal.outline.set) {
            buildPath();
            setCol(cr, pal.outline);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }

        // Tucked hind paws at bottom
        cairo_save(cr);
        cairo_translate(cr, seatX, seatY);
        cairo_rotate(cr, bTilt);
        cairo_scale(cr, sx, sy);

        double stagger = skin.shape.pawStagger;
        double leftHindCX = -b.hindCX * bw;
        double rightHindCX = b.hindCX * bw;
        double hindRX = b.hindRX;
        double leftHindCY = b.hindLeftCY;
        double leftHindRY = b.hindLeftRY;
        double rightHindCY = b.hindLeftCY + (b.hindStaggerCY - b.hindLeftCY) * stagger;
        double rightHindRY = b.hindLeftRY + (b.hindStaggerRY - b.hindLeftRY) * stagger;
        bool rightTucked = physics.hindFootActive.value < 0.35f;

        setCol(cr, pal.paws);
        cairo_new_path(cr);
        fillEllipse(cr, leftHindCX, leftHindCY, hindRX, leftHindRY);
        cairo_fill(cr);
        if (rightTucked) {
            cairo_new_path(cr);
            fillEllipse(cr, rightHindCX, rightHindCY, hindRX, rightHindRY);
            cairo_fill(cr);
        }

        if (!isClassicBlack(skin)) {
            setCol(cr, skin.colorpoint() ? pal.body : pal.crease, 0.56);
            cairo_set_line_width(cr, 1.3);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_new_path(cr);
            cairo_arc(cr, leftHindCX, leftHindCY, 7.5, 0.25, PI - 0.25);
            cairo_stroke(cr);
            if (rightTucked) {
                cairo_new_path(cr);
                cairo_arc(cr, rightHindCX, rightHindCY, 7.5, 0.25, PI - 0.25);
                cairo_stroke(cr);
            }
        }
        if (pal.pawPads.set) {
            setCol(cr, pal.pawPads);
            for (int side = 0; side < (rightTucked ? 2 : 1); ++side) {
                double cx = side ? rightHindCX : leftHindCX, cy = side ? rightHindCY : leftHindCY;
                cairo_new_path(cr); fillEllipse(cr, cx, cy + 1.6, 2.7, 1.9); cairo_fill(cr);
                for (double tx : {-3.4, 0.0, 3.4}) {
                    cairo_new_path(cr); fillEllipse(cr, cx + tx, cy - 1.7 - (tx == 0.0 ? 0.8 : 0.0), 1.25, 1.1); cairo_fill(cr);
                }
            }
        }
        cairo_restore(cr);
    }

    // MARK: - 3rd Key Hind Foot

    static void drawHindFoot(cairo_t* cr, float seatX, float seatY, const CatPhysics& physics, const SkinSpec& skin) {
        Pt hip = { seatX + 26.0f * skin.shape.bodyWidth, seatY + 2.0f };
        Pt target = physics.hindFoot.point();
        Pt mid = { (hip.x + target.x) * 0.5f + 10.0f, (hip.y + target.y) * 0.5f - 8.0f };
        drawLimb(cr, hip, mid, target, 15.0f, 12.0f, true, 0.85f * physics.hindFootActive.value, skin);
    }

    // MARK: - 2.5D Eyes & Whiskers

    static void drawHeadFeatures(cairo_t* cr, Pt center, const CatPhysics& physics, EyeColorTheme eyeTheme, const SkinSpec& skin) {
        cairo_save(cr);
        cairo_translate(cr, center.x, center.y);
        cairo_rotate(cr, physics.headTilt.value);
        if (skin.shape.headScale != 1.0f) cairo_scale(cr, skin.shape.headScale, skin.shape.headScale);

        float yawRad = physics.headYaw.value * 0.50f;
        float pitchRad = physics.headPitch.value * 0.36f;

        drawEye3D(cr, true, yawRad, pitchRad, physics, eyeTheme, skin);
        drawEye3D(cr, false, yawRad, pitchRad, physics, eyeTheme, skin);
        if (skin.palette.nose.set) {
            Pt nose = projectHead3D(0.0f, 8.2f, 29.0f, yawRad, pitchRad);
            cairo_new_path(cr);
            cairo_move_to(cr, nose.x - 2.3, nose.y - 1.1);
            quadCurveTo(cr, nose.x - 2.3, nose.y - 1.1, nose.x, nose.y - 1.6, nose.x + 2.3, nose.y - 1.1);
            quadCurveTo(cr, nose.x + 2.3, nose.y - 1.1, nose.x + 1.8, nose.y + 0.6, nose.x, nose.y + 1.5);
            quadCurveTo(cr, nose.x, nose.y + 1.5, nose.x - 1.8, nose.y + 0.6, nose.x - 2.3, nose.y - 1.1);
            cairo_close_path(cr);
            setCol(cr, skin.palette.nose);
            cairo_fill(cr);
        }
        if (skin.palette.mouth.set && physics.yawnAmount < 0.15f) {
            Pt mL = projectHead3D(-4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            Pt mM = projectHead3D(0.0f, 11.2f, 28.8f, yawRad, pitchRad);
            Pt mR = projectHead3D(4.2f, 12.2f, 28.4f, yawRad, pitchRad);
            setCol(cr, skin.palette.mouth);
            cairo_set_line_width(cr, 1.05);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_new_path(cr);
            cairo_move_to(cr, mL.x, mL.y);
            quadCurveTo(cr, mL.x, mL.y, (mL.x + mM.x) * 0.5, mL.y + 1.6, mM.x, mM.y);
            quadCurveTo(cr, mM.x, mM.y, (mM.x + mR.x) * 0.5, mR.y + 1.6, mR.x, mR.y);
            cairo_stroke(cr);
        }
        if (isBuiltInPatterned(skin)) drawPatternedMuzzle(cr, yawRad, pitchRad);
        if (physics.yawnAmount > 0.03f) drawYawnMouth(cr, yawRad, pitchRad, physics.yawnAmount, skin);
        drawWhiskers3D(cr, yawRad, pitchRad, physics, skin);

        // Pulsing red anime anger vein (💢) when typing super fast
        float anger = std::clamp(physics.angerIntensity.value, 0.0f, 1.0f);
        if (anger > 0.12f) {
            double pulse = 1.0 + std::sin(physics.time * 14.0f) * 0.12;
            double ax = 22.0 + yawRad * 8.0;
            double ay = -23.0 + pitchRad * 4.0;
            cairo_save(cr);
            cairo_translate(cr, ax, ay);
            cairo_scale(cr, pulse, pulse);
            cairo_set_source_rgba(cr, 0.94, 0.22, 0.22, 0.92 * anger);
            cairo_set_line_width(cr, 2.2);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            const double dirs[4][2] = { {-1,-1}, {1,-1}, {-1,1}, {1,1} };
            for (int d = 0; d < 4; ++d) {
                double sx = dirs[d][0], sy = dirs[d][1];
                cairo_new_path(cr);
                cairo_move_to(cr, sx * 2.0, sy * 6.0);
                cairo_line_to(cr, sx * 2.0, sy * 2.0);
                cairo_line_to(cr, sx * 6.0, sy * 2.0);
                cairo_stroke(cr);
            }
            cairo_restore(cr);
        }

        cairo_restore(cr);
    }

    static void drawYawnMouth(cairo_t* cr, float yawRad, float pitchRad, float yawn, const SkinSpec& skin) {
        double y = std::clamp(yawn, 0.0f, 1.0f);
        Pt c = projectHead3D(0.0f, (float)(16.2 + y * 2.2), 30.2f, yawRad, pitchRad);
        double rx = 3.2 + y * 5.8;
        double ry = 1.6 + y * 7.4;

        auto mouthPath = [&]() {
            cairo_new_path(cr);
            cairo_move_to(cr, 0.0, -ry * 0.92);
            cairo_curve_to(cr, rx * 0.68, -ry * 0.92, rx * 1.05, -ry * 0.42, rx, ry * 0.08);
            cairo_curve_to(cr, rx * 0.95, ry * 0.72, rx * 0.50, ry, 0.0, ry);
            cairo_curve_to(cr, -rx * 0.50, ry, -rx * 0.95, ry * 0.72, -rx, ry * 0.08);
            cairo_curve_to(cr, -rx * 1.05, -ry * 0.42, -rx * 0.68, -ry * 0.92, 0.0, -ry * 0.92);
            cairo_close_path(cr);
        };

        cairo_save(cr);
        cairo_translate(cr, c.x, c.y);

        mouthPath();
        cairo_set_source_rgba(cr, 0.78, 0.19, 0.25, 0.98);
        cairo_fill(cr);

        cairo_save(cr);
        mouthPath();
        cairo_clip(cr);

        cairo_save(cr);
        cairo_translate(cr, 0.0, -ry * 0.26);
        cairo_scale(cr, rx * 0.65, ry * 0.46);
        cairo_set_source_rgba(cr, 0.55, 0.10, 0.16, 0.55);
        cairo_arc(cr, 0.0, 0.0, 1.0, 0.0, PI * 2.0);
        cairo_fill(cr);
        cairo_restore(cr);

        if (y > 0.22) {
            double tAlpha = std::min(1.0, (y - 0.22) / 0.35);
            cairo_set_source_rgba(cr, 0.95, 0.48, 0.55, 0.92 * tAlpha);
            cairo_new_path(cr);
            cairo_move_to(cr, -rx * 0.62, ry * 0.55);
            quadCurveTo(cr, -rx * 0.62, ry * 0.55, -rx * 0.32, ry * 0.16, 0.0, ry * 0.36);
            quadCurveTo(cr, 0.0, ry * 0.36, rx * 0.32, ry * 0.16, rx * 0.62, ry * 0.55);
            quadCurveTo(cr, rx * 0.62, ry * 0.55, 0.0, ry * 1.25, -rx * 0.62, ry * 0.55);
            cairo_close_path(cr);
            cairo_fill(cr);
        }

        if (y > 0.28) {
            double fScale = std::min(1.0, (y - 0.28) / 0.32);
            cairo_set_source_rgba(cr, 1.0, 0.99, 0.96, 0.98);
            double uBaseY = -ry * 0.80, uLen = (2.6 + y * 2.2) * fScale, uHW = (1.15 + y * 0.55) * fScale;
            for (double sign : { -1.0, 1.0 }) {
                double fx = sign * rx * 0.46;
                cairo_new_path(cr);
                cairo_move_to(cr, fx - uHW, uBaseY - 0.8);
                cairo_line_to(cr, fx + uHW, uBaseY - 0.8);
                cairo_line_to(cr, fx - sign * 0.25, uBaseY + uLen);
                cairo_close_path(cr);
                cairo_fill(cr);
            }
            double lBaseY = ry * 0.84, lLen = (1.6 + y * 1.3) * fScale, lHW = (0.85 + y * 0.35) * fScale;
            for (double sign : { -1.0, 1.0 }) {
                double fx = sign * rx * 0.36;
                cairo_new_path(cr);
                cairo_move_to(cr, fx - lHW, lBaseY + 0.8);
                cairo_line_to(cr, fx + lHW, lBaseY + 0.8);
                cairo_line_to(cr, fx - sign * 0.15, lBaseY - lLen);
                cairo_close_path(cr);
                cairo_fill(cr);
            }
        }
        cairo_restore(cr);

        mouthPath();
        if (isClassicBlack(skin) || skin.colorpoint() || skin.palette.body.luminance() < 0.45)
            cairo_set_source_rgba(cr, 0.42, 0.08, 0.12, 0.85);
        else
            cairo_set_source_rgba(cr, 0.56, 0.22, 0.26, 0.72);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        cairo_restore(cr);
    }

    static void drawPatternedMuzzle(cairo_t* cr, float yawRad, float pitchRad) {
        const Pt local[] = {{-2.90f, 8.50f}, {-1.80f, 8.10f}, {1.80f, 8.10f}, {2.90f, 8.50f}, {3.20f, 8.90f}, {0.80f, 12.10f}, {0.00f, 12.30f}, {-0.80f, 12.10f}, {-3.20f, 8.90f}, {-2.90f, 8.50f}, {-5.90f, 14.40f}, {-4.70f, 18.00f}, {-1.40f, 18.00f}, {0.00f, 14.30f}, {1.40f, 18.00f}, {4.70f, 18.00f}, {5.90f, 14.40f}, {0.00f, 12.00f}};
        Pt p[18];
        for (int i=0;i<18;++i) p[i]=projectHead3D(local[i].x,local[i].y,30.0f,yawRad,pitchRad);
        cairo_save(cr); cairo_new_path(cr); cairo_move_to(cr,p[0].x,p[0].y);
        cairo_curve_to(cr,p[1].x,p[1].y,p[2].x,p[2].y,p[3].x,p[3].y);
        cairo_curve_to(cr,p[4].x,p[4].y,p[5].x,p[5].y,p[6].x,p[6].y);
        cairo_curve_to(cr,p[7].x,p[7].y,p[8].x,p[8].y,p[9].x,p[9].y);
        cairo_close_path(cr); cairo_set_source_rgb(cr,217.0/255,143.0/255,155.0/255); cairo_fill(cr);
        cairo_set_source_rgba(cr,139.0/255,140.0/255,134.0/255,0.78); cairo_set_line_width(cr,1.15);
        cairo_set_line_cap(cr,CAIRO_LINE_CAP_ROUND); cairo_set_line_join(cr,CAIRO_LINE_JOIN_ROUND);
        cairo_new_path(cr); cairo_move_to(cr,p[17].x,p[17].y); cairo_line_to(cr,p[13].x,p[13].y);
        cairo_move_to(cr,p[10].x,p[10].y); cairo_curve_to(cr,p[11].x,p[11].y,p[12].x,p[12].y,p[13].x,p[13].y);
        cairo_curve_to(cr,p[14].x,p[14].y,p[15].x,p[15].y,p[16].x,p[16].y); cairo_stroke(cr); cairo_restore(cr);
    }

    static void drawEye3D(cairo_t* cr, bool isLeft, float yawRad, float pitchRad, const CatPhysics& physics, EyeColorTheme eyeTheme, const SkinSpec& skin) {
        float sign = isLeft ? -1.0f : 1.0f;
        bool customIris = skin.palette.iris.set; // a skin's iris overrides the eye-colour menu
        const SkinColor& iris = skin.palette.iris;
        RGBA eyeCol = customIris ? RGBA{iris.r, iris.g, iris.b, iris.a} : eyePrimary(eyeTheme);
        Pt proj = projectHead3D(sign * 17.2f, -2.0f, 27.5f, yawRad, pitchRad);

        float eyeNormal = yawRad + sign * 0.34f;
        double foreX = std::clamp((double)std::cos(eyeNormal) / std::cos(0.34), 0.64, 1.06);
        double foreY = std::clamp((double)std::cos(pitchRad * 0.85f), 0.78, 1.03);

        cairo_save(cr);
        cairo_translate(cr, proj.x, proj.y);
        cairo_scale(cr, foreX, foreY);

        // Mode 0: Dizzy spinning spiral eyes (@ @) — appear AFTER initial golden stars warning (>0.28)
        float dizzy = std::clamp(physics.dizzyIntensity.value, 0.0f, 1.0f);
        if (dizzy > 0.28f) {
            cairo_set_source_rgba(cr, 0.98, 0.98, 0.96, 0.95);
            cairo_new_path(cr);
            fillEllipse(cr, 0.0, 0.0, 11.5, 11.0);
            cairo_fill(cr);
            cairo_set_source_rgba(cr, 0.14, 0.14, 0.18, 0.95);
            cairo_set_line_width(cr, 1.85);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            double spin = physics.dizzyPhase * (isLeft ? 1.0 : -1.0);
            cairo_new_path(cr);
            for (int i = 0; i <= 42; ++i) {
                double t = (double)i / 42.0;
                double theta = spin + t * PI * 5.2;
                double rad = 1.0 + t * 8.6;
                if (i == 0) cairo_move_to(cr, std::cos(theta) * rad, std::sin(theta) * rad);
                else cairo_line_to(cr, std::cos(theta) * rad, std::sin(theta) * rad);
            }
            cairo_stroke(cr);
            cairo_restore(cr);
            return;
        }

        if (physics.isHappyClosedEyes()) {
            setCol(cr, eyeCol);
            cairo_set_line_width(cr, 2.6);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_move_to(cr, -11.5, 1.0);
            quadCurveTo(cr, -11.5, 1.0, sign * 1.5, 3.4, 11.5, 1.0);
            cairo_stroke(cr);
            cairo_restore(cr);
            return;
        }

        if (physics.isBlissfulSquint()) {
            setCol(cr, eyeCol);
            cairo_new_path(cr);
            cairo_move_to(cr, -11.5, -0.5);
            quadCurveTo(cr, -11.5, -0.5, 0.0, 0.6, 11.5, -0.5);
            quadCurveTo(cr, 11.5, -0.5, 0.0, 4.8, -11.5, -0.5);
            cairo_close_path(cr);
            cairo_fill(cr);
            cairo_restore(cr);
            return;
        }

        float hunt = std::clamp(physics.huntIntensity.value, 0.0f, 1.0f);
        cairo_rotate(cr, sign * (0.06f * (1.0f - hunt * 0.6f)));

        bool siameseBlue = !customIris && eyeTheme == EyeColorTheme::SiameseBlue;
        bool isSiameseEye = buildOf(skin).sleekEyes && siameseBlue;
        double rx = isSiameseEye ? (11.4 + hunt * 1.3) : (12.0 + hunt * 1.1);
        double ry = isSiameseEye ? (9.8 + hunt * 2.0) : (10.8 + hunt * 1.8);

        cairo_save(cr);
        cairo_new_path(cr);
        fillEllipse(cr, 0, 0, rx, ry);
        cairo_clip(cr);

        // Clip the opening before drawing; never repaint the face through the oval.
        float closure = std::clamp(physics.effectiveEyeClosure(), 0.0f, 1.0f);
        if (closure > 0.0f) {
            cairo_rectangle(cr, -rx - 2, -ry + 2 * ry * closure,
                            2 * rx + 4, 2 * ry * (1 - closure));
            cairo_clip(cr);
        }

        setCol(cr, eyeCol);
        cairo_rectangle(cr, -rx - 1, -ry - 1, rx * 2 + 2, ry * 2 + 2);
        cairo_fill(cr);
        if (siameseBlue) {
            cairo_pattern_t* ig = cairo_pattern_create_linear(0.0, -ry, 0.0, ry);
            cairo_pattern_add_color_stop_rgba(ig, 0.00, 0.56, 0.77, 0.90, 1.0);
            cairo_pattern_add_color_stop_rgba(ig, 0.55, 0.71, 0.87, 0.96, 1.0);
            cairo_pattern_add_color_stop_rgba(ig, 1.00, 0.86, 0.945, 0.99, 1.0);
            cairo_set_source(cr, ig);
            cairo_rectangle(cr, -rx - 1, -ry - 1, rx * 2 + 2, ry * 2 + 2);
            cairo_fill(cr);
            cairo_pattern_destroy(ig);
        }

        Pt gaze = physics.eyeGaze.point();
        double px = gaze.x * (4.2 - hunt * 1.2);
        double py = gaze.y * (3.6 - hunt * 1.0);
        double pScale = physics.pupilScale.value;
        double normalW = 5.0 * pScale;
        double normalH = 13.5 * std::min(1.12, pScale);
        double pupilW = normalW + (18.8 - normalW) * hunt;
        double pupilH = normalH + (18.8 - normalH) * hunt;

        // Reference iris colors, blended only in a narrow ring around the moving pupil.
        if (!customIris && eyeTheme == EyeColorTheme::ReferenceHazel) {
            for (int i = 0; i <= 24; ++i) {
                double t = i / 24.0, mix = t*t*(3-2*t), rim = (1-t)*2.2;
                cairo_set_source_rgb(cr, (218-77*mix)/255, (178-33*mix)/255, (87+41*mix)/255);
                cairo_new_path(cr);
                fillEllipse(cr, px, py, pupilW*0.5+rim, pupilH*0.5+rim);
                cairo_fill(cr);
            }
        }
        setCol(cr, catBlack);
        cairo_new_path(cr);
        fillEllipse(cr, px, py, pupilW * 0.5, pupilH * 0.5);
        cairo_fill(cr);

        if (hunt > 0.25f) {
            double ga = std::min(1.0, (hunt - 0.25) / 0.55) * 0.92;
            double gr = 2.1 + hunt * 0.9;
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, ga);
            cairo_arc(cr, px - pupilW * 0.22, py - pupilH * 0.22, gr, 0, PI * 2.0);
            cairo_fill(cr);
        } else if (siameseBlue) {
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.86);
            cairo_arc(cr, px - 2.9, py - 3.6, 1.65, 0, PI * 2.0);
            cairo_fill(cr);
        }

        cairo_restore(cr);

        // Angry furrowed V-brow slant over eyes when typing super fast
        float anger = std::clamp(physics.angerIntensity.value, 0.0f, 1.0f);
        if (anger > 0.08f) {
            double drop = anger * 8.2;
            setCol(cr, skin.colorpoint() ? skin.palette.points : skin.palette.body);
            cairo_new_path(cr);
            cairo_move_to(cr, -rx - 2.0, -ry - 2.0);
            cairo_line_to(cr,  rx + 2.0, -ry - 2.0);
            cairo_line_to(cr,  rx + 2.0, -ry + (isLeft ? drop : 1.0));
            cairo_line_to(cr, -rx - 2.0, -ry + (isLeft ? 1.0 : drop));
            cairo_close_path(cr);
            cairo_fill(cr);
        }

        cairo_restore(cr);
    }

    static void drawWhiskers3D(cairo_t* cr, float yawRad, float pitchRad, const CatPhysics& physics, const SkinSpec& skin) {
        float wiggle = std::sin(physics.time * 4.5f) * (std::max(physics.petHeadIntensity, physics.petBodyIntensity) * 1.2f);
        setCol(cr, skin.palette.whiskers);
        cairo_set_line_width(cr, 1.05);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        const float rows[3][3] = {
            { 10.5f, 36.5f, 8.8f + wiggle },
            { 14.2f, 37.8f, 14.2f + wiggle * 0.5f },
            { 18.0f, 35.5f, 19.6f - wiggle * 0.4f }
        };

        for (float sign : {-1.0f, 1.0f}) {
            for (int i = 0; i < 3; ++i) {
                Pt p0 = projectHead3D(sign * 24.0f, rows[i][0], 23.0f, yawRad, pitchRad);
                Pt p2 = projectHead3D(sign * rows[i][1], rows[i][2], 14.0f, yawRad, pitchRad);
                cairo_move_to(cr, p0.x, p0.y);
                quadCurveTo(cr, p0.x, p0.y, (p0.x + p2.x) * 0.5, (p0.y + p2.y) * 0.5 - 0.8, p2.x, p2.y);
            }
        }
        cairo_stroke(cr);
    }

    // MARK: - Decorations (anchored PNG stickers)

    static cairo_surface_t* decorationSurface(const SkinDecoration& d) {
        if (!d.nativeImage) {
            struct Reader { const std::vector<uint8_t>* bytes; size_t pos; };
            Reader reader{&d.png, 0};
            cairo_surface_t* s = cairo_image_surface_create_from_png_stream(+[](void* closure, unsigned char* data, unsigned int length) {
                Reader* r = (Reader*)closure;
                if (r->pos + length > r->bytes->size()) return CAIRO_STATUS_READ_ERROR;
                std::memcpy(data, r->bytes->data() + r->pos, length);
                r->pos += length;
                return CAIRO_STATUS_SUCCESS;
            }, &reader);
            if (cairo_surface_status(s) != CAIRO_STATUS_SUCCESS) { cairo_surface_destroy(s); s = nullptr; }
            // An undecodable PNG is remembered as an empty surface so it is not retried every frame.
            d.nativeImage = std::shared_ptr<void>(s ? (void*)s : (void*)cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 0, 0),
                                                  [](void* p) { cairo_surface_destroy((cairo_surface_t*)p); });
        }
        cairo_surface_t* s = (cairo_surface_t*)d.nativeImage.get();
        return cairo_image_surface_get_width(s) > 0 ? s : nullptr;
    }

    static void drawDecorations(cairo_t* cr, float seatX, float seatY, Pt head, const CatPhysics& physics, const SkinSpec& skin) {
        float yawRad = physics.headYaw.value * 0.50f, pitchRad = physics.headPitch.value * 0.36f;
        for (const SkinDecoration& d : skin.decorations) {
            cairo_surface_t* img = decorationSurface(d);
            if (!img) continue;
            Pt at;
            double rot = 0, kx = 1, ky = 1;
            switch (d.anchor) {
                case SkinAnchorPoint::HeadTop:
                case SkinAnchorPoint::Face: {
                    float baseY = d.anchor == SkinAnchorPoint::HeadTop ? -30.0f : 4.0f;
                    at = headPoint({d.offsetX, baseY + d.offsetY}, false, skin, head, physics);
                    rot = physics.headTilt.value;
                    kx = std::max(0.55, (double)std::cos(yawRad)) * skin.shape.headScale;
                    ky = std::max(0.70, (double)std::cos(pitchRad)) * skin.shape.headScale;
                    break;
                }
                case SkinAnchorPoint::Neck:
                    at = bodyPoint({d.offsetX, -64.0f + d.offsetY}, skin, seatX, seatY, physics);
                    rot = physics.bodyTilt.value;
                    break;
                case SkinAnchorPoint::TailTip: {
                    TailCurve c = tailCurve(seatX, seatY, physics, skin);
                    at = { (float)c.x3 + d.offsetX, (float)c.y3 + d.offsetY };
                    break;
                }
            }
            double scale = 0.5 * d.scale; // stickers are authored at 2x: 1 image px = 0.5 canvas units
            cairo_save(cr);
            cairo_translate(cr, at.x, at.y);
            cairo_rotate(cr, rot + d.rotation * PI / 180.0);
            cairo_scale(cr, scale * kx, scale * ky);
            cairo_set_source_surface(cr, img, -cairo_image_surface_get_width(img) * 0.5, -cairo_image_surface_get_height(img) * 0.5);
            cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_GOOD);
            cairo_paint(cr);
            cairo_restore(cr);
        }
    }

    // MARK: - Front Paws

    static void drawFrontPaws(cairo_t* cr, float seatX, float seatY, float breath, const CatPhysics& physics, const SkinSpec& skin) {
        Pt lPos = physics.leftPaw.point();
        Pt rPos = physics.rightPaw.point();
        float lDist = std::hypot(lPos.x - CatPhysics::LeftPawRest.x, lPos.y - CatPhysics::LeftPawRest.y);
        float rDist = std::hypot(rPos.x - CatPhysics::RightPawRest.x, rPos.y - CatPhysics::RightPawRest.y);

        const Build& b = buildOf(skin);
        float bw = skin.shape.bodyWidth;
        float headLift = b.headLift;
        float lie = std::clamp(physics.lieIntensity.value, 0.0f, 1.0f);
        float lieS = lie * lie * (3.0f - 2.0f * lie);
        float idleTopY = seatY - b.idleTopDY + lieS * 24.0f;
        float idleTopX = b.idleTopX * bw - lieS * 5.0f;
        float idleBotX = b.idleBotX * bw - lieS * 4.5f;
        float idleBotY = seatY + 12.0f - lieS * 4.0f;
        float shX = b.shoulderX * bw;
        float limbStartW = b.limbStartW;
        float limbEndW = b.limbEndW;

        auto shoulder = [&](float x) -> Pt {
            float sy=physics.bodySquash.value*(1.0f-lieS*0.42f), sx=(1+(1-physics.bodySquash.value)*0.55f)*(1.0f+lieS*0.03f);
            float c=std::cos(physics.bodyTilt.value), s=std::sin(physics.bodyTilt.value);
            float floor=126+headLift+physics.headOffsetY.value+physics.headPitch.value*4.8f+breath*1.1f+42;
            return {seatX+x*sx*c+58*sy*s, std::max(seatY+x*sx*s-58*sy*c,floor)};
        };
        Pt lSh=shoulder(-shX), rSh=shoulder(shX);

        if (lDist < 6.0f && !physics.leftPawOnMouse && !physics.leftPawTyping) {
            if (lieS < 0.78f) drawIdleFrontLeg(cr, { seatX - idleTopX, idleTopY }, { seatX - idleBotX, idleBotY }, true, skin);
        } else {
            Pt mid = { (lSh.x + lPos.x) * 0.5f - (physics.leftPawOnMouse ? 8.0f : 4.0f), (lSh.y + lPos.y) * 0.5f - 6.0f };
            drawLimb(cr, lSh, mid, lPos, limbStartW, limbEndW, false, 0.78f, skin);
        }

        if (rDist < 6.0f && !physics.rightPawTyping) {
            if (lieS < 0.78f) drawIdleFrontLeg(cr, { seatX + idleTopX, idleTopY }, { seatX + idleBotX, idleBotY }, false, skin);
        } else {
            Pt mid = { (rSh.x + rPos.x) * 0.5f + 4.0f, (rSh.y + rPos.y) * 0.5f - 6.0f };
            drawLimb(cr, rSh, mid, rPos, limbStartW, limbEndW, true, 0.78f, skin);
        }
    }

    static void drawIdleFrontLeg(cairo_t* cr, Pt top, Pt bottom, bool isLeft, const SkinSpec& skin) {
        const SkinPalette& pal = skin.palette;
        float sign = isLeft ? -1.0f : 1.0f;
        if (skin.colorpoint()) {
            cairo_save(cr);
            roundedRect(cr, bottom.x - 7.5, top.y, 15.0, bottom.y - top.y + 5.0, 6.5);
            cairo_pattern_t* grad = cairo_pattern_create_linear(top.x, top.y, bottom.x, bottom.y + 5.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.05, pal.body.r, pal.body.g, pal.body.b, 0.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.50, pal.pointsMid.r, pal.pointsMid.g, pal.pointsMid.b, 0.75);
            cairo_pattern_add_color_stop_rgba(grad, 0.82, pal.points.r, pal.points.g, pal.points.b, 1.0);
            cairo_set_source(cr, grad);
            cairo_fill(cr);
            cairo_pattern_destroy(grad);
            cairo_restore(cr);
        } else {
            // Smooth shoulder-fade leg fill so palette.legs shows cleanly on idle sitting legs too
            // ("idleLegs": "legs"; the built-in coats use "chest" and keep their approved look)
            if (skin.idleLegsTinted) {
            cairo_save(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, top.x + sign * 8.5, top.y);
            quadCurveTo(cr, top.x + sign * 8.5, top.y, top.x + sign * 9.5, (top.y + bottom.y) * 0.5, bottom.x + sign * 7.5, bottom.y - 1.0);
            quadCurveTo(cr, bottom.x + sign * 7.5, bottom.y - 1.0, bottom.x + sign * 7.0, bottom.y + 5.2, bottom.x + sign * 0.8, bottom.y + 5.2);
            quadCurveTo(cr, bottom.x + sign * 0.8, bottom.y + 5.2, bottom.x - sign * 6.2, bottom.y + 5.2, bottom.x - sign * 6.0, bottom.y - 1.0);
            quadCurveTo(cr, bottom.x - sign * 6.0, bottom.y - 1.0, top.x - sign * 6.5, (top.y + bottom.y) * 0.5, top.x - sign * 7.0, top.y + 4.0);
            cairo_close_path(cr);
            cairo_pattern_t* legGrad = cairo_pattern_create_linear(top.x, top.y, bottom.x, bottom.y + 5.0);
            cairo_pattern_add_color_stop_rgba(legGrad, 0.04, pal.legs.r, pal.legs.g, pal.legs.b, 0.0);
            cairo_pattern_add_color_stop_rgba(legGrad, 0.32, pal.legs.r, pal.legs.g, pal.legs.b, pal.legs.a);
            cairo_pattern_add_color_stop_rgba(legGrad, 1.00, pal.legs.r, pal.legs.g, pal.legs.b, pal.legs.a);
            cairo_set_source(cr, legGrad);
            cairo_fill(cr);
            cairo_pattern_destroy(legGrad);
            cairo_restore(cr);
            }

            if (isClassicBlack(skin)) {
                setCol(cr, blackOutline);
                cairo_set_line_width(cr, BlackOutlineW * 1.8);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_new_path(cr);
                cairo_arc(cr, bottom.x, bottom.y - 1.5, 7.2, 0.15, PI - 0.15);
                cairo_stroke(cr);
            }
            setCol(cr, pal.paws);
            roundedRect(cr, bottom.x - 7.5, bottom.y - 13, 15, 18, 6.5);
            cairo_fill(cr);
        }
        if (isClassicBlack(skin)) setCol(cr, blackOutline, 0.75);
        else setCol(cr, pal.crease);
        cairo_set_line_width(cr, 1.45);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

        cairo_move_to(cr, top.x + sign * 8.5, top.y);
        quadCurveTo(cr, top.x + sign * 8.5, top.y, top.x + sign * 9.5, (top.y + bottom.y) * 0.5, bottom.x + sign * 7.5, bottom.y - 1.0);
        cairo_move_to(cr, top.x - sign * 7.0, top.y + 4.0);
        quadCurveTo(cr, top.x - sign * 7.0, top.y + 4.0, top.x - sign * 6.5, (top.y + bottom.y) * 0.5, bottom.x - sign * 6.0, bottom.y - 1.0);
        cairo_stroke(cr);
    }

    static void drawLimb(cairo_t* cr, Pt start, Pt ctrl, Pt end, float startW, float endW, bool outlineOnLeft, float outlineAlpha, const SkinSpec& skin) {
        const SkinPalette& pal = skin.palette;
        constexpr int steps = 14;
        std::vector<Pt> leftEdge, rightEdge;
        leftEdge.reserve(steps + 1); rightEdge.reserve(steps + 1);

        for (int i = 0; i <= steps; ++i) {
            float t = (float)i / (float)steps;
            Pt p = quadPoint(start, ctrl, end, t);
            Pt tan = quadTangent(start, ctrl, end, t);
            float len = std::max(0.001f, std::hypot(tan.x, tan.y));
            float nx = -tan.y / len, ny = tan.x / len;
            float hw = (startW + (endW - startW) * t) * 0.5f;
            leftEdge.push_back({ p.x + nx * hw, p.y + ny * hw });
            rightEdge.push_back({ p.x - nx * hw, p.y - ny * hw });
        }

        auto traceLimb = [&]() {
            cairo_new_path(cr);
            cairo_move_to(cr, leftEdge[0].x, leftEdge[0].y);
            for (int i = 1; i <= steps; ++i) cairo_line_to(cr, leftEdge[i].x, leftEdge[i].y);
            for (int i = steps; i >= 0; --i) cairo_line_to(cr, rightEdge[i].x, rightEdge[i].y);
            Pt tangent=quadTangent(start,ctrl,end,0);
            float length=std::max(0.001f,std::hypot(tangent.x,tangent.y));
            quadCurveTo(cr,rightEdge[0].x,rightEdge[0].y,
                start.x-tangent.x/length*startW*0.5f,start.y-tangent.y/length*startW*0.5f,
                leftEdge[0].x,leftEdge[0].y);
            cairo_close_path(cr);
        };

        double pawR = endW * 0.58;
        if (isClassicBlack(skin)) {
            setCol(cr, blackOutline);
            cairo_set_line_width(cr, BlackOutlineW * 2.0);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_new_path(cr);
            cairo_move_to(cr, leftEdge[2].x, leftEdge[2].y);
            for (int i = 3; i <= steps; ++i) cairo_line_to(cr, leftEdge[i].x, leftEdge[i].y);
            cairo_stroke(cr);
            cairo_new_path(cr);
            cairo_move_to(cr, rightEdge[2].x, rightEdge[2].y);
            for (int i = 3; i <= steps; ++i) cairo_line_to(cr, rightEdge[i].x, rightEdge[i].y);
            cairo_stroke(cr);
            cairo_new_path(cr);
            fillEllipse(cr, end.x, end.y, pawR, pawR * 0.85);
            cairo_stroke(cr);
        }
        const SkinColor& lineCol = pal.outline.set ? pal.outline : pal.crease;
        if (!skin.colorpoint()) {
            traceLimb();
            setCol(cr, pal.legs);
            if (pal.belly.set && !pal.belly.sameAs(pal.legs)) {
                // White chest fading into the leg colour along the first 30% of the arm.
                const SkinColor& chest=pal.belly; const SkinColor& base=pal.legs;
                cairo_pattern_t* gradient=cairo_pattern_create_linear(start.x,start.y,end.x,end.y);
                cairo_pattern_add_color_stop_rgb(gradient,0,chest.r,chest.g,chest.b);
                cairo_pattern_add_color_stop_rgb(gradient,0.30,base.r,base.g,base.b);
                cairo_pattern_set_extend(gradient,CAIRO_EXTEND_PAD);
                cairo_set_source(cr,gradient);cairo_fill(cr);cairo_pattern_destroy(gradient);
            } else cairo_fill(cr);
            if (!pal.paws.sameAs(pal.legs)) {
                int first=(int)(steps*0.72f);
                cairo_new_path(cr);cairo_move_to(cr,leftEdge[first].x,leftEdge[first].y);
                for (int i=first+1;i<=steps;++i)cairo_line_to(cr,leftEdge[i].x,leftEdge[i].y);
                for (int i=steps;i>=first;--i)cairo_line_to(cr,rightEdge[i].x,rightEdge[i].y);
                cairo_close_path(cr);setCol(cr,pal.paws);cairo_fill(cr);
            }
            setCol(cr, pal.paws);
            cairo_new_path(cr);
            fillEllipse(cr, end.x, end.y, pawR, pawR * 0.85);
            cairo_fill(cr);
        } else {
            cairo_save(cr);
            traceLimb();
            cairo_pattern_t* grad = cairo_pattern_create_linear(start.x, start.y, end.x, end.y);
            cairo_pattern_add_color_stop_rgba(grad, 0.0,  pal.body.r, pal.body.g, pal.body.b, 0.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.16, pal.body.r, pal.body.g, pal.body.b, 1.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.48, pal.pointsMid.r, pal.pointsMid.g, pal.pointsMid.b, 1.0);
            cairo_pattern_add_color_stop_rgba(grad, 0.78, pal.points.r, pal.points.g, pal.points.b, 1.0);
            cairo_set_source(cr, grad);
            cairo_fill(cr);
            cairo_pattern_destroy(grad);
            cairo_restore(cr);

            setCol(cr, pal.paws);
            cairo_new_path(cr);
            fillEllipse(cr, end.x, end.y, pawR, pawR * 0.85);
            cairo_fill(cr);

            const auto& outerEdge = outlineOnLeft ? rightEdge : leftEdge;
            setCol(cr, lineCol, 0.78);
            cairo_set_line_width(cr, 1.25);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_move_to(cr, outerEdge[3].x, outerEdge[3].y);
            for (int i = 4; i < steps; ++i) cairo_line_to(cr, outerEdge[i].x, outerEdge[i].y);
            cairo_stroke(cr);
        }

        if (!isClassicBlack(skin)) {
            const auto& hiEdge = outlineOnLeft ? leftEdge : rightEdge;
            int sIdx = 2, eIdx = (int)(steps * 0.68f);
            setCol(cr, skin.colorpoint() ? lineCol : pal.crease, outlineAlpha * 0.9);
            cairo_set_line_width(cr, 1.35);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
            cairo_move_to(cr, hiEdge[sIdx].x, hiEdge[sIdx].y);
            for (int i = sIdx + 1; i <= eIdx; ++i) cairo_line_to(cr, hiEdge[i].x, hiEdge[i].y);
            cairo_stroke(cr);
        }
    }

    // MARK: - Floating Keycaps

    static void drawFloatingKeycaps(cairo_t* cr, const CatPhysics& physics) {
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

        for (const auto& b : physics.floatingKeycaps) {
            float prog = b.age / b.lifetime;
            double scale = 1.0;
            if (b.age < 0.09f) scale = 0.72 + 0.38 * std::sin((b.age / 0.09) * PI * 0.65);
            else if (b.age < 0.18f) scale = 1.10 - 0.10 * ((b.age - 0.09) / 0.09);

            double alpha = prog < 0.62f ? 1.0 : std::max(0.0, 1.0 - (prog - 0.62) / 0.38);
            if (alpha < 0.02) continue;

            // Estimate display glyph count (UTF-8 multi-byte symbols count as 1 char)
            size_t glyphLen = 0;
            bool hasNonAscii = false;
            for (unsigned char c : b.label) {
                if ((c & 0xC0) != 0x80) glyphLen++;
                if (c >= 0x80) hasNonAscii = true;
            }
            double w = glyphLen <= 1 ? 23.0 : (glyphLen == 2 ? (hasNonAscii ? 34.0 : 30.0) : 38.0);
            double h = 22.0;

            cairo_save(cr);
            cairo_translate(cr, b.position.x, b.position.y);
            cairo_rotate(cr, b.rotation);
            cairo_scale(cr, scale, scale);

            roundedRect(cr, -w * 0.5, -h * 0.5 + 2.5, w, h, 5.5);
            cairo_set_source_rgba(cr, 0.15, 0.15, 0.17, 0.95 * alpha);
            cairo_fill(cr);

            roundedRect(cr, -w * 0.5, -h * 0.5, w, h, 5.2);
            cairo_set_source_rgba(cr, 0.975, 0.972, 0.952, alpha);
            cairo_fill_preserve(cr);
            cairo_set_source_rgba(cr, 0.16, 0.16, 0.18, alpha);
            cairo_set_line_width(cr, 1.45);
            cairo_stroke(cr);

            cairo_set_font_size(cr, glyphLen <= 1 ? 11.5 : 9.8);
            cairo_text_extents_t ext;
            cairo_text_extents(cr, b.label.c_str(), &ext);
            cairo_set_source_rgba(cr, 0.12, 0.12, 0.14, alpha);
            cairo_move_to(cr, -(ext.width * 0.5 + ext.x_bearing), -(ext.height * 0.5 + ext.y_bearing));
            cairo_show_text(cr, b.label.c_str());

            cairo_restore(cr);
        }
    }

    // MARK: - Sparkles & Sleep Z

    static void drawSparkles(cairo_t* cr, const CatPhysics& physics, EyeColorTheme eyeTheme) {
        RGBA c = eyePrimary(eyeTheme);
        for (const auto& sp : physics.sparkles) {
            double p = std::min(1.0, (double)(sp.age / sp.lifetime));
            double alpha = std::sin(p * PI) * 0.85;
            if (alpha < 0.02) continue;

            cairo_save(cr);
            setCol(cr, c, alpha);
            if (sp.isSleepZ) {
                cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
                cairo_set_font_size(cr, 11.0 + p * 3.0);
                cairo_move_to(cr, sp.position.x, sp.position.y);
                cairo_show_text(cr, "z");
            } else {
                double r = 3.5 + p * 1.5;
                cairo_translate(cr, sp.position.x, sp.position.y);
                cairo_move_to(cr, 0, -r);
                quadCurveTo(cr, 0, -r, 0.8, -0.8, r, 0);
                quadCurveTo(cr, r, 0, 0.8, 0.8, 0, r);
                quadCurveTo(cr, 0, r, -0.8, 0.8, -r, 0);
                quadCurveTo(cr, -r, 0, -0.8, -0.8, 0, -r);
                cairo_close_path(cr);
                cairo_fill(cr);
            }
            cairo_restore(cr);
        }
    }
};
