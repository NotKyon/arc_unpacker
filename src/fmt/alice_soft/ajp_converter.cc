// AJP JPEG image wrapper
//
// Company:   Alice Soft
// Engine:    -
// Extension: .ajp
// Archives:  AFA
//
// Known games:
// - Daiakuji

#include <algorithm>
#include "fmt/alice_soft/ajp_converter.h"
#include "fmt/alice_soft/pm_converter.h"
#include "util/image.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::alice_soft;

static const bstr magic = "AJP\x00"_b;
static const bstr key =
    "\x5D\x91\xAE\x87\x4A\x56\x41\xCD\x83\xEC\x4C\x92\xB5\xCB\x16\x34"_b;

static void decrypt(bstr &input)
{
    for (auto i : util::range(std::min(input.size(), key.size())))
        input[i] ^= key[i];
}

bool AjpConverter::is_recognized_internal(File &file) const
{
    return file.io.read(magic.size()) == magic;
}

std::unique_ptr<File> AjpConverter::decode_internal(File &file) const
{
    file.io.skip(magic.size());
    file.io.skip(4 * 2);
    auto width = file.io.read_u32_le();
    auto height = file.io.read_u32_le();
    auto jpeg_offset = file.io.read_u32_le();
    auto jpeg_size = file.io.read_u32_le();
    auto mask_offset = file.io.read_u32_le();
    auto mask_size = file.io.read_u32_le();

    file.io.seek(jpeg_offset);
    auto jpeg_data = file.io.read(jpeg_size);
    decrypt(jpeg_data);

    file.io.seek(mask_offset);
    auto mask_data = file.io.read(mask_size);
    decrypt(mask_data);

    if (!mask_size)
    {
        std::unique_ptr<File> output_file(new File);
        output_file->name = file.name;
        output_file->io.write(jpeg_data);
        output_file->guess_extension();
        return output_file;
    }

    PmConverter pm_converter;
    auto mask_image = pm_converter.decode_to_image(mask_data);
    auto jpeg_image = util::Image::from_boxed(jpeg_data);

    std::vector<util::Color> pixels(width * height);
    auto *pixels_ptr = &pixels[0];
    for (auto y : util::range(height))
    {
        for (auto x : util::range(width))
        {
            auto color = jpeg_image->color_at(x, y);
            color.a = mask_image->color_at(x, y).r;
            *pixels_ptr++ = color;
        }
    }

    auto output_image = util::Image::from_pixels(width, height, pixels);
    return output_image->create_file(file.name);
}