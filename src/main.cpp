/*
 * main.cpp — cuerpo del profiler con el patron de callbacks de SDL3.
 *
 * No hay `main()`: SDL3 pone el punto de entrada y llama a estas funciones
 * (ver `docs/README-main-functions.md` de SDL y el ejemplo
 * `examples/renderer/01-clear/clear.c`):
 *
 *   SDL_AppInit     setup    -> ventana, renderer, fuentes. Una sola vez.
 *   SDL_AppEvent    entrada  -> ESC cierra.
 *   SDL_AppIterate  cuadro   -> limpia con el fondo y presenta.
 *   SDL_AppQuit     limpieza -> libera lo de Init.
 */

#define SDL_MAIN_USE_CALLBACKS 1
#define DEBUG
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <utility>
#include <vector>

#include "pie_chart.hpp"
#include "theme.hpp"

namespace {

/* Todo lo que vive mientras el programa corre. */
struct app_state {
    SDL_Window*   window     = nullptr;
    SDL_Renderer* renderer   = nullptr;
    TTF_Font*     font_title = nullptr;   /* Montserrat, tamano de titulo */
    TTF_Font*     font_body  = nullptr;   /* Montserrat, tamano normal   */
    float         scale      = 1.0f;      /* escala de pantalla (HiDPI)  */
    pie_chart*    chart      = nullptr;   /* la circunferencia de secciones */
};

#ifndef PROFILER_FONT_PATH
#define PROFILER_FONT_PATH "third_party/fonts/Montserrat/static/Montserrat-Regular.ttf"
#endif

}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    SDL_SetAppMetadata("profiler", "0.1", "");

    app_state* state = new app_state();
    *appstate = state;

    /* Antes de inicializar el video: si esta el driver x11 se pide ese, porque
     * en Wayland el cliente no puede elegir ni posicion ni pantalla. */
    bool has_x11_driver = false;
    const int video_driver_count = SDL_GetNumVideoDrivers();
    for (int i = 0; i < video_driver_count; ++i) {
        const char* video_driver_name = SDL_GetVideoDriver(i);
#ifdef DEBUG
        SDL_Log("driver %d: %s", i, video_driver_name);
#endif
        if (SDL_strcmp(video_driver_name, "x11") == 0) {
            has_x11_driver = true;
        }
    }
    if (has_x11_driver) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
#ifdef DEBUG
    SDL_Log("driver de video: %s", SDL_GetCurrentVideoDriver());
