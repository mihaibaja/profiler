/*
 * pie_chart.hpp — Tarta de secciones.
 *
 */
#ifndef PROFILER_PIE_CHART_HPP
#define PROFILER_PIE_CHART_HPP

#include <SDL3/SDL.h>

#include <string>
#include <utility>
#include <vector>

class pie_chart {
public:
    /* Especificacion de una seccion: nombre, proporcion permanente y color.
     * El color se pasa explicitamente; el chart no elige ninguno por indice. */
    struct section_spec {
        std::string name;
        double      proportion = 0.0;
        SDL_Color   color      = { 0, 0, 0, 0 };
    };

    /* Cada porcion. El constructor guarda sus parametros y calcula los
     * vertices; calculate_vertices permite recalcularlos cuando avanzan las
     * animaciones. El centro se calcula desde el centro del chart, desplazado
     * hasta un 5 % del radio en la direccion del angulo medio de la seccion. */
    struct section {
        std::string name;
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

        /* Animacion de hover: 0 = color permanente, 1 = resaltado completo.
         * El booleano hover_active indica si la transicion esta en curso. */
        double hover_state    = 0.0;
        double hover_from     = 0.0;
        double hover_to       = 0.0;
        Uint64 hover_start_ms = 0;
        bool   hover_active   = false;

        /* Contorno negro del hover. Se dibuja despues de todos los rellenos
         * para que las secciones vecinas no lo tapen. */
        std::vector<SDL_Vertex> border_vertices;
        std::vector<int>        border_indices;

        section(const std::string& new_name, const pie_chart& chart,
                float radius, float angle_start, float angle_end,
                SDL_Color color);
        ~section();

        void calculate_vertices(const pie_chart& chart, double separation_state);
    };

    /* Especificacion: la suma de las proporciones no debe superar 1.0. Cada
     * elemento genera una seccion con su nombre, su proporcion y su color. */
    pie_chart(SDL_Rect window,
              const std::vector<section_spec>& new_sections);

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

    /* Transicion de entrada a un hijo: junta las secciones hacia el centro y
     * expande `index` hasta ocupar el 100 %. Ambas animaciones van a la vez. */
    void collapse_to_sector(int index);

    /* Estado inicial del chart hijo: `index` al 100 % y todo junto en el
     * centro. Despues arranca la transicion inversa (separar y devolver la
     * seccion a su proporcion fija) para revelar los demas sectores. */
    void reveal_from_sector(int index);

    /* true mientras la separacion o el crecimiento estan en curso. El hover
     * no cuenta para esta comprobacion. */
    bool animations_running() const;

    /* Anima el resaltado de una seccion. hovered = true lo activa y false lo
     * desactiva; si se activa una seccion, las demas se desactivan. */
    void set_section_hover(int index, bool hovered);

    /* Atajo para el raton: resuelve la seccion bajo (x, y) y actualiza el
     * resaltado; si no hay ninguna, lo desactiva todo. */
    void update_hover(float x, float y);

    /* Desactiva el resaltado de todas las secciones. */
    void clear_hover();

    /* Indice de la seccion que contiene el punto, o -1 si ninguna. */
    int section_at(float x, float y) const;

    void draw(SDL_Renderer* renderer) const;

private:
    void update_separation();
    void update_proportions();
    void update_hover_animation();
    void recalculate_geometry();
    void start_growing(int index, bool to_full);
    double current_growth_amount() const;
    void set_expanded_state(int index);

    SDL_FPoint           center = { 0.0f, 0.0f };
    float                radius = 0.0f;
    std::vector<section> sections;
    int                  hovered_section = -1;   /* ultima seccion bajo el cursor */

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
