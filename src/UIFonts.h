#pragma once

// Font objects are supplied by TFT_eSPI's gfxfont.h when LOAD_GFXFF is set.
// This header must be included after TFT_eSPI.h.
namespace UIFonts {
static const GFXfont *const Glucose = &FreeSansBold24pt7b;
static const GFXfont *const Unit = &FreeSans12pt7b;
static const GFXfont *const Trend = &FreeSansBold12pt7b;
static const GFXfont *const Button = &FreeSansBold9pt7b;
}  // namespace UIFonts

