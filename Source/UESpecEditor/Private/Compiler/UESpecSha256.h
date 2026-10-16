#pragma once

#include "CoreMinimal.h"

/**
 * Self-contained SHA-256 (FIPS 180-4) used for the UESpecSpecHash ownership tag.
 *
 * Core has no SHA-256 (Misc/SecureHash.h only ships SHA-1/MD5), and
 * FGenericPlatformMisc::GetSHA256Signature is a checkf(false) stub on Windows,
 * so the compiler carries its own spec-following implementation. Output is
 * lowercase hex, identical to Python's hashlib.sha256(...).hexdigest(), so the
 * MCP backend can compare a spec file against the tag without loading the asset.
 */
namespace UE::UESpec::Editor::Sha256
{
	namespace Detail
	{
		inline uint32 RotR(const uint32 X, const uint32 N) { return (X >> N) | (X << (32 - N)); }

		static constexpr uint32 RoundConstants[64] = {
			0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
			0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
			0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
			0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
			0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
			0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
			0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
			0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
		};
	}

	/** SHA-256 of Len bytes at Data as 64 lowercase hex characters. */
	inline FString HashBytesToHex(const uint8* Data, const int64 Len)
	{
		uint32 H[8] = {
			0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
			0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
		};

		TArray<uint8> Padded;
		Padded.Reserve(static_cast<int32>(Len) + 72);
		if (Data && Len > 0)
		{
			Padded.Append(Data, static_cast<int32>(Len));
		}
		Padded.Add(0x80);
		while ((Padded.Num() % 64) != 56)
		{
			Padded.Add(0);
		}
		const uint64 BitLen = static_cast<uint64>(Len) * 8ull;
		for (int32 Shift = 7; Shift >= 0; --Shift)
		{
			Padded.Add(static_cast<uint8>((BitLen >> (Shift * 8)) & 0xFF));
		}

		for (int32 Chunk = 0; Chunk < Padded.Num(); Chunk += 64)
		{
			uint32 W[64];
			for (int32 T = 0; T < 16; ++T)
			{
				const int32 Base = Chunk + T * 4;
				W[T] = (uint32(Padded[Base]) << 24) | (uint32(Padded[Base + 1]) << 16) | (uint32(Padded[Base + 2]) << 8) | uint32(Padded[Base + 3]);
			}
			for (int32 T = 16; T < 64; ++T)
			{
				const uint32 S0 = Detail::RotR(W[T - 15], 7) ^ Detail::RotR(W[T - 15], 18) ^ (W[T - 15] >> 3);
				const uint32 S1 = Detail::RotR(W[T - 2], 17) ^ Detail::RotR(W[T - 2], 19) ^ (W[T - 2] >> 10);
				W[T] = W[T - 16] + S0 + W[T - 7] + S1;
			}

			uint32 A = H[0], B = H[1], C = H[2], D = H[3], E = H[4], F = H[5], G = H[6], Hh = H[7];
			for (int32 T = 0; T < 64; ++T)
			{
				const uint32 S1 = Detail::RotR(E, 6) ^ Detail::RotR(E, 11) ^ Detail::RotR(E, 25);
				const uint32 Ch = (E & F) ^ (~E & G);
				const uint32 Temp1 = Hh + S1 + Ch + Detail::RoundConstants[T] + W[T];
				const uint32 S0 = Detail::RotR(A, 2) ^ Detail::RotR(A, 13) ^ Detail::RotR(A, 22);
				const uint32 Maj = (A & B) ^ (A & C) ^ (B & C);
				const uint32 Temp2 = S0 + Maj;
				Hh = G; G = F; F = E; E = D + Temp1;
				D = C; C = B; B = A; A = Temp1 + Temp2;
			}
			H[0] += A; H[1] += B; H[2] += C; H[3] += D;
			H[4] += E; H[5] += F; H[6] += G; H[7] += Hh;
		}

		static const TCHAR* Digits = TEXT("0123456789abcdef");
		FString Out;
		Out.Reserve(64);
		for (int32 Index = 0; Index < 8; ++Index)
		{
			for (int32 Shift = 28; Shift >= 0; Shift -= 4)
			{
				Out.AppendChar(Digits[(H[Index] >> Shift) & 0xF]);
			}
		}
		return Out;
	}

	/** SHA-256 of the UTF-8 encoding of Text. */
	inline FString HashUtf8ToHex(const FString& Text)
	{
		const FTCHARToUTF8 Utf8(*Text);
		return HashBytesToHex(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	}
}
