#pragma once

namespace fast_io::intrinsics::msvc::arm
{

extern "C"
{
	unsigned __int64 __getReg(int);

	unsigned int _MoveFromCoprocessor(unsigned int, unsigned int, unsigned int, unsigned int, unsigned int);

#if defined(__ARM_NEON)
	__n64 neon_dupr8(__int32);
	__n64 neon_dupr16(__int32);
	__n64 neon_dupr32(__int32);
	__n64 neon_duprf32(float);
	__n64 neon_dupr64(__int64);
	__n64 neon_duprf64(double);
	__n128 neon_dupqr8(__int32);
	__n128 neon_dupqr16(__int32);
	__n128 neon_dupqr32(__int32);
	__n128 neon_dupqrf32(float);
	__n128 neon_dupqr64(__int64);
	__n128 neon_dupqrf64(double);

	__n8 neon_dups8(__n64, __int32 const);
	__n16 neon_dups16(__n64, __int32 const);
	float neon_dups32(__n64, __int32 const);
	__n64 neon_dups64(__n64, __int32 const);
	__n8 neon_dups8q(__n128, __int32 const);
	__n16 neon_dups16q(__n128, __int32 const);
	float neon_dups32q(__n128, __int32 const);
	__n64 neon_dups64q(__n128, __int32 const);
#endif
}

} // namespace fast_io::intrinsics::msvc::arm
