// Copyright 2012 Luis Pedro Coelho <luis@luispedro.org>
// License: MIT (see COPYING.MIT file)

#include "base.h"
#include "_png.h"

#include <png.h>

#include <cstring>
#include <vector>

class png_holder {
    public:
        png_holder(int m)
            :png_ptr((m == write_mode ? png_create_write_struct : png_create_read_struct)(PNG_LIBPNG_VER_STRING, 0, 0, 0))
            ,png_info(0)
            { }
        ~png_holder() {
            png_infopp pp = (png_info ? &png_info : 0);
            if (mode == read_mode) png_destroy_read_struct(&png_ptr, pp, 0);
            else png_destroy_write_struct(&png_ptr, pp);
        }
        void create_info() {
            png_info = png_create_info_struct(png_ptr);
            if (!png_info) throw "error";
        }

        png_structp png_ptr;
        png_infop png_info;
        enum { read_mode, write_mode } mode;
};

namespace {
void read_from_source(png_structp png_ptr, png_byte* buffer, size_t n) {
    byte_source* s = static_cast<byte_source*>(png_get_io_ptr(png_ptr));
    const size_t actual = s->read(reinterpret_cast<byte*>(buffer), n);
    if (actual != n) {
        throw CannotReadError();
    }
}

void write_to_source(png_structp png_ptr, png_byte* buffer, size_t n) {
    byte_sink* s = static_cast<byte_sink*>(png_get_io_ptr(png_ptr));
    const size_t actual = s->write(reinterpret_cast<byte*>(buffer), n);
    if (actual != n) {
        throw CannotReadError();
    }
}
void flush_source(png_structp png_ptr) {
    byte_sink* s = static_cast<byte_sink*>(png_get_io_ptr(png_ptr));
    s->flush();
}

int color_type_of(Image* im) {
    if (im->ndims() == 2) return PNG_COLOR_TYPE_GRAY;
    if (im->ndims() != 3) throw CannotWriteError();
    if (im->dim(2) == 3) return PNG_COLOR_TYPE_RGB;
    if (im->dim(2) == 4) return PNG_COLOR_TYPE_RGBA;
    throw CannotWriteError();
}
}
void PNGFormat::read(byte_source* src, Image* output) {
    png_holder p(png_holder::read_mode);
    if (setjmp(png_jmpbuf(p.png_ptr))) {
        throw CannotReadError();
    }
    png_set_read_fn(p.png_ptr, src, read_from_source);
    p.create_info();
    png_read_info(p.png_ptr, p.png_info);
    
    const int w = png_get_image_width (p.png_ptr, p.png_info);
    const int h = png_get_image_height(p.png_ptr, p.png_info);
    int d = -1;
    switch (png_get_color_type(p.png_ptr, p.png_info)) {
        case PNG_COLOR_TYPE_RGB:
            d = 3;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            d = 4;
            break;
        case PNG_COLOR_TYPE_GRAY:
            d = -1;
            break;
        default:
            throw CannotReadError("Unhandled color type");
    }
    if (png_get_bit_depth(p.png_ptr, p.png_info) != 8) {
        throw CannotReadError("Can currently only read 8-bit images");
    }

    output->set_size(h, w, d);
    std::vector<png_bytep> rowps;
    for (int r = 0; r != h; ++r) {
        png_byte* rowp = output->rowp_as<png_byte>(r);
        rowps.push_back(rowp);
    }
    png_read_image(p.png_ptr, &rowps[0]);
}

void PNGFormat::write(Image* input, byte_sink* output) {
    png_holder p(png_holder::write_mode);
    p.create_info();
    png_set_write_fn(p.png_ptr, output, write_to_source, flush_source);
    const int height = input->dim(0);
    const int width = input->dim(1);
    const int bit_depth = 8;
    const int color_type = color_type_of(input);
    if (setjmp(png_jmpbuf(p.png_ptr))) {
        throw CannotWriteError();
    }

    png_set_IHDR(p.png_ptr, p.png_info, width, height,
                     bit_depth, color_type, PNG_INTERLACE_NONE,
                     PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(p.png_ptr, p.png_info);

    std::vector<png_bytep> rowps;
    for (int r = 0; r != height; ++r) {
        rowps.push_back(input->rowp_as<png_byte>(r));
    }
    png_write_image(p.png_ptr, &rowps[0]);
    png_write_end(p.png_ptr, p.png_info);
}


