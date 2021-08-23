#ifndef __ARTIST_COLOR_H
#define __ARTIST_COLOR_H

#include <artist/color.hpp>

#ifdef __cplusplus
using namespace cycfi;
namespace colors = cycfi::artist::colors;
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Colors
   ////////////////////////////////////////////////////////////////////////////
   using color = artist::color;

   color rgb(std::uint32_t rgb) { return artist::rgb(rgb); }
   color rgba(std::uint32_t rgba) { return artist::rgba(rgba); }
   color rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) { return artist::rgb(r, g, b); }
   color rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) { return artist::rgba(r, g, b, a); }
   color hsl(float h, float s, float l) { return artist::hsl(h, s, l); }
   color opacity(color src, float alpha_) { return src.opacity(alpha_); }
   color level(color src, float amount) { return src.level(amount); }

   ////////////////////////////////////////////////////////////////////////////
   // Common colors
   ////////////////////////////////////////////////////////////////////////////

   color COLOR_ALICE_BLUE = colors::alice_blue;
   color COLOR_ANTIQUE_WHITE = colors::antique_white;
   color COLOR_AQUAMARINE = colors::aquamarine;
   color COLOR_AZURE = colors::azure;
   color COLOR_BEIGE = colors::beige;
   color COLOR_BISQUE = colors::bisque;
   color COLOR_BLACK = colors::black;
   color COLOR_BLANCHED_ALMOND = colors::blanched_almond;
   color COLOR_BLUE = colors::blue;
   color COLOR_BLUE_VIOLET = colors::blue_violet;
   color COLOR_BROWN = colors::brown;
   color COLOR_BURLY_WOOD = colors::burly_wood;
   color COLOR_CADET_BLUE = colors::cadet_blue;
   color COLOR_CHARTREUSE = colors::chartreuse;
   color COLOR_CHOCOLATE = colors::chocolate;
   color COLOR_CORAL = colors::coral;
   color COLOR_CORNFLOWER_BLUE = colors::cornflower_blue;
   color COLOR_CORN_SILK = colors::corn_silk;
   color COLOR_CYAN = colors::cyan;
   color COLOR_DARK_GOLDENROD = colors::dark_goldenrod;
   color COLOR_DARK_GREEN = colors::dark_green;
   color COLOR_DARK_KHAKI = colors::dark_khaki;
   color COLOR_DARK_OLIVE_GREEN = colors::dark_olive_green;
   color COLOR_DARK_ORANGE = colors::dark_orange;
   color COLOR_DARK_ORCHID = colors::dark_orchid;
   color COLOR_DARK_SALMON = colors::dark_salmon;
   color COLOR_DARK_SEA_GREEN = colors::dark_sea_green;
   color COLOR_DARK_SLATE_BLUE = colors::dark_slate_blue;
   color COLOR_DARK_SLATE_GRAY = colors::dark_slate_gray;
   color COLOR_DARK_TURQUOISE = colors::dark_turquoise;
   color COLOR_DARK_VIOLET = colors::dark_violet;
   color COLOR_DEEP_PINK = colors::deep_pink;
   color COLOR_DEEP_SKY_BLUE = colors::deep_sky_blue;
   color COLOR_DIM_GRAY = colors::dim_gray;
   color COLOR_DODGER_BLUE = colors::dodger_blue;
   color COLOR_FIREBRICK = colors::firebrick;
   color COLOR_FLORAL_WHITE = colors::floral_white;
   color COLOR_FOREST_GREEN = colors::forest_green;
   color COLOR_GAINS_BORO = colors::gains_boro;
   color COLOR_GHOST_WHITE = colors::ghost_white;
   color COLOR_GOLD = colors::gold;
   color COLOR_GOLDENROD = colors::goldenrod;
   color COLOR_GREEN = colors::green;
   color COLOR_GREEN_YELLOW = colors::green_yellow;
   color COLOR_HONEYDEW = colors::honeydew;
   color COLOR_HOT_PINK = colors::hot_pink;
   color COLOR_INDIAN_RED = colors::indian_red;
   color COLOR_IVORY = colors::ivory;
   color COLOR_KHAKI = colors::khaki;
   color COLOR_LAVENDER = colors::lavender;
   color COLOR_LAVENDER_BLUSH = colors::lavender_blush;
   color COLOR_LAWN_GREEN = colors::lawn_green;
   color COLOR_LEMON_CHIFFON = colors::lemon_chiffon;
   color COLOR_LIGHT_BLUE = colors::light_blue;
   color COLOR_LIGHT_CORAL = colors::light_coral;
   color COLOR_LIGHT_CYAN = colors::light_cyan;
   color COLOR_LIGHT_GOLDENROD = colors::light_goldenrod;
   color COLOR_LIGHT_GOLDENROD_YELLOW = colors::light_goldenrod_yellow;
   color COLOR_LIGHT_GRAY = colors::light_gray;
   color COLOR_LIGHT_PINK = colors::light_pink;
   color COLOR_LIGHT_SALMON = colors::light_salmon;
   color COLOR_LIGHT_SEA_GREEN = colors::light_sea_green;
   color COLOR_LIGHT_SKY_BLUE = colors::light_sky_blue;
   color COLOR_LIGHT_SLATE_BLUE = colors::light_slate_blue;
   color COLOR_LIGHT_SLATE_GRAY = colors::light_slate_gray;
   color COLOR_LIGHT_STEEL_BLUE = colors::light_steel_blue;
   color COLOR_LIGHT_YELLOW = colors::light_yellow;
   color COLOR_LIME_GREEN = colors::lime_green;
   color COLOR_LINEN = colors::linen;
   color COLOR_MAGENTA = colors::magenta;
   color COLOR_MAROON = colors::maroon;
   color COLOR_MEDIUM_AQUAMARINE = colors::medium_aquamarine;
   color COLOR_MEDIUM_BLUE = colors::medium_blue;
   color COLOR_MEDIUM_FOREST_GREEN = colors::medium_forest_green;
   color COLOR_MEDIUM_GOLDENROD = colors::medium_goldenrod;
   color COLOR_MEDIUM_ORCHID = colors::medium_orchid;
   color COLOR_MEDIUM_PURPLE = colors::medium_purple;
   color COLOR_MEDIUM_SEA_GREEN = colors::medium_sea_green;
   color COLOR_MEDIUM_SLATE_BLUE = colors::medium_slate_blue;
   color COLOR_MEDIUM_SPRING_GREEN = colors::medium_spring_green;
   color COLOR_MEDIUM_TURQUOISE = colors::medium_turquoise;
   color COLOR_MEDIUM_VIOLET_RED = colors::medium_violet_red;
   color COLOR_MIDNIGHT_BLUE = colors::midnight_blue;
   color COLOR_MINT_CREAM = colors::mint_cream;
   color COLOR_MISTY_ROSE = colors::misty_rose;
   color COLOR_MOCCASIN = colors::moccasin;
   color COLOR_NAVAJO_WHITE = colors::navajo_white;
   color COLOR_NAVY = colors::navy;
   color COLOR_NAVY_BLUE = colors::navy_blue;
   color COLOR_OLD_LACE = colors::old_lace;
   color COLOR_OLIVE_DRAB = colors::olive_drab;
   color COLOR_ORANGE = colors::orange;
   color COLOR_ORANGE_RED = colors::orange_red;
   color COLOR_ORCHID = colors::orchid;
   color COLOR_PALE_GOLDENROD = colors::pale_goldenrod;
   color COLOR_PALE_GREEN = colors::pale_green;
   color COLOR_PALE_TURQUOISE = colors::pale_turquoise;
   color COLOR_PALE_VIOLET_RED = colors::pale_violet_red;
   color COLOR_PAPAYA_WHIP = colors::papaya_whip;
   color COLOR_PEACH_PUFF = colors::peach_puff;
   color COLOR_PERU = colors::peru;
   color COLOR_PINK = colors::pink;
   color COLOR_PLUM = colors::plum;
   color COLOR_POWDER_BLUE = colors::powder_blue;
   color COLOR_PURPLE = colors::purple;
   color COLOR_RED = colors::red;
   color COLOR_ROSY_BROWN = colors::rosy_brown;
   color COLOR_ROYAL_BLUE = colors::royal_blue;
   color COLOR_SADDLE_BROWN = colors::saddle_brown;
   color COLOR_SALMON = colors::salmon;
   color COLOR_SANDY_BROWN = colors::sandy_brown;
   color COLOR_SEA_GREEN = colors::sea_green;
   color COLOR_SEA_SHELL = colors::sea_shell;
   color COLOR_SIENNA = colors::sienna;
   color COLOR_SKY_BLUE = colors::sky_blue;
   color COLOR_SLATE_BLUE = colors::slate_blue;
   color COLOR_SLATE_GRAY = colors::slate_gray;
   color COLOR_SNOW = colors::snow;
   color COLOR_SPRING_GREEN = colors::spring_green;
   color COLOR_STEEL_BLUE = colors::steel_blue;
   color COLOR_TAN = colors::tan;
   color COLOR_THISTLE = colors::thistle;
   color COLOR_TOMATO = colors::tomato;
   color COLOR_TURQUOISE = colors::turquoise;
   color COLOR_VIOLET = colors::violet;
   color COLOR_VIOLET_RED = colors::violet_red;
   color COLOR_WHEAT = colors::wheat;
   color COLOR_WHITE = colors::white;
   color COLOR_WHITE_SMOKE = colors::white_smoke;
   color COLOR_YELLOW = colors::yellow;
   color COLOR_YELLOW_GREEN = colors::yellow_green;

#ifdef __cplusplus
}
#endif
#endif
