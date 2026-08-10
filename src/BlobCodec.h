//=============================================================================//
//                                                                             //
//   The Living Commonwealth — Fallout 4 adapter for the Living Commonwealth   //
//   Engine (LCE).                                                             //
//                                                                             //
//   QUOTE: <the author's line goes here>                                      //
//                                                                             //
//=============================================================================//

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace TLC::Codec
{
    //-------------------------------------------------------------------------
    // Tiny little-endian byte pack/unpack helpers. The core never
    // interprets component blobs — only the adapter's serializers give the
    // bytes meaning — so this codec is the adapter's own, kept deliberately
    // small (Law 001: simple things).
    //-------------------------------------------------------------------------
    struct Writer
    {
        std::vector<std::byte> Bytes;

        void U8(std::uint8_t value)
        {
            Bytes.push_back(static_cast<std::byte>(value));
        }

        void U32(std::uint32_t value)
        {
            for (int i = 0; i < 4; ++i)
            {
                Bytes.push_back(
                    static_cast<std::byte>((value >> (8 * i)) & 0xFFu));
            }
        }

        void U64(std::uint64_t value)
        {
            for (int i = 0; i < 8; ++i)
            {
                Bytes.push_back(
                    static_cast<std::byte>((value >> (8 * i)) & 0xFFull));
            }
        }

        void F(float value)
        {
            std::uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            U32(bits);
        }

        // Appends raw bytes — the co-save record's component blobs and
        // stable type names. (Named Raw: Bytes is already the data member.)
        void Raw(const void* a_data, std::size_t a_count)
        {
            const auto* first = static_cast<const std::byte*>(a_data);
            Bytes.insert(Bytes.end(), first, first + a_count);
        }
    };

    struct Reader
    {
        const std::vector<std::byte>& Bytes;
        std::size_t Position = 0;

        // How many bytes are left — the co-save decode checks this before
        // every read so a truncated record is refused, never half-read.
        [[nodiscard]] std::size_t Remaining() const noexcept
        {
            return Bytes.size() - Position;
        }

        std::uint8_t U8()
        {
            return std::to_integer<std::uint8_t>(Bytes[Position++]);
        }

        std::uint32_t U32()
        {
            std::uint32_t value = 0;

            for (int i = 0; i < 4; ++i)
            {
                value |= std::to_integer<std::uint32_t>(Bytes[Position++])
                    << (8 * i);
            }

            return value;
        }

        std::uint64_t U64()
        {
            std::uint64_t value = 0;

            for (int i = 0; i < 8; ++i)
            {
                value |= std::to_integer<std::uint64_t>(Bytes[Position++])
                    << (8 * i);
            }

            return value;
        }

        float F()
        {
            const std::uint32_t bits = U32();
            float value = 0.0f;
            std::memcpy(&value, &bits, sizeof(value));
            return value;
        }

        // Reads a_count raw bytes. The caller checks Remaining() first.
        // (Named Raw: Bytes is already the data member.)
        std::vector<std::byte> Raw(std::size_t a_count)
        {
            const auto first = Bytes.begin() + Position;
            Position += a_count;
            return { first, first + a_count };
        }
    };
}
