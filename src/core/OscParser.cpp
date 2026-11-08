#include "core/OscParser.h"

#include <cstring>
#include <sstream>

namespace led {

namespace {

constexpr int kMaxBundleDepth = 8;

std::size_t pad4(std::size_t n)
{
    return (n + 3) & ~static_cast<std::size_t>(3);
}

bool readString(const std::uint8_t* data, std::size_t size, std::size_t& pos, std::string& out)
{
    const std::size_t start = pos;
    while (pos < size && data[pos] != 0)
        ++pos;
    if (pos >= size)
        return false;
    out.assign(reinterpret_cast<const char*>(data + start), pos - start);
    pos = start + pad4(pos - start + 1);
    return pos <= size;
}

std::uint32_t readU32(const std::uint8_t* p)
{
    return (std::uint32_t(p[0]) << 24) | (std::uint32_t(p[1]) << 16) | (std::uint32_t(p[2]) << 8) | std::uint32_t(p[3]);
}

std::uint64_t readU64(const std::uint8_t* p)
{
    return (std::uint64_t(readU32(p)) << 32) | readU32(p + 4);
}

std::string numberText(double v)
{
    std::ostringstream s;
    s << v;
    return s.str();
}

void parseElement(const std::uint8_t* data, std::size_t size, int depth, std::vector<OscMessage>& out)
{
    if (size < 4 || depth > kMaxBundleDepth)
        return;

    if (size >= 16 && std::memcmp(data, "#bundle", 8) == 0) {
        std::size_t pos = 16; // "#bundle\0" + 8바이트 시각
        while (pos + 4 <= size) {
            const std::uint32_t len = readU32(data + pos);
            pos += 4;
            if (len == 0 || len > size - pos)
                return;
            parseElement(data + pos, len, depth + 1, out);
            pos += len;
        }
        return;
    }

    std::size_t pos = 0;
    OscMessage msg;
    if (!readString(data, size, pos, msg.address) || msg.address.empty() || msg.address[0] != '/')
        return;
    std::string types;
    if (pos < size && data[pos] == ',') {
        if (!readString(data, size, pos, types))
            return;
        types = types.substr(1);
    }
    for (char t : types) {
        switch (t) {
        case 'i':
            if (pos + 4 > size)
                return;
            msg.args.push_back(std::to_string(static_cast<std::int32_t>(readU32(data + pos))));
            pos += 4;
            break;
        case 'f': {
            if (pos + 4 > size)
                return;
            const std::uint32_t bits = readU32(data + pos);
            float f;
            std::memcpy(&f, &bits, sizeof(f));
            msg.args.push_back(numberText(f));
            pos += 4;
            break;
        }
        case 'h':
            if (pos + 8 > size)
                return;
            msg.args.push_back(std::to_string(static_cast<std::int64_t>(readU64(data + pos))));
            pos += 8;
            break;
        case 'd': {
            if (pos + 8 > size)
                return;
            const std::uint64_t bits = readU64(data + pos);
            double d;
            std::memcpy(&d, &bits, sizeof(d));
            msg.args.push_back(numberText(d));
            pos += 8;
            break;
        }
        case 's':
        case 'S': {
            std::string s;
            if (!readString(data, size, pos, s))
                return;
            msg.args.push_back(s);
            break;
        }
        case 'T': msg.args.push_back("1"); break;
        case 'F': msg.args.push_back("0"); break;
        case 'N': break;
        default: // 지원하지 않는 형식: 이후 인수는 해석 불가
            out.push_back(std::move(msg));
            return;
        }
    }
    out.push_back(std::move(msg));
}

} // namespace

std::vector<OscMessage> parseOscPacket(const std::uint8_t* data, std::size_t size)
{
    std::vector<OscMessage> out;
    if (data && size > 0)
        parseElement(data, size, 0, out);
    return out;
}

std::string oscToCommand(const OscMessage& message)
{
    std::string cmd;
    // "/a/b" -> "a b"
    for (char c : message.address.substr(1))
        cmd += c == '/' ? ' ' : c;
    for (const std::string& arg : message.args) {
        cmd += " \"";
        for (char c : arg)
            if (c != '"')
                cmd += c;
        cmd += '"';
    }
    return cmd;
}

} // namespace led
