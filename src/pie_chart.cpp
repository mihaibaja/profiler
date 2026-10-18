/*
 * pie_chart.cpp — Tarta de secciones: circulo relleno.
 *
 */
#include "pie_chart.hpp"

#include "theme.hpp"

#include <algorithm>
#include <cmath>

namespace {

constexpr float two_pi  = 6.283185307179586f;
constexpr float feather = 1.5f;    /* banda de suavizado en el borde exterior */

/* Separacion angular de la seccion: un vertice cada ~10 px de arco, con un
 * minimo de 8. Es lo que fija el tamano de los vectores. */
int steps_for(float radius, float sweep)
{
    return std::max(8, (int)(std::fabs(sweep) * radius * 0.1f));
}

SDL_FColor to_fcolor(SDL_Color color, float alpha)
{
    return { (float)color.r / 255.0f, (float)color.g / 255.0f,
             (float)color.b / 255.0f, alpha };
}

}

pie_chart::section::section(const char* new_name, const pie_chart& chart,
                            float new_radius, float new_angle_start,
                            float new_angle_end, SDL_Color new_color)
{
    name        = new_name;
    radius      = new_radius;
    angle_start = new_angle_start;
    angle_end   = new_angle_end;
    color       = new_color;

    calculate_vertices(chart, chart.separation_state);
}

/*
 * Calcula los vertices del sector: cuerpo (abanico desde su centro), borde
 * exterior suavizado, bordes rectos y esquinas. Los vectores se dimensionan
 * de una vez, despues se rellenan por indice.
 */
void pie_chart::section::calculate_vertices(const pie_chart& chart,
                                             double separation_state)
{
    /* Centro de la seccion: el del chart desplazado hasta un 5 % del radio
     * hacia el angulo medio de la porcion, segun el estado de separacion. */
    const float mid_angle  = (angle_start + angle_end) * 0.5f;
    const float separation = 0.05f * radius * (float)separation_state;
    center = { chart.center.x + separation * std::cos(mid_angle),
               chart.center.y + separation * std::sin(mid_angle) };

    const float sweep = angle_end - angle_start;
    const int   steps = steps_for(radius, sweep);

    const int body_count   = steps + 2;          /* centro + arco */
    const int border_count = 2 * (steps + 1);    /* interior y exterior */
    const int edge_vertices = 8;                 /* dos bandas de 4 vertices */
    const int edge_indices  = 18;                /* bandas + esquinas */
    vertices.resize((size_t)(body_count + border_count + edge_vertices));
    indices.resize((size_t)(3 * steps + 6 * steps + edge_indices));

    const SDL_FColor fill = to_fcolor(color, 1.0f);
    const SDL_FColor fade = to_fcolor(color, 0.0f);
    const SDL_FPoint origin = { 0.0f, 0.0f };

    /* Cuerpo: abanico desde el centro de la seccion. */
    vertices[0].position  = center;
    vertices[0].color     = fill;
    vertices[0].tex_coord = origin;
    for (int i = 0; i <= steps; ++i) {
        const float angle = angle_start + sweep * (float)i / (float)steps;
        SDL_Vertex& vertex = vertices[(size_t)(1 + i)];
        vertex.position  = { center.x + radius * std::cos(angle),
                             center.y + radius * std::sin(angle) };
        vertex.color     = fill;
        vertex.tex_coord = origin;
    }
    for (int i = 0; i < steps; ++i) {
        indices[(size_t)(3 * i + 0)] = 0;
        indices[(size_t)(3 * i + 1)] = i + 1;
        indices[(size_t)(3 * i + 2)] = i + 2;
    }

    /* Borde exterior: del radio a radio + feather, con alfa 1 -> 0. */
    const int base = body_count;
    for (int i = 0; i <= steps; ++i) {
        const float angle   = angle_start + sweep * (float)i / (float)steps;
        const float cosinus = std::cos(angle);
        const float sinus   = std::sin(angle);

        SDL_Vertex& inner = vertices[(size_t)(base + 2 * i)];
        inner.position  = { center.x + radius * cosinus,
                            center.y + radius * sinus };
        inner.color     = fill;
        inner.tex_coord = origin;

        SDL_Vertex& outer = vertices[(size_t)(base + 2 * i + 1)];
        outer.position  = { center.x + (radius + feather) * cosinus,
                            center.y + (radius + feather) * sinus };
        outer.color     = fade;
        outer.tex_coord = origin;
    }
    for (int i = 0; i < steps; ++i) {
        const int current = base + 2 * i;
        const int next    = current + 2;
        int* quad = &indices[(size_t)(3 * steps + 6 * i)];
        quad[0] = current;
        quad[1] = current + 1;
        quad[2] = next + 1;
        quad[3] = current;
        quad[4] = next + 1;
        quad[5] = next;
    }

    /* Bordes rectos: una banda de suavizado por cada radio del sector. La
     * arista interior queda opaca y la exterior se desvanece, igual que en el
     * arco. */
    auto add_edge_band = [&](float angle, float normal_sign, int vertex_base,
                             int index_base) {
        const float cosinus = std::cos(angle);
        const float sinus   = std::sin(angle);
        const float nx      = normal_sign * -sinus;
        const float ny      = normal_sign *  cosinus;

        const SDL_FPoint inner_end = { center.x + radius * cosinus,
                                       center.y + radius * sinus };

        SDL_Vertex& inner_start = vertices[(size_t)(vertex_base + 0)];
        inner_start.position  = center;
        inner_start.color     = fill;
        inner_start.tex_coord = origin;

        SDL_Vertex& outer_start = vertices[(size_t)(vertex_base + 1)];
        outer_start.position  = { center.x + feather * nx,
                                  center.y + feather * ny };
        outer_start.color     = fade;
        outer_start.tex_coord = origin;

        SDL_Vertex& inner_end_vertex = vertices[(size_t)(vertex_base + 2)];
        inner_end_vertex.position  = inner_end;
        inner_end_vertex.color     = fill;
        inner_end_vertex.tex_coord = origin;

        SDL_Vertex& outer_end = vertices[(size_t)(vertex_base + 3)];
        outer_end.position  = { inner_end.x + feather * nx,
                                inner_end.y + feather * ny };
        outer_end.color     = fade;
        outer_end.tex_coord = origin;

        indices[(size_t)(index_base + 0)] = vertex_base + 0;
        indices[(size_t)(index_base + 1)] = vertex_base + 1;
        indices[(size_t)(index_base + 2)] = vertex_base + 3;
        indices[(size_t)(index_base + 3)] = vertex_base + 0;
        indices[(size_t)(index_base + 4)] = vertex_base + 3;
        indices[(size_t)(index_base + 5)] = vertex_base + 2;
    };

    const int edge_vertex_base = body_count + border_count;
    const int edge_index_base  = 3 * steps + 6 * steps;
    add_edge_band(angle_start, -1.0f, edge_vertex_base, edge_index_base);
    add_edge_band(angle_end,    1.0f, edge_vertex_base + 4, edge_index_base + 6);

    /* Esquinas entre cada borde recto y el arco exterior: completan el
     * degradado sin dejar un punto duro en el encuentro. */
    int* corner = &indices[(size_t)(edge_index_base + 12)];
    corner[0] = edge_vertex_base + 2;       /* arista interior, inicio */
    corner[1] = edge_vertex_base + 3;       /* borde exterior, inicio */
    corner[2] = base + 1;                   /* arco exterior, inicio */

    corner[3] = edge_vertex_base + 6;       /* arista interior, fin */
    corner[4] = base + 2 * steps + 1;       /* arco exterior, fin */
    corner[5] = edge_vertex_base + 7;       /* borde exterior, fin */
}

pie_chart::section::~section()
{
    vertices.clear();
    indices.clear();
}

pie_chart::pie_chart(
    SDL_Rect window,
    const std::vector<std::pair<const char*, double>>& new_sections)
{
    center = { (float)window.x + (float)window.w * 0.5f,
               (float)window.y + (float)window.h * 0.5f };
    radius = 0.4f * (float)std::min(window.w, window.h);
    growing_start_ms = SDL_GetTicks();

    /* Cada par (nombre, proporcion) genera su seccion una sola vez. */
    sections.clear();
    sections.reserve(new_sections.size());
    float angle = 0.0f;
    for (size_t i = 0; i < new_sections.size(); ++i) {
        const char*     name  = new_sections[i].first;
        const double    share = new_sections[i].second;
        const float     sweep = (float)share * two_pi;
        const SDL_Color color = theme::color_sections[i % 10];
        sections.emplace_back(name, *this, radius, angle, angle + sweep, color);
        sections.back().fixed_proportion     = share;
        sections.back().temporary_proportion = share;
        angle += sweep;
    }
}

