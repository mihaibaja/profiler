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

#include <algorithm>
#include <iterator>
#include <memory>
#include <numeric>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "pie_chart.hpp"
#include "theme.hpp"

namespace {

/* Nodo del arbol: guarda su color, su posicion en el chart del padre y el
 * chart que se muestra al entrar en el. */
struct chart_node {
    std::string name;
    SDL_Color   color = { 0, 0, 0, 0 };
    int         level = 1;
    int         parent_sector_index = -1;
    chart_node* parent = nullptr;
    std::vector<std::unique_ptr<chart_node>> children;
    std::unique_ptr<pie_chart> chart;
};

enum class transition_phase {
    idle,
    collapsing,   /* juntar + expandir el sector pulsado */
    revealing     /* separar + devolver el sector a su proporcion */
};

/* Todo lo que vive mientras el programa corre. */
struct app_state {
    SDL_Window*   window     = nullptr;
    SDL_Renderer* renderer   = nullptr;
    TTF_Font*     font_title = nullptr;   /* Montserrat, tamano de titulo */
    TTF_Font*     font_body  = nullptr;   /* Montserrat, tamano normal   */
    float         scale      = 1.0f;      /* escala de pantalla (HiDPI)  */

    std::unique_ptr<chart_node> root;
    chart_node* current = nullptr;
    chart_node* pending = nullptr;
    transition_phase phase = transition_phase::idle;
    bool going_forward = false;
};

#ifndef PROFILER_FONT_PATH
#define PROFILER_FONT_PATH "third_party/fonts/Montserrat/static/Montserrat-Regular.ttf"
#endif

bool same_color(SDL_Color a, SDL_Color b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

int random_int(std::mt19937& rng, int minimum, int maximum)
{
    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(rng);
}

/* Elige `count` colores distintos de la paleta, evitando los colores
 * prohibidos (por ejemplo, el color propio del nodo). */
std::vector<SDL_Color> pick_colors(std::mt19937& rng, int count,
                                   const std::vector<SDL_Color>& forbidden)
{
    std::vector<int> palette(std::size(theme::color_sections));
    std::iota(palette.begin(), palette.end(), 0);
    std::shuffle(palette.begin(), palette.end(), rng);

    std::vector<SDL_Color> result;
    result.reserve((size_t)count);

    for (int index : palette) {
        const SDL_Color candidate = theme::color_sections[index];
        bool used = false;
        for (const SDL_Color color : forbidden) {
            used = used || same_color(candidate, color);
        }
        for (const SDL_Color color : result) {
            used = used || same_color(candidate, color);
        }
        if (!used) {
            result.push_back(candidate);
            if ((int)result.size() == count) {
                break;
            }
        }
    }

    return result;
}

/* Proporciones aleatorias positivas que suman exactamente 1. */
std::vector<double> random_proportions(std::mt19937& rng, int count)
{
    std::uniform_real_distribution<double> distribution(0.45, 1.0);
    std::vector<double> weights((size_t)count);
    double total = 0.0;
    for (double& weight : weights) {
        weight = distribution(rng);
        total += weight;
    }

    for (double& weight : weights) {
        weight /= total;
    }

    /* La suma de los redondeos puede quedar ligeramente por debajo de 1;
     * se ajusta la ultima proporcion para cerrar el circulo. */
    if (count > 0) {
        double accumulated = 0.0;
        for (int i = 0; i < count - 1; ++i) {
            accumulated += weights[(size_t)i];
        }
        weights[(size_t)(count - 1)] =
            std::max(0.0, 1.0 - accumulated);
    }

    return weights;
}

std::unique_ptr<chart_node> build_node(std::mt19937& rng,
                                       const std::string& name, int level,
                                       SDL_Color color, chart_node* parent,
                                       int parent_sector_index,
                                       const SDL_Rect& window)
{
    auto node = std::make_unique<chart_node>();
    node->name        = name;
    node->color       = color;
    node->level       = level;
    node->parent      = parent;
    node->parent_sector_index = parent_sector_index;

    std::vector<pie_chart::section_spec> specs;

    if (level < 3) {
        /* Niveles 1 y 2: cada hijo es un nodo con su propio chart. */
        const int child_count = (level == 1) ? random_int(rng, 2, 4)
                                             : random_int(rng, 1, 3);

        std::vector<SDL_Color> forbidden;
        if (level > 1) {
            forbidden.push_back(node->color);
        }
        const std::vector<SDL_Color> child_colors =
            pick_colors(rng, child_count, forbidden);

        for (int i = 0; i < child_count; ++i) {
            const int sector_index = (level == 1) ? i : i + 1;
            const std::string child_name =
                node->name + "." + std::to_string(i + 1);
            auto child = build_node(rng, child_name, level + 1,
                                    child_colors[(size_t)i], node.get(),
                                    sector_index, window);
            node->children.push_back(std::move(child));
        }

        const int sector_count = (level == 1) ? child_count : child_count + 1;
        const std::vector<double> proportions =
            random_proportions(rng, sector_count);

        int position = 0;
        if (level > 1) {
            specs.push_back({ node->name, proportions[(size_t)position++],
                              node->color });
        }
        for (const std::unique_ptr<chart_node>& child : node->children) {
            specs.push_back({ child->name, proportions[(size_t)position++],
                              child->color });
        }
    } else {
        /* Nivel 3: hoja. Tiene su sector propio y sectores hoja que no
         * navegan a ningun chart hijo. */
        const int leaf_count = random_int(rng, 1, 3);
        const std::vector<SDL_Color> leaf_colors =
            pick_colors(rng, leaf_count, { node->color });
        const std::vector<double> proportions =
            random_proportions(rng, leaf_count + 1);

        specs.push_back({ node->name, proportions[0], node->color });
        for (int i = 0; i < leaf_count; ++i) {
            const std::string leaf_name =
                node->name + ".hoja" + std::to_string(i + 1);
            specs.push_back({ leaf_name,
                              proportions[(size_t)(i + 1)],
                              leaf_colors[(size_t)i] });
        }
    }

#ifdef DEBUG
    SDL_Log("nodo %s (nivel %d) color #%02x%02x%02x, %zu secciones",
            node->name.c_str(), node->level, node->color.r, node->color.g,
            node->color.b, specs.size());
    for (const pie_chart::section_spec& spec : specs) {
        SDL_Log("  sector %-12s prop %.4f color #%02x%02x%02x",
                spec.name.c_str(), spec.proportion, spec.color.r,
                spec.color.g, spec.color.b);
    }
#endif

    node->chart = std::make_unique<pie_chart>(window, specs);
    return node;
}

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
    /* Arbol aleatorio de tres niveles. El chart raiz contiene un sector por
     * cada hijo de nivel 2; los charts de nivel 2 anaden su sector propio
     * (color heredado) y los charts de nivel 3 son hojas. */
    SDL_Rect window_rect = { 0, 0, 0, 0 };
    SDL_GetWindowSize(state->window, &window_rect.w, &window_rect.h);

    std::mt19937 rng(std::random_device{}());
    state->root = build_node(rng, "raiz", 1, theme::color_sections[0],
                             nullptr, -1, window_rect);
    state->current = state->root.get();

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
    app_state* state = (app_state*)appstate;

    if (event->type == SDL_EVENT_QUIT) {
        return SDL_APP_SUCCESS;
    }

    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_ESCAPE || event->key.key == SDLK_Q) {
            return SDL_APP_SUCCESS;
        }
    }

    if (event->type != SDL_EVENT_MOUSE_BUTTON_DOWN || state == nullptr ||
        state->current == nullptr || state->phase != transition_phase::idle) {
        return SDL_APP_CONTINUE;
    }

    /* El click derecho vuelve al padre desde cualquier punto de la ventana. */
    if (event->button.button == SDL_BUTTON_RIGHT) {
        if (state->current->parent != nullptr) {
#ifdef DEBUG
            SDL_Log("click derecho: %s -> %s",
                    state->current->name.c_str(),
                    state->current->parent->name.c_str());
#endif
            state->pending       = state->current->parent;
            state->going_forward = false;
            state->phase         = transition_phase::collapsing;
            state->current->chart->collapse_to_sector(0);
        }
        return SDL_APP_CONTINUE;
    }

    if (event->button.button != SDL_BUTTON_LEFT) {
        return SDL_APP_CONTINUE;
    }

    const int index = state->current->chart->section_at(event->button.x,
                                                        event->button.y);
    if (index < 0) {
        return SDL_APP_CONTINUE;
    }

    chart_node* next = nullptr;
    if (state->current == state->root.get()) {
        if (index < (int)state->current->children.size()) {
            next = state->current->children[(size_t)index].get();
        }
    } else if (index > 0 &&
               index - 1 < (int)state->current->children.size()) {
        next = state->current->children[(size_t)(index - 1)].get();
    }

    if (next != nullptr) {
#ifdef DEBUG
        SDL_Log("click izquierdo: %s -> %s (sector %d)",
                state->current->name.c_str(), next->name.c_str(), index);
#endif
        state->pending       = next;
        state->going_forward = true;
        state->phase         = transition_phase::collapsing;
        state->current->chart->collapse_to_sector(index);
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

    if (state->current == nullptr) {
        return SDL_APP_CONTINUE;
    }

    /* Primero avanza la animacion del chart visible. */
    state->current->chart->update();

    /* Al terminar de juntar + expandir se cambia de nodo. El chart nuevo
     * arranca mostrando el sector heredado al 100 % y todo junto; despues se
     * separa y ese sector vuelve a su proporcion fija. */
    if (state->phase == transition_phase::collapsing &&
        !state->current->chart->animations_running()) {
        chart_node* from = state->current;
        chart_node* next = state->pending;

        if (next != nullptr) {
            state->current = next;
            state->current->chart->clear_hover();

            if (state->going_forward) {
                state->current->chart->reveal_from_sector(0);
            } else {
                state->current->chart->reveal_from_sector(
                    from->parent_sector_index);
            }
            state->phase = transition_phase::revealing;
        } else {
            state->phase = transition_phase::idle;
        }
        state->pending = nullptr;
    } else if (state->phase == transition_phase::revealing &&
               !state->current->chart->animations_running()) {
        state->phase = transition_phase::idle;
    }

    /* El hover solo se actualiza cuando no hay una transicion de nivel en
     * curso; durante ella el resaltado se mantiene desactivado. */
    if (state->phase == transition_phase::idle) {
        if (SDL_GetMouseFocus() == state->window) {
            float mouse_x = 0.0f;
            float mouse_y = 0.0f;
            SDL_GetMouseState(&mouse_x, &mouse_y);
            state->current->chart->update_hover(mouse_x, mouse_y);
        } else {
            state->current->chart->clear_hover();
        }
    } else {
        state->current->chart->clear_hover();
    }

    state->current->chart->draw(state->renderer);
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
    delete state;

    TTF_Quit();
    SDL_Quit();
}
