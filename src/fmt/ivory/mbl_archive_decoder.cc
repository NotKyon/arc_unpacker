#include "fmt/ivory/mbl_archive_decoder.h"
#include "fmt/ivory/prs_image_decoder.h"
#include "fmt/ivory/wady_audio_decoder.h"
#include "log.h"
#include "util/encoding.h"
#include "util/format.h"
#include "util/plugin_mgr.hh"
#include "util/range.h"

using namespace au;
using namespace au::fmt::ivory;

namespace
{
    enum Version
    {
        Unknown,
        Version1,
        Version2,
    };

    using PluginFunc = std::function<void(bstr &)>;

    struct ArchiveEntryImpl final : fmt::ArchiveEntry
    {
        size_t offset;
        size_t size;
    };

    struct ArchiveMetaImpl final : fmt::ArchiveMeta
    {
        bool encrypted;
        PluginFunc decrypt;
    };
}

static int check_version(io::IO &arc_io, size_t file_count, size_t name_size)
{
    arc_io.skip((file_count - 1) * (name_size + 8));
    arc_io.skip(name_size);
    auto last_file_offset = arc_io.read_u32_le();
    auto last_file_size = arc_io.read_u32_le();
    return last_file_offset + last_file_size == arc_io.size();
}

static Version get_version(io::IO &arc_io)
{
    auto file_count = arc_io.read_u32_le();
    if (check_version(arc_io, file_count, 16))
        return Version::Version1;

    arc_io.seek(4);
    auto name_size = arc_io.read_u32_le();
    if (check_version(arc_io, file_count, name_size))
        return Version::Version2;

    return Version::Unknown;
}

struct MblArchiveDecoder::Priv final
{
    util::PluginManager<PluginFunc> plugin_mgr;
    PrsImageDecoder prs_image_decoder;
    WadyAudioDecoder wady_audio_decoder;
    PluginFunc plugin;
};

MblArchiveDecoder::MblArchiveDecoder() : p(new Priv)
{
    add_decoder(&p->prs_image_decoder);
    add_decoder(&p->wady_audio_decoder);

    p->plugin_mgr.add("noop", "Unencrypted games", [](bstr &) { });

    p->plugin_mgr.add("candy", "Candy Toys",
        [](bstr &data)
        {
            for (auto i : util::range(data.size()))
                data[i] = -data[i];
        });

    p->plugin_mgr.add("wanko", "Wanko to Kurasou",
        [](bstr &data)
        {
            static const bstr key =
                "\x82\xED\x82\xF1\x82\xB1\x88\xC3\x8D\x86\x89\xBB"_b;
            for (auto i : util::range(data.size()))
                data[i] ^= key[i % key.size()];
        });
}

MblArchiveDecoder::~MblArchiveDecoder()
{
}

void MblArchiveDecoder::register_cli_options(ArgParser &arg_parser) const
{
    p->plugin_mgr.register_cli_options(
        arg_parser, "Specifies plugin for decoding dialog files.");
    ArchiveDecoder::register_cli_options(arg_parser);
}

void MblArchiveDecoder::parse_cli_options(const ArgParser &arg_parser)
{
    p->plugin = p->plugin_mgr.get_from_cli_options(arg_parser, false);
    ArchiveDecoder::parse_cli_options(arg_parser);
}

void MblArchiveDecoder::set_plugin(const std::string &plugin_name)
{
    p->plugin = p->plugin_mgr.get_from_string(plugin_name);
}

bool MblArchiveDecoder::is_recognized_impl(File &arc_file) const
{
    return get_version(arc_file.io) != Version::Unknown;
}

std::unique_ptr<fmt::ArchiveMeta>
    MblArchiveDecoder::read_meta_impl(File &arc_file) const
{
    auto meta = std::make_unique<ArchiveMetaImpl>();
    auto version = get_version(arc_file.io);
    meta->encrypted = arc_file.name.find("mg_data") != std::string::npos;
    meta->decrypt = p->plugin;
    arc_file.io.seek(0);

    auto file_count = arc_file.io.read_u32_le();
    auto name_size = version == Version::Version2
        ? arc_file.io.read_u32_le()
        : 16;
    for (auto i : util::range(file_count))
    {
        auto entry = std::make_unique<ArchiveEntryImpl>();
        entry->name = util::sjis_to_utf8(
            arc_file.io.read_to_zero(name_size)).str();
        entry->offset = arc_file.io.read_u32_le();
        entry->size = arc_file.io.read_u32_le();
        meta->entries.push_back(std::move(entry));
    }
    return meta;
}

std::unique_ptr<File> MblArchiveDecoder::read_file_impl(
    File &arc_file, const ArchiveMeta &m, const ArchiveEntry &e) const
{
    auto meta = static_cast<const ArchiveMetaImpl*>(&m);
    auto entry = static_cast<const ArchiveEntryImpl*>(&e);

    arc_file.io.seek(entry->offset);
    auto data = arc_file.io.read(entry->size);
    if (meta->encrypted)
    {
        if (!meta->decrypt)
            throw err::UsageError("File is decrypted, but plugin not set.");
        meta->decrypt(data);
    }

    auto output_file = std::make_unique<File>(entry->name, data);
    output_file->guess_extension();
    return output_file;
}

static auto dummy = fmt::Registry::add<MblArchiveDecoder>("ivory/mbl");