void pie_chart::update()
{
    /* Solo se recalcula la geometria mientras alguna animacion esta en curso. */
    const bool in_animation = separation_active || growing_active;
    if (!in_animation) {
        return;
    }

    if (separation_active) {
        update_separation();
    }
    if (growing_active) {
        update_proportions();
    }

    /* Recalcula los angulos con las proporciones temporales y reconstruye la
     * geometria de cada seccion. */
    float angle = 0.0f;
    for (section& current : sections) {
        const float sweep = (float)current.temporary_proportion * two_pi;
        current.angle_start = angle;
        current.angle_end   = angle + sweep;
        current.calculate_vertices(*this, separation_state);
        angle += sweep;
    }
}

void pie_chart::separate(bool outward)
{
    separation_from     = separation_state;
    separation_to       = outward ? 1.0 : 0.0;
    separation_start_ms = SDL_GetTicks();
    separation_active   = true;
}

void pie_chart::toggle_separation()
{
    const bool separated = separation_active ? (separation_to >= 0.5)
                                             : (separation_state >= 0.5);
    separate(!separated);
}

void pie_chart::toggle_growing()
{
    /* Cambia el sentido y continua desde el progreso actual. */
    growing_direction       = !growing_direction;
    growing_progress_offset = 1.0 - growing_state;
    growing_start_ms        = SDL_GetTicks();
    growing_active          = true;
}

void pie_chart::set_growing_sector(int index)
{
    if (sections.empty()) {
        growing_sector = 0;
        return;
    }
    growing_sector = std::clamp(index, 0, (int)sections.size() - 1);
}

void pie_chart::update_separation()
{
    if (!separation_active) {
        return;
    }

    constexpr double duration_ms = theme::animation_separation_ms;
    const double elapsed_ms = (double)(SDL_GetTicks() - separation_start_ms);
    const double progress   = elapsed_ms / duration_ms;

    if (progress >= 1.0) {
        separation_state  = separation_to;
        separation_active = false;
    } else {
        separation_state = separation_from
                         + (separation_to - separation_from) * progress;
    }
}

void pie_chart::update_proportions()
{
    constexpr double duration_ms = theme::animation_growing_half_period_ms;

    const double elapsed_ms = (double)(SDL_GetTicks() - growing_start_ms);
    double progress = growing_progress_offset + elapsed_ms / duration_ms;
    if (progress >= 1.0) {
        progress       = 1.0;
        growing_active = false;
    }
    growing_state = progress;

    const bool growing = growing_direction;

    if (sections.empty()) {
        return;
    }
    if (growing_sector < 0 || growing_sector >= (int)sections.size()) {
        for (section& current : sections) {
            current.temporary_proportion = current.fixed_proportion;
        }
        return;
    }

    const int    index         = growing_sector;
    const double fixed_growing = sections[(size_t)index].fixed_proportion;

    /* Suma de las proporciones fijas de las demas secciones. */
    double fixed_others = 0.0;
    for (size_t i = 0; i < sections.size(); ++i) {
        if ((int)i != index) {
            fixed_others += sections[i].fixed_proportion;
        }
    }

    for (size_t i = 0; i < sections.size(); ++i) {
        if ((int)i == index) {
            /* La seccion elegida pasa de su proporcion fija al 100 %, o al
             * reves, segun el booleano growing. */
            sections[i].temporary_proportion = growing
                ? fixed_growing + (1.0 - fixed_growing) * growing_state
                : 1.0 - (1.0 - fixed_growing) * growing_state;
        } else {
            /* Las demas se reparten (100 % - fija de la que crece) de forma
             * proporcional a su proporcion fija local. */
            const double delta = (fixed_others > 0.0)
                ? (1.0 - fixed_growing) * sections[i].fixed_proportion / fixed_others
                : 0.0;
            sections[i].temporary_proportion = growing
                ? sections[i].fixed_proportion - delta * growing_state
                : delta * growing_state;
        }
        sections[i].temporary_proportion =
            std::clamp(sections[i].temporary_proportion, 0.0, 1.0);
    }
}

void pie_chart::draw(SDL_Renderer* renderer) const
{
    if (renderer == nullptr) {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    for (const section& current : sections) {
        SDL_RenderGeometry(renderer, nullptr, current.vertices.data(),
                           (int)current.vertices.size(),
                           current.indices.data(), (int)current.indices.size());
    }
}
