#include "util/encoding.h"
#include <cerrno>
#include <memory>
#include <iconv.h>
#include "err.h"

using namespace au;

bstr util::convert_encoding(
    const bstr &input, const std::string &from, const std::string &to)
{
    iconv_t conv = iconv_open(to.c_str(), from.c_str());
    if (!conv)
        throw std::logic_error("Failed to initialize iconv");

    bstr output;
    output.reserve(input.size() * 2);

    char *input_ptr = const_cast<char*>(input.get<const char>());
    size_t input_bytes_left = input.size();
    bstr buffer(32);

    while (true)
    {
        char *output_buffer = buffer.get<char>();
        size_t output_bytes_left = buffer.size();
        int ret = iconv(
            conv,
            &input_ptr,
            &input_bytes_left,
            &output_buffer,
            &output_bytes_left);

        output += buffer.substr(0, buffer.size() - output_bytes_left);

        if (ret != -1 && input_bytes_left == 0)
            break;

        switch (errno)
        {
            case EINVAL:
            case EILSEQ:
                throw err::CorruptDataError("Invalid byte sequence");

            case E2BIG:
                // repeat the iteration unless we got nothing at all
                if (output_bytes_left != buffer.size())
                    continue;
                throw err::CorruptDataError(
                    "Code point too large to decode (?)");
        }
    }

    return output;
}

bstr util::sjis_to_utf8(const bstr &input)
{
    return convert_encoding(input, "cp932", "utf-8");
}

bstr util::utf8_to_sjis(const bstr &input)
{
    return convert_encoding(input, "utf-8", "cp932");
}
