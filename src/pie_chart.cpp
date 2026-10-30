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

/* Aplica la formula de brillo del hover: C1 = C0 + (255 - C0) * factor *
 * estado. Se mantiene en flotante para que la transicion sea continua. */
SDL_FColor to_fcolor(SDL_Color color, float alpha, double hover_state)
{
    const float lift = (float)std::clamp(hover_state, 0.0, 1.0)
                     * theme::hover_brightness_factor;
    auto channel = [lift](Uint8 value) {
        return ((float)value + (255.0f - (float)value) * lift) / 255.0f;
    };
    return { channel(color.r), channel(color.g), channel(color.b), alpha };
}

}

pie_chart::section::section(const std::string& new_name, const pie_chart& chart,
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

    const double     hovered_state = std::clamp(hover_state, 0.0, 1.0);
    const SDL_FColor fill = to_fcolor(color, 1.0f, hovered_state);
    const SDL_FColor fade = to_fcolor(color, 0.0f, hovered_state);
    const SDL_FPoint origin = { 0.0f, 0.0f };

    border_vertices.clear();
    border_indices.clear();

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

    /* Contorno del hover: un nucleo negro del ancho configurado, centrado
     * en el arco exterior y en los dos radios, con una banda de pluma a cada
     * lado. El alfa del nucleo crece con el estado y la pluma va de alfa 0 al
     * del nucleo, de modo que el contorno aparece suavizado y queda
     * completamente negro al final de la transicion. */
    if (hovered_state > 0.0) {
        const float half_width = theme::hover_border_width * 0.5f;
        const SDL_FColor stroke = { 0.0f, 0.0f, 0.0f,
                                    (float)hovered_state };
        const SDL_FColor fade   = { 0.0f, 0.0f, 0.0f, 0.0f };

        /* Cuatro vertices por paso angular: pluma interior, nucleo (dos
         * vertices con el mismo alfa) y pluma exterior. */
        border_vertices.reserve((size_t)(4 * (steps + 1) + 16));
        border_indices.reserve((size_t)(18 * steps + 36));

        auto add_vertex = [&](SDL_FPoint position, SDL_FColor color) {
            SDL_Vertex vertex;
            vertex.position  = position;
            vertex.color     = color;
            vertex.tex_coord = origin;
            border_vertices.push_back(vertex);
            return (int)border_vertices.size() - 1;
        };
        auto add_quad = [&](int a, int b, int c, int d) {
            border_indices.push_back(a);
            border_indices.push_back(b);
            border_indices.push_back(c);
            border_indices.push_back(a);
            border_indices.push_back(c);
            border_indices.push_back(d);
        };

        /* Arco exterior: pluma, nucleo, pluma. */
        const float radii[4] = {
            radius - half_width - feather,
            radius - half_width,
            radius + half_width,
            radius + half_width + feather
        };
        const SDL_FColor colors[4] = { fade, stroke, stroke, fade };

        int previous[4] = { -1, -1, -1, -1 };
        for (int i = 0; i <= steps; ++i) {
            const float angle   = angle_start + sweep * (float)i / (float)steps;
            const float cosinus = std::cos(angle);
            const float sinus   = std::sin(angle);

            int current[4];
            for (int band = 0; band < 4; ++band) {
                current[band] = add_vertex(
                    { center.x + radii[band] * cosinus,
                      center.y + radii[band] * sinus },
                    colors[band]);
            }

            if (i > 0) {
                for (int band = 0; band < 3; ++band) {
                    add_quad(previous[band], previous[band + 1],
                             current[band + 1], current[band]);
                }
            }
            for (int band = 0; band < 4; ++band) {
                previous[band] = current[band];
            }
        }

        /* Radios: nucleo centrado en cada arista recta, con pluma a ambos
         * lados. Se prolonga hasta radio + mitad para encontrarse con el
         * arco. */
        const float offsets[4] = {
            -(half_width + feather), -half_width,
             half_width,              half_width + feather
        };

        auto add_side_border = [&](float angle) {
            const float cosinus = std::cos(angle);
            const float sinus   = std::sin(angle);
            const float normal_x = -sinus;
            const float normal_y =  cosinus;
            const float end_radius = radius + half_width;

            int start[4];
            int end[4];
            for (int band = 0; band < 4; ++band) {
                const float offset_x = normal_x * offsets[band];
                const float offset_y = normal_y * offsets[band];
                start[band] = add_vertex(
                    { center.x + offset_x, center.y + offset_y },
                    colors[band]);
                end[band] = add_vertex(
                    { center.x + end_radius * cosinus + offset_x,
                      center.y + end_radius * sinus + offset_y },
                    colors[band]);
            }
            for (int band = 0; band < 3; ++band) {
                add_quad(start[band], end[band],
                         end[band + 1], start[band + 1]);
            }
        };

        add_side_border(angle_start);
        add_side_border(angle_end);
    }
}

