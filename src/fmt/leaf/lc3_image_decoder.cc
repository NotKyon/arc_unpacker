#include "fmt/leaf/lc3_image_decoder.h"
#include "fmt/leaf/common/custom_lzss.h"
#include "err.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::leaf;

static const bstr magic = "LEAFC64\x00"_b;

static bstr get_data(io::IO &io, const size_t size_comp, const size_t size_orig)
{
    auto data = io.read(size_comp);
    for (auto &c : data)
        c ^= 0xFF;
    return common::custom_lzss_decompress(data, size_orig);
}

bool Lc3ImageDecoder::is_recognized_impl(File &file) const
{
    return file.io.read(magic.size()) == magic;
}

pix::Grid Lc3ImageDecoder::decode_impl(File &file) const
{
    file.io.seek(magic.size());
    file.io.skip(4);
    const auto width = file.io.read_u16_le();
    const auto height = file.io.read_u16_le();

    const auto alpha_pos = file.io.read_u32_le();
    const auto color_pos = file.io.read_u32_le();

    file.io.seek(color_pos);
    const auto color_data
        = get_data(file.io, file.io.size() - color_pos, width * height * 2);
    pix::Grid image(width, height, color_data, pix::Format::BGR555X);

    file.io.seek(alpha_pos);
    auto mask_data
        = get_data(file.io, color_pos - alpha_pos, width * height);
    for (auto &c : mask_data)
        c <<= 3;
    pix::Grid mask(width, height, mask_data, pix::Format::Gray8);

    image.apply_mask(mask);
    image.flip_vertically();
    return image;
}

static auto dummy = fmt::register_fmt<Lc3ImageDecoder>("leaf/lc3");
