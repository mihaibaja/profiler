/*
 * pie_chart.hpp — Tarta de secciones.
 *
 */
#ifndef PROFILER_PIE_CHART_HPP
#define PROFILER_PIE_CHART_HPP

#include <SDL3/SDL.h>

#include <utility>
#include <vector>

class pie_chart {
public:
    /* Cada porcion. El constructor guarda sus parametros y calcula los
     * vertices; calculate_vertices permite recalcularlos cuando avanzan las
     * animaciones. El centro se calcula desde el centro del chart, desplazado
     * hasta un 5 % del radio en la direccion del angulo medio de la seccion. */
    struct section {
        const char* name = "";
        double fixed_proportion     = 0.0;   /* proporcion permanente */
        double temporary_proportion = 0.0;   /* proporcion animada */
        SDL_FPoint center = { 0.0f, 0.0f };
        std::vector<SDL_Vertex> vertices;
        std::vector<int>        indices;

        /* Parametros de la porcion, necesarios para reconstruir la geometria
         * en cada fotograma de la animacion. */
        float     radius      = 0.0f;
        float     angle_start = 0.0f;
        float     angle_end   = 0.0f;
        SDL_Color color       = { 0, 0, 0, 0 };

        section(const char* new_name, const pie_chart& chart, float radius,
                float angle_start, float angle_end, SDL_Color color);
        ~section();

        void calculate_vertices(const pie_chart& chart, double separation_state);
    };

    /* Especificacion: la suma de las proporciones no debe superar 1.0. Cada
     * par (nombre, proporcion) genera una seccion. El color de cada seccion se
     * toma de theme::color_sections en orden. */
    pie_chart(SDL_Rect window,
              const std::vector<std::pair<const char*, double>>& new_sections);

    /* Avanza las animaciones y recalcula los vertices de las secciones.
     * Si no hay ninguna animacion en curso no hace nada. */
    void update();

    /* Anima la separacion radial durante el tiempo definido en theme.hpp.
     * outward = true separa; outward = false junta.
     * No se llama por defecto: las secciones quedan separadas. */
    void separate(bool outward);

    /* Alterna el sentido: separar o juntar. */
    void toggle_separation();

    /* Alterna el sentido: crecer al 100 % o volver a la proporcion fija. */
    void toggle_growing();

    /* Elige la seccion que crece (indice dentro de sections). */
    void set_growing_sector(int index);

    void draw(SDL_Renderer* renderer) const;

private:
    void update_separation();
    void update_proportions();

    SDL_FPoint           center = { 0.0f, 0.0f };
    float                radius = 0.0f;
    std::vector<section> sections;

    /* Animacion de proporciones: una transicion de una sola pasada por toggle.
     * growing_direction indica el sentido: true = al 100 %, false = a la fija. */
    int    growing_sector   = 0;
    double growing_state    = 1.0;   /* 0 = inicio, 1 = final; inicial colapsado */
    double growing_progress_offset = 0.0;
    Uint64 growing_start_ms = 0;
    bool   growing_direction = false;
    bool   growing_active    = false;

    /* Animacion de separacion radial, controlada por separate(). */
    double separation_state    = 1.0;   /* 1 = separado por defecto */
    double separation_from     = 1.0;
    double separation_to       = 1.0;
    Uint64 separation_start_ms = 0;
    bool   separation_active   = false;
};

#endif // PROFILER_PIE_CHART_HPP