pie_chart::section::~section()
{
    vertices.clear();
    indices.clear();
}

pie_chart::pie_chart(
    SDL_Rect window,
    const std::vector<section_spec>& new_sections)
{
    center = { (float)window.x + (float)window.w * 0.5f,
               (float)window.y + (float)window.h * 0.5f };
    radius = 0.4f * (float)std::min(window.w, window.h);
    growing_start_ms = SDL_GetTicks();

    /* Cada especificacion genera su seccion con el color recibido. */
    sections.clear();
    sections.reserve(new_sections.size());
    float angle = 0.0f;
    for (const section_spec& spec : new_sections) {
        const float sweep = (float)spec.proportion * two_pi;
        sections.emplace_back(spec.name, *this, radius, angle,
                              angle + sweep, spec.color);
        sections.back().fixed_proportion     = spec.proportion;
        sections.back().temporary_proportion = spec.proportion;
        angle += sweep;
    }
}

void pie_chart::update()
{
    /* Solo se recalcula la geometria mientras alguna animacion esta en curso. */
    bool in_animation = separation_active || growing_active;
    for (const section& current : sections) {
        if (current.hover_active) {
            in_animation = true;
            break;
        }
    }
    if (!in_animation) {
        return;
    }

    if (separation_active) {
        update_separation();
    }
    if (growing_active) {
        update_proportions();
    }
    update_hover_animation();

    recalculate_geometry();
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

void pie_chart::collapse_to_sector(int index)
{
    if (index < 0 || index >= (int)sections.size()) {
        return;
    }

    clear_hover();
    separate(false);
    start_growing(index, true);
}

void pie_chart::reveal_from_sector(int index)
{
    if (sections.empty()) {
        return;
    }

    set_expanded_state(index);
    separate(true);
    start_growing(index, false);
}

bool pie_chart::animations_running() const
{
    return separation_active || growing_active;
}

void pie_chart::set_section_hover(int index, bool hovered)
{
    if (index < 0 || index >= (int)sections.size()) {
        return;
    }

    /* Solo una seccion puede estar resaltada: al activar una, las demas se
     * desactivan desde su estado actual. */
    if (hovered) {
        for (int i = 0; i < (int)sections.size(); ++i) {
            if (i != index) {
                set_section_hover(i, false);
            }
        }
    }

    section& current = sections[(size_t)index];
    const double target = hovered ? 1.0 : 0.0;

    /* Si ya va o esta en el destino no se reinicia la transicion. */
    if (current.hover_active ? current.hover_to == target
                             : current.hover_state == target) {
        return;
    }

    current.hover_from     = current.hover_state;
    current.hover_to       = target;
    current.hover_start_ms = SDL_GetTicks();
    current.hover_active   = true;

    if (hovered) {
        hovered_section = index;
    } else if (hovered_section == index) {
        hovered_section = -1;
    }
}

void pie_chart::update_hover(float x, float y)
{
    const int index = section_at(x, y);
    if (index == hovered_section) {
        return;
    }

    hovered_section = index;
    for (int i = 0; i < (int)sections.size(); ++i) {
        set_section_hover(i, i == index);
    }
}

void pie_chart::clear_hover()
{
    for (int i = 0; i < (int)sections.size(); ++i) {
        set_section_hover(i, false);
    }
    hovered_section = -1;
}

int pie_chart::section_at(float x, float y) const
{
    /* Se usa el centro propio de cada porcion, que es el origen real del
     * abanico dibujado, y su intervalo angular actual. */
    const float reach = radius + feather + theme::hover_border_width * 0.5f;

    for (size_t i = 0; i < sections.size(); ++i) {
        const section& current = sections[i];
        const float dx = x - current.center.x;
        const float dy = y - current.center.y;

        if (dx * dx + dy * dy > reach * reach) {
            continue;
        }

        float angle = std::atan2(dy, dx);
        if (angle < 0.0f) {
            angle += two_pi;
        }

        float start = std::fmod(current.angle_start, two_pi);
        if (start < 0.0f) {
            start += two_pi;
        }

        float delta = angle - start;
        if (delta < 0.0f) {
            delta += two_pi;
        }

        const float sweep = current.angle_end - current.angle_start;
        if (delta <= sweep) {
            return (int)i;
        }
    }

    return -1;
}

void pie_chart::update_hover_animation()
{
    constexpr double duration_ms = theme::animation_hover_ms;
    const Uint64 now = SDL_GetTicks();

    for (section& current : sections) {
        if (!current.hover_active) {
            continue;
        }

        const double elapsed_ms = (double)(now - current.hover_start_ms);
        const double progress   = elapsed_ms / duration_ms;

        if (progress >= 1.0) {
            current.hover_state  = current.hover_to;
            current.hover_active = false;
        } else {
            current.hover_state = current.hover_from
                                + (current.hover_to - current.hover_from)
                                * progress;
        }
    }
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

double pie_chart::current_growth_amount() const
{
    /* growing_state es el progreso de la transicion, no el tamano visible.
     * El tamano visible es growing_state al crecer y 1 - growing_state al
     * volver a la proporcion fija. */
    return growing_direction ? growing_state : 1.0 - growing_state;
}

void pie_chart::start_growing(int index, bool to_full)
{
    if (sections.empty()) {
        return;
    }

    set_growing_sector(index);
    const double amount = std::clamp(current_growth_amount(), 0.0, 1.0);

    /* Ya esta en el destino: no hace falta otra transicion. */
    if ((to_full && amount >= 1.0) || (!to_full && amount <= 0.0)) {
        return;
    }

    /* Se arranca desde el tamano visible actual para que no haya saltos. */
    growing_direction       = to_full;
    growing_progress_offset = to_full ? amount : 1.0 - amount;
    growing_state           = growing_progress_offset;
    growing_start_ms        = SDL_GetTicks();
    growing_active          = true;
}

void pie_chart::set_expanded_state(int index)
{
    if (sections.empty()) {
        return;
    }

    index = std::clamp(index, 0, (int)sections.size() - 1);
    clear_hover();

    separation_state  = 0.0;
    separation_from   = 0.0;
    separation_to     = 0.0;
    separation_active = false;

    growing_sector          = index;
    growing_direction       = true;
    growing_state           = 1.0;
    growing_progress_offset = 0.0;
    growing_active          = false;

    for (size_t i = 0; i < sections.size(); ++i) {
        section& current = sections[i];
        current.temporary_proportion = ((int)i == index) ? 1.0 : 0.0;

        /* Al entrar en un chart no debe quedar ningun resaltado de la visita
         * anterior, ni siquiera durante el primer fotograma. */
        current.hover_state  = 0.0;
        current.hover_from   = 0.0;
        current.hover_to     = 0.0;
        current.hover_active = false;
    }

    recalculate_geometry();
}

void pie_chart::recalculate_geometry()
{
    float angle = 0.0f;
    for (section& current : sections) {
        const float sweep = (float)current.temporary_proportion * two_pi;
        current.angle_start = angle;
        current.angle_end   = angle + sweep;
        current.calculate_vertices(*this, separation_state);
        angle += sweep;
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

    /* Los contornos van despues de todos los rellenos: asi el borde de la
     * seccion resaltada no queda tapado por una seccion vecina. */
    for (const section& current : sections) {
        if (current.border_vertices.empty()) {
            continue;
        }
        SDL_RenderGeometry(renderer, nullptr, current.border_vertices.data(),
                           (int)current.border_vertices.size(),
                           current.border_indices.data(),
                           (int)current.border_indices.size());
    }
}
