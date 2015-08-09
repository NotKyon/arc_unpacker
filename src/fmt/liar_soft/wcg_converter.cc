// WCG image
//
// Company:   Liar-soft
// Engine:    -
// Extension: .wcg
// Archives:  XFL
//
// Known games:
// - Souten No Celenaria - What a Beautiful World
// - Sekien no Inganock - What a Beautiful People
// - Shikkoku no Sharnoth - What a Beautiful Tomorrow

#include <stdexcept>
#include "fmt/liar_soft/wcg_converter.h"
#include "io/bit_reader.h"
#include "io/buffered_io.h"
#include "util/image.h"
#include "util/format.h"
#include "util/range.h"

using namespace au;
using namespace au::fmt::liar_soft;

static const bstr magic = "WG"_b;

static size_t wcg_unpack(
    io::IO &io,
    u8 *output,
    unsigned int output_size,
    int input_shift,
    int output_shift)
{
    u8 *output_ptr = output;
    u8 *output_guardian = output + output_size * output_shift;
    size_t expected_size = output_size << input_shift;
    size_t actual_size = io.read_u32_le();
    if (expected_size != actual_size)
    {
        throw std::runtime_error(util::format(
            "Unexpected size: %d != %d", actual_size, expected_size));
    }

    u32 base_offset = io.read_u32_le();
    u32 table_entry_count = io.read_u16_le();
    io.skip(2);

    u32 table_size = table_entry_count << input_shift;
    auto table = io.read(table_size);

    base_offset += io.tell();

    int tmp = table_entry_count - 1;
    if (tmp < 0)
        throw std::runtime_error("No table entries found");

    //risky
    tmp = tmp < 0x1001 ? -1 : 0;
    size_t var1 = tmp * 8 + 0xE;
    size_t var2 = tmp + 4;

    io::BitReader bit_reader(io);
    while (output_ptr != output_guardian)
    {
        size_t sequence_length = 1;
        size_t table_offset_length = bit_reader.get(var2);

        if (!table_offset_length)
        {
            sequence_length = bit_reader.get(4) + 2;
            table_offset_length = bit_reader.get(var2);
        }
        if (!table_offset_length)
            throw std::runtime_error("Table offset length = 0");

        u32 table_offset = 0;
        --table_offset_length;
        if (!table_offset_length)
        {
            table_offset = (table_offset << 1) + bit_reader.get(1);
        }
        else
        {
            if (table_offset_length >= var1)
            {
                while (bit_reader.get(1))
                    ++table_offset_length;
            }
            ++table_offset;
            table_offset <<= table_offset_length;
            table_offset |= bit_reader.get(table_offset_length);
        }

        if (table_offset >= table_entry_count)
            throw std::runtime_error("Bad table offset");

        if (input_shift == 1)
        {
            auto table16 = table.get<u16>();
            auto fragment = table16[table_offset];
            while (sequence_length--)
            {
                *reinterpret_cast<u16*>(output_ptr) = fragment;
                output_ptr += output_shift;
            }
        }
        else
        {
            auto table8 = table.get<u8>();
            auto fragment = table8[table_offset];
            while (sequence_length--)
            {
                *reinterpret_cast<u8*>(output_ptr) = fragment;
                output_ptr += output_shift;
            }
        }
    }

    return base_offset;
}

bool WcgConverter::is_recognized_internal(File &file) const
{
    if (file.io.read(magic.size()) != magic)
        return false;

    int version = file.io.read_u16_le();
    if (((version & 0xF) != 1) || ((version & 0x1C0) != 64))
        return false;

    return true;
}

std::unique_ptr<File> WcgConverter::decode_internal(File &file) const
{
    file.io.skip(magic.size());

    file.io.skip(2);
    if (file.io.read_u16_le() != 0x20)
        throw std::runtime_error("Unknown WCG version");
    file.io.skip(2);

    size_t width = file.io.read_u32_le();
    size_t height = file.io.read_u32_le();

    bstr pixels;
    pixels.resize(width * height * 4);

    io::BufferedIO buffered_io(file.io);
    auto ret = wcg_unpack(
        buffered_io, pixels.get<u8>() + 2, width * height, 1, 4);

    buffered_io.seek(ret);
    wcg_unpack(
        buffered_io, pixels.get<u8>(), width * height, 1, 4);

    for (auto i : util::range(0, pixels.size(), 4))
        pixels[i + 3] ^= 0xFF;

    std::unique_ptr<util::Image> image = util::Image::from_pixels(
        width, height, pixels, util::PixelFormat::BGRA);
    return image->create_file(file.name);
}
