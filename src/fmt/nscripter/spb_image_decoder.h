#pragma once

#include "fmt/image_decoder.h"

namespace au {
namespace fmt {
namespace nscripter {

    class SpbImageDecoder final : public ImageDecoder
    {
    protected:
        bool is_recognized_impl(File &) const override;
        pix::Grid decode_impl(File &) const override;
    };

} } }
