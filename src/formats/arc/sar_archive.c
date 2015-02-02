#include <stdlib.h>
#include "assert_ex.h"
#include "formats/arc/sar_archive.h"
#include "logger.h"

typedef struct
{
    char *name;
    uint32_t offset;
    uint32_t size;
} TableEntry;

typedef struct
{
    IO *arc_io;
    TableEntry *table_entry;
} UnpackContext;

static VirtualFile *read_file(void *_context)
{
    VirtualFile *file = virtual_file_create();
    UnpackContext *context = (UnpackContext*)_context;
    io_seek(context->arc_io, context->table_entry->offset);

    io_write_string_from_io(
        file->io,
        context->arc_io,
        context->table_entry->size);

    virtual_file_set_name(file, context->table_entry->name);
    return file;
}

static bool unpack(Archive *archive, IO *arc_io, OutputFiles *output_files)
{
    TableEntry **table;
    size_t i, j;

    assert_not_null(archive);
    assert_not_null(arc_io);
    assert_not_null(output_files);

    uint16_t file_count = io_read_u16_be(arc_io);
    uint32_t offset_to_files = io_read_u32_be(arc_io);
    if (offset_to_files > io_size(arc_io))
    {
        log_error("Bad offset to files");
        return false;
    }

    table = (TableEntry**)malloc(file_count * sizeof(TableEntry*));
    assert_not_null(table);
    for (i = 0; i < file_count; i ++)
    {
        TableEntry *entry = (TableEntry*)malloc(sizeof(TableEntry));
        assert_not_null(entry);
        entry->name = NULL;
        io_read_until_zero(arc_io, &entry->name, NULL);
        entry->offset = io_read_u32_be(arc_io) + offset_to_files;
        entry->size = io_read_u32_be(arc_io);
        if (entry->offset + entry->size > io_size(arc_io))
        {
            log_error("Bad offset to file");
            for (j = 0; j < i; j ++)
                free(table[j]);
            free(table);
            return false;
        }
        table[i] = entry;
    }

    UnpackContext context;
    context.arc_io = arc_io;
    for (i = 0; i < file_count; i ++)
    {
        context.table_entry = table[i];
        output_files_save(output_files, &read_file, &context);
    }

    for (i = 0; i < file_count; i ++)
    {
        free(table[i]->name);
        free(table[i]);
    }
    free(table);

    return true;
}

Archive *sar_archive_create()
{
    Archive *archive = archive_create();
    archive->unpack = &unpack;
    return archive;
}
