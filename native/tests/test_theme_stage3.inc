// 0.14 stage 3: the "Theme effects" switch (Settings, saved): Off = no page textures, clouds, bubbles, grain, halftone, grid, streak, glints, brush lines, reflection,
// corner brackets or HUD decoration; colours, gloss gradients and fonts stay. Included by test_theme_style.inc. WRITTEN BEFORE the code (link-red first).
// References: the rules of the owner's brief and the maths of stage 2 (a texture is only ever a move of the page colour towards its tone, so with none the page is the gradient).

static bool sameRgb(const Rgb& a, const Rgb& b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static void testEffectsSetting() {
    checkStr("Theme effects default to On", Prefs{}.effects, "on");
    Prefs p; p.effects = "off";
    checkStr("Off is saved in the settings file", loadPrefs(savePrefs(p)).effects, "off");
    checkTrue("the settings text has the line effects=off", savePrefs(p).find("effects=off\n") != std::string::npos);
    p.effects = "on";
    checkStr("On round-trips", loadPrefs(savePrefs(p)).effects, "on");
    checkStr("an old settings file without the line means On", loadPrefs("theme=dark\nmotion=auto\n").effects, "on");
    checkStr("a bad value is ignored (stays On)", loadPrefs("effects=maybe\n").effects, "on");
    checkStr("the other settings are not touched by it", loadPrefs("effects=off\ntheme=aero\ntextSize=large\n").theme, "aero");
    checkTrue("validEffects accepts on and off only", validEffects("on") && validEffects("off") && !validEffects("") && !validEffects("auto"));
}

static void testEffectsOff() {
    for (const ThemeDef* t : allThemes()) {
        const std::string id = t->id;
        if (id == "auto") continue;
        const ThemeStyle full = themeColors(id, false).style, off = withoutEffects(full);
        const std::string w = id + ": ";
        checkTrue(w + "no page textures, no streak", pageFxTotal(off.page.fx) == 0 && off.page.fx.streak == 0 && off.page.fx.clouds == 0 && off.page.fx.bubbles == 0 && off.page.fx.grain == 0 &&
                  off.page.fx.halftone == 0 && off.page.fx.grid == 0);
        checkTrue(w + "no glints, brush lines, reflection, brackets, HUD rules or spec label", off.widget.glint == 0 && off.widget.brush == 0 && off.panel.reflectAlpha == 0 && off.panel.reflectHeight == 0 &&
                  off.panel.bracketAlpha == 0 && off.panel.bracketSize == 0 && off.panel.hudAlpha == 0 && off.panel.spec.empty());
        // colours, gloss and fonts stay exactly
        bool same = off.page.kind == full.page.kind && off.page.stops == full.page.stops;
        for (int i = 0; i < full.page.stops; i++) same = same && sameRgb(off.page.stop[i].c, full.page.stop[i].c) && off.page.stop[i].pos == full.page.stop[i].pos;
        checkTrue(w + "the page gradient is unchanged", same);
        const WidgetStyle& a = off.widget; const WidgetStyle& b = full.widget;
        checkTrue(w + "the gloss gradients, rim, glow and tints are unchanged", a.hi == b.hi && a.split == b.split && a.loAmount == b.loAmount && sameRgb(a.lo, b.lo) && a.shineLineAlpha == b.shineLineAlpha &&
                  a.pressDark == b.pressDark && a.hoverTint == b.hoverTint && a.pressTint == b.pressTint && a.rimDark == b.rimDark && a.rimLight == b.rimLight && a.hoverGlow == b.hoverGlow &&
                  a.glowAlpha == b.glowAlpha && sameRgb(a.glow, b.glow));
        const PanelStyle& pa = off.panel; const PanelStyle& pb = full.panel;
        checkTrue(w + "the glass, shine, shadow, rim, outer glow and title bar are unchanged", pa.fillAlpha == pb.fillAlpha && pa.floatAlpha == pb.floatAlpha && sameRgb(pa.fill, pb.fill) && pa.shineAlpha == pb.shineAlpha &&
                  pa.shadowAlpha == pb.shadowAlpha && pa.rimAlpha == pb.rimAlpha && pa.glowAlpha == pb.glowAlpha && pa.glowBlur == pb.glowBlur && pa.title.set == pb.title.set && sameRgb(pa.title.top, pb.title.top) &&
                  sameRgb(pa.title.bottom, pb.title.bottom) && pa.title.shine == pb.title.shine && sameRgb(pa.title.text, pb.title.text) && pa.floatRounding == pb.floatRounding);
        checkTrue(w + "the fonts are unchanged", off.font == full.font && off.fontRegular == full.fontRegular && off.fontBold == full.fontBold && off.fontItalic == full.fontItalic && off.titleFont == full.titleFont);
        checkTrue(w + "the button colour, accent and fills are unchanged", off.button.set == full.button.set && sameRgb(off.button.c, full.button.c) && off.accent.set == full.accent.set && off.accentFill.set == full.accentFill.set &&
                  off.roundScale == full.roundScale && off.square == full.square && off.grayscale == full.grayscale);
        checkTrue(w + "switching effects off twice changes nothing more", pageFxTotal(withoutEffects(off).page.fx) == 0 && withoutEffects(off).panel.title.set == off.panel.title.set);
        if (!hasStage2Decor(full) || id == "light") checkTrue(w + "a plain theme is the same style either way", isPlainStyle(off) == isPlainStyle(full));
    }
    checkTrue("Aero and Night DO lose decoration (the switch does something)", pageFxTotal(themeColors("aero", true).style.page.fx) > 0 && pageFxTotal(withoutEffects(themeColors("aero", true).style).page.fx) == 0 &&
              themeColors("y2knight", false).style.page.fx.streak > 0 && withoutEffects(themeColors("y2knight", false).style).page.fx.streak == 0 && themeColors("y2knight", false).style.panel.bracketAlpha > 0 &&
              withoutEffects(themeColors("y2knight", false).style).panel.bracketAlpha == 0);
    // what the page is with the textures gone: the plain gradient; the four extremes of stage 2 are then the page itself
    for (const char* id : {"aero", "y2knight"}) {
        const ThemeStyle off = withoutEffects(themeColors(id, false).style);
        bool plain = true;
        for (int i = 0; i <= 20; i++) for (const Rgb& e : pageFxExtremes(off.page.fx, pageColourAt(off.page, rgb8(0, 0, 0), i / 20.f))) plain = plain && sameRgb(e, pageColourAt(off.page, rgb8(0, 0, 0), i / 20.f));
        checkTrue(std::string(id) + ": with effects off the page is its plain gradient at every height", plain);
        checkLook(id, true);                                       // every text pair still passes, with the switch Off
    }
}

static void testStage3() {
    testEffectsSetting();
    testEffectsOff();
}
