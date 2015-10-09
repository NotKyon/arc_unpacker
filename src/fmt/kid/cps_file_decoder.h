#pragma once

#include "fmt/file_decoder.h"

namespace au {
namespace fmt {
namespace kid {

    class CpsFileDecoder final : public FileDecoder
    {
    protected:
        bool is_recognized_impl(File &) const override;
        std::unique_ptr<File> decode_impl(File &) const override;
    };

} } }
