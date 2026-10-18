/*
 * theme.hpp — Paleta y tipografia de la interfaz.
 *
 */
#ifndef PROFILER_THEME_HPP
#define PROFILER_THEME_HPP

#include <SDL3/SDL.h>

namespace theme {

/* ------------------------------------------------------------------ colores */

constexpr SDL_Color color_background   = { 0x1a, 0x28, 0x48, 0xff };   /* #1a2848 */
constexpr SDL_Color color_panel        = { 0x30, 0x49, 0x7d, 0xff };   /* #30497d */
constexpr SDL_Color color_panel_raised = { 0x45, 0x5b, 0x8a, 0xff };   /* #455b8a */
constexpr SDL_Color color_accent       = { 0xf2, 0x84, 0x2f, 0xff };   /* #f2842f */

/* Secciones de la tarta: los diez tonos de la rampa verde -> rojo (tono
 * 132 -> 8, luminosidad 0.86 -> 0.50) pero barajados con paso 3, o sea en el
 * orden 1, 4, 7, 10, 2, 5, 8, 3, 6, 9. Asi dos secciones consecutivas nunca
 * cogen colores vecinos de la rampa y se distinguen mucho mas. */
constexpr SDL_Color color_sections[10] = {
    { 0xc5, 0xf1, 0xce, 0xff },   /* #c5f1ce  rampa 1  */
    { 0xbc, 0xe6, 0x94, 0xff },   /* #bce694  rampa 4  */
    { 0xda, 0xc5, 0x62, 0xff },   /* #dac562  rampa 7  */
    { 0xcf, 0x46, 0x30, 0xff },   /* #cf4630  rampa 10 */
    { 0xb6, 0xee, 0xb5, 0xff },   /* #b6eeb5  rampa 2  */
    { 0xc7, 0xe2, 0x83, 0xff },   /* #c7e283  rampa 5  */
    { 0xd6, 0xa0, 0x51, 0xff },   /* #d6a051  rampa 8  */
    { 0xb6, 0xea, 0xa4, 0xff },   /* #b6eaa4  rampa 3  */
    { 0xd8, 0xde, 0x73, 0xff },   /* #d8de73  rampa 6  */
    { 0xd2, 0x76, 0x41, 0xff },   /* #d27641  rampa 9  */
};

/* --------------------------------------------------------------- animacion */

/* Duraciones en milisegundos. */
constexpr double animation_separation_ms          = 1000.0;  /* separate() */
constexpr double animation_growing_half_period_ms = 1000.0;  /* 0 -> 100 % */
constexpr double animation_growing_period_ms      = 2.0 * animation_growing_half_period_ms;

/* --------------------------------------------------------------- tipografia */

constexpr float font_size_title = 34.0f;
constexpr float font_size_base  = 22.0f;

}

#endif // PROFILER_THEME_HPP