#endif
    if (!TTF_Init()) {
        SDL_Log("TTF_Init: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* De las pantallas disponibles se elige la mas grande, por area. */
    SDL_Rect display_bounds;
    int display_count = 0;
    int largest_display_index = -1;
    SDL_DisplayID* displays = SDL_GetDisplays(&display_count);
    if (displays != nullptr) {
#ifdef DEBUG
        SDL_Log("pantallas disponibles: %d", display_count);
#endif

        int largest_area = 0;
        for (int i = 0; i < display_count; ++i) {
            SDL_Rect bounds;
            if (!SDL_GetDisplayBounds(displays[i], &bounds)) {
                continue;
            }
            const int area = bounds.w * bounds.h;
#ifdef DEBUG
            SDL_Log("pantalla %d: id %u, %dx%d en (%d,%d), area %d",
                    i, (unsigned int)displays[i], bounds.w, bounds.h,
                    bounds.x, bounds.y, area);
#endif
            if (area > largest_area) {
                largest_area          = area;
                largest_display_index = i;
            }
        }

        /* Con el indice se sacan las coordenadas de la pantalla elegida. */
        if (largest_display_index < 0) {
            SDL_Log("No display bounds found.");
            SDL_free(displays);
            return SDL_APP_FAILURE;
        }
        SDL_GetDisplayBounds(displays[largest_display_index], &display_bounds);
#ifdef DEBUG
        SDL_Log("elegida: indice %d, %dx%d en (%d,%d)", largest_display_index,
                display_bounds.w, display_bounds.h,
                display_bounds.x, display_bounds.y);
#endif
        SDL_free(displays);
    } else {
        SDL_Log("No window found.");
        return SDL_APP_FAILURE;
    }

    SDL_PropertiesID window_properties = SDL_CreateProperties();
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_X_NUMBER, display_bounds.x);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_Y_NUMBER, display_bounds.y);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, display_bounds.w);
    SDL_SetNumberProperty(window_properties, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, display_bounds.h);
    SDL_SetBooleanProperty(window_properties, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
    SDL_SetStringProperty(window_properties, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "profiler");
    state->window = SDL_CreateWindowWithProperties(window_properties);
    SDL_DestroyProperties(window_properties);
    if (state->window == nullptr) {
        SDL_Log("SDL_CreateWindowWithProperties: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    state->renderer = SDL_CreateRenderer(state->window, nullptr);
    if (state->renderer == nullptr) {
        SDL_Log("SDL_CreateRenderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(state->renderer, 1);

    /* Escala de pantalla: la tipografia se abre en puntos logicos. */
    state->scale = SDL_GetWindowDisplayScale(state->window);
    if (!(state->scale > 0.0f)) {
        state->scale = 1.0f;
    }

    state->font_title = TTF_OpenFont(PROFILER_FONT_PATH, theme::font_size_title * state->scale);
    /* La circunferencia, a partir de las coordenadas de la ventana. */
    SDL_Rect window_rect = { 0, 0, 0, 0 };
    SDL_GetWindowSize(state->window, &window_rect.w, &window_rect.h);
    std::vector<std::pair<const char*, double>> sections = {
        { "name1", 0.10 },
        { "name2", 0.15 },
        { "name3", 0.20 },
        { "name4", 0.25 },
        { "name5", 0.30 },
    };
    state->chart = new pie_chart(window_rect, sections);

    state->font_body  = TTF_OpenFont(PROFILER_FONT_PATH, theme::font_size_base * state->scale);
    if (state->font_title == nullptr || state->font_body == nullptr) {
        SDL_Log("TTF_OpenFont(%s): %s", PROFILER_FONT_PATH, SDL_GetError());
    }

#ifdef DEBUG
    {
        int window_width = 0, window_height = 0;
        int window_x = 0, window_y = 0;
        SDL_GetWindowSize(state->window, &window_width, &window_height);
        SDL_GetWindowPosition(state->window, &window_x, &window_y);
        SDL_Log("ventana: %dx%d en (%d,%d)", window_width, window_height,
                window_x, window_y);
    }
#endif

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        app_state* state = (app_state*)appstate;
        /* Tecla 1: separar/juntar. Tecla 2: activar/desactivar crecimiento. */
        if (!event->key.repeat && state != nullptr && state->chart != nullptr) {
            if (event->key.key == SDLK_1) {
                state->chart->toggle_separation();
            } else if (event->key.key == SDLK_2) {
                state->chart->toggle_growing();
            }
        }

        if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q) {
            return SDL_APP_SUCCESS;
        }
    }

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    app_state* state = (app_state*)appstate;

    SDL_SetRenderDrawColor(state->renderer,
                           theme::color_background.r, theme::color_background.g,
                           theme::color_background.b, theme::color_background.a);
    SDL_RenderClear(state->renderer);
    state->chart->update();
    state->chart->draw(state->renderer);
    SDL_RenderPresent(state->renderer);

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    app_state* state = (app_state*)appstate;
    if (state == nullptr) {
        return;
    }

    if (state->font_title != nullptr) {
        TTF_CloseFont(state->font_title);
    }
    if (state->font_body  != nullptr) {
        TTF_CloseFont(state->font_body);
    }
    if (state->renderer   != nullptr) {
        SDL_DestroyRenderer(state->renderer);
    }
    if (state->window     != nullptr) {
        SDL_DestroyWindow(state->window);
    }
    delete state->chart;
    delete state;

    TTF_Quit();
    SDL_Quit();
}
