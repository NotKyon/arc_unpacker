#pragma once

#include "fmt/archive_decoder.h"

namespace au {
namespace fmt {
namespace minato_soft {

    class PacArchiveDecoder final : public ArchiveDecoder
    {
    public:
        PacArchiveDecoder();
        ~PacArchiveDecoder();
    protected:
        bool is_recognized_impl(File &) const override;
        std::unique_ptr<ArchiveMeta> read_meta_impl(File &) const override;
        std::unique_ptr<File> read_file_impl(
            File &, const ArchiveMeta &, const ArchiveEntry &) const override;
    private:
        struct Priv;
        std::unique_ptr<Priv> p;
    };

} } }
