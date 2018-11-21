#include <algorithm>

#include "../include/scout/utf8_canvas.h"

utf8_canvas::utf8_canvas(int const width)
    : width_(std::max(0, width)), height_(0) { }

int utf8_canvas::width() const
{
    return width_;
}
int utf8_canvas::height() const
{
    return height_;
}

void utf8_canvas::add_shape(int const layer, utf8_shape const* const shape)
{
    operator[](layer).push_back(shape);

    height_ = std::max(height_, shape->y_pos + shape->y_size);
}

utf8_illustration utf8_canvas::illustrate() const
{
    utf8_illustration illustration(height_);
    for (auto const& layer : *this)
    {
        for (auto const* shape : layer.second)
        {
            auto const shape_illustration = shape->illustrate();

            for (auto y = std::max(0, shape->y_pos); y < shape->y_pos + shape->y_size; ++y)
            {
                auto& composition_line = illustration.at(y);
                if (static_cast<int>(composition_line.size()) < shape->x_pos)
                    composition_line.resize(std::min(shape->x_pos, width_));

                for (auto x = std::max(0, shape->x_pos); x < std::min(shape->x_pos + shape->x_size, width_); ++x)
                {
                    auto const utf8_char = shape_illustration.at(y - shape->y_pos).at(x - shape->x_pos);

                    if (x < static_cast<int>(composition_line.size()))
                        composition_line.at(x) = utf8_char;
                    else
                        composition_line.push_back(utf8_char);
                }
            }
        }
    }

    return illustration;
}