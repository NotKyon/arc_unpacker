#ifndef AU_FMT_TOUHOU_PAK2_ARCHIVE_H
#define AU_FMT_TOUHOU_PAK2_ARCHIVE_H
#include "fmt/archive.h"

namespace au {
namespace fmt {
namespace touhou {

    class Pak2Archive final : public Archive
    {
    public:
        Pak2Archive();
        ~Pak2Archive();
    protected:
        bool is_recognized_internal(File &) const override;
        void unpack_internal(File &, FileSaver &) const override;
    private:
        struct Priv;
        std::unique_ptr<Priv> p;
    };

} } }

#endif