static int font_size_index(float scale) {
    if (scale >= 1.5f) return 5;
    if (scale >= 1.15f) return 4;
    if (scale >= 1.0f) return 3;
    if (scale >= 0.85f) return 2;
    if (scale >= 0.72f) return 1;
    return 0;
}

static void draw_text(BitmapFont *font, int x, int y, float scale, unsigned int color, const char *text) {
    if (!font || !font->atlas || !text) return;
    int size_index = font_size_index(scale);
    float pen_x = (float)x;
    int baseline_y = y;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '\n') {
            pen_x = (float)x;
            baseline_y += FONT_ATLAS_SIZES[size_index].line_height;
            continue;
        }
        unsigned char ch = *p;
        if (ch < FONT_ATLAS_FIRST || ch >= FONT_ATLAS_FIRST + FONT_ATLAS_COUNT) ch = '?';
        const BitmapGlyph *glyph = &FONT_ATLAS_GLYPHS[size_index][ch - FONT_ATLAS_FIRST];
        if (ch != ' ') {
            vita2d_draw_texture_tint_part(font->atlas,
                pen_x + glyph->xoff,
                (float)(baseline_y + glyph->yoff),
                glyph->x,
                glyph->y,
                glyph->w,
                glyph->h,
                color);
        }
        pen_x += glyph->advance;
    }
}
