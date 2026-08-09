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
    };

    struct Reader
    {
        const std::vector<std::byte>& Bytes;
        std::size_t Position = 0;

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
    };
}
