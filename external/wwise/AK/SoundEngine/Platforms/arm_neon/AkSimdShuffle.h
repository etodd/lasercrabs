//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file 
/// _AKSIMD_LOCAL::SHUFFLE_V4F32<zyxw>(a, b) - arm_neon implementation

#ifndef __AK_SIMD_SHUFFLE_H__
#define __AK_SIMD_SHUFFLE_H__

namespace _AKSIMD_LOCAL
{
	// Same as _mm_shuffle_ps( a, b, _MM_SHUFFLE( z, y, x, w ) ). There is no default implementation
	// of this template function. Every required combination of zyxw needs to be explicitely implemented
	// below.
	template< int zyxw > AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32( const AKSIMD_V4F32& a, const AKSIMD_V4F32& b );

	/* 0 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xxaa( xyzw, abcd );
					AKSIMD_V2F32 xx = vdup_lane_f32( vget_low_f32( xyzw ), 0 );
					AKSIMD_V2F32 aa = vdup_lane_f32( vget_low_f32( abcd ), 0 );
					AKSIMD_V4F32 xxaa = vcombine_f32( xx, aa );
					return xxaa;
				}
	/* 1 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxaa( xyzw, abcd ); }
	/* 2 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxaa( xyzw, abcd ); }
	/* 3 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxaa( xyzw, abcd ); }
	/* 4 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyaa( xyzw, abcd ); }
	/* 5 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyaa( xyzw, abcd ); }
	/* 6 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyaa( xyzw, abcd ); }
	/* 7 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyaa( xyzw, abcd ); }
	/* 8 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzaa( xyzw, abcd ); }
	/* 9 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzaa( xyzw, abcd ); }
	/* 10 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzaa( xyzw, abcd ); }
	/* 11 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzaa( xyzw, abcd ); }
	/* 12 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwaa( xyzw, abcd ); }
	/* 13 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywaa( xyzw, abcd ); }
	/* 14 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwaa( xyzw, abcd ); }
	/* 15 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 0, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_wwaa( xyzw, abcd );
					AKSIMD_V2F32 ww = vdup_lane_f32( vget_high_f32( xyzw ), 1 );
					AKSIMD_V2F32 aa = vdup_lane_f32( vget_low_f32( abcd ), 0 );
					AKSIMD_V4F32 wwaa = vcombine_f32( ww, aa );
					return wwaa;

				}
	/* 16 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxba( xyzw, abcd ); }
	/* 17 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yxba( xyzw, abcd );
					AKSIMD_V2F32 yx = vrev64_f32( vget_low_f32( xyzw ) );
					AKSIMD_V2F32 ba = vrev64_f32( vget_low_f32( abcd ) );
					AKSIMD_V4F32 yxba = vcombine_f32( yx , ba );
					return yxba;
				}
	/* 18 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxba( xyzw, abcd ); }
	/* 19 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxba( xyzw, abcd ); }
	/* 20 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyba( xyzw, abcd ); }
	/* 21 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyba( xyzw, abcd ); }
	/* 22 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyba( xyzw, abcd ); }
	/* 23 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyba( xyzw, abcd ); }
	/* 24 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzba( xyzw, abcd ); }
	/* 25 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzba( xyzw, abcd ); }
	/* 26 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzba( xyzw, abcd ); }
	/* 27 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzba( xyzw, abcd ); }
	/* 28 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwba( xyzw, abcd ); }
	/* 29 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywba( xyzw, abcd ); }
	/* 30 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_zwba( xyzw, abcd );
					AKSIMD_V2F32 zw = vget_high_f32( xyzw );
					AKSIMD_V2F32 ba = vrev64_f32( vget_low_f32( abcd ) );
					AKSIMD_V4F32 zwba = vcombine_f32( zw , ba );
					return zwba;
				}
	/* 31 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 1, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwba( xyzw, abcd ); }
	/* 32 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxca( xyzw, abcd ); }
	/* 33 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxca( xyzw, abcd ); }
	/* 34 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxca( xyzw, abcd ); }
	/* 35 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxca( xyzw, abcd ); }
	/* 36 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyca( xyzw, abcd ); }
	/* 37 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyca( xyzw, abcd ); }
	/* 38 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyca( xyzw, abcd ); }
	/* 39 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyca( xyzw, abcd ); }
	/* 40 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzca( xyzw, abcd ); }
	/* 41 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzca( xyzw, abcd ); }
	/* 42 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzca( xyzw, abcd ); }
	/* 43 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzca( xyzw, abcd ); }
	/* 44 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwca( xyzw, abcd ); }
	/* 45 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywca( xyzw, abcd ); }
	/* 46 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwca( xyzw, abcd ); }
	/* 47 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 2, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwca( xyzw, abcd ); }
	/* 48 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxda( xyzw, abcd ); }
	/* 49 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxda( xyzw, abcd ); }
	/* 50 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxda( xyzw, abcd ); }
	/* 51 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxda( xyzw, abcd ); }
	/* 52 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyda( xyzw, abcd ); }
	/* 53 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyda( xyzw, abcd ); }
	/* 54 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_zyda( xyzw, abcd );
					AKSIMD_V4F32 bcda = vextq_f32( abcd, abcd, 1 );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V4F32 zyda = vcombine_f32( vrev64_f32( vget_low_f32( yzwx ) ), vget_high_f32( bcda ) );
					return zyda;
				}
	/* 55 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyda( xyzw, abcd ); }
	/* 56 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzda( xyzw, abcd ); }
	/* 57 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzda( xyzw, abcd ); }
	/* 58 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzda( xyzw, abcd ); }
	/* 59 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzda( xyzw, abcd ); }
	/* 60 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwda( xyzw, abcd ); }
	/* 61 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywda( xyzw, abcd ); }
	/* 62 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwda( xyzw, abcd ); }
	/* 63 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(0, 3, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwda( xyzw, abcd ); }
	/* 64 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxab( xyzw, abcd ); }
	/* 65 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxab( xyzw, abcd ); }
	/* 66 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxab( xyzw, abcd ); }
	/* 67 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxab( xyzw, abcd ); }
	/* 68 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xyab( xyzw, abcd );
					AKSIMD_V4F32 xyab = vcombine_f32( vget_low_f32( xyzw ) , vget_low_f32( abcd ) );
					return xyab;
				}
	/* 69 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyab( xyzw, abcd ); }
	/* 70 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyab( xyzw, abcd ); }
	/* 71 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyab( xyzw, abcd ); }
	/* 72 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzab( xyzw, abcd ); }
	/* 73 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yzab( xyzw, abcd );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V2F32 yz = vget_low_f32( yzwx );
					AKSIMD_V2F32 ab = vget_low_f32( abcd );
					AKSIMD_V4F32 yzab = vcombine_f32( yz, ab );
					return yzab;
				}
	/* 74 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzab( xyzw, abcd ); }
	/* 75 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzab( xyzw, abcd ); }
	/* 76 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwab( xyzw, abcd ); }
	/* 77 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywab( xyzw, abcd ); }
	/* 78 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_zwab( xyzw, abcd );
					AKSIMD_V2F32 zw = vget_high_f32( xyzw );
					AKSIMD_V2F32 ab = vget_low_f32( abcd );
					AKSIMD_V4F32 zwab = vcombine_f32( zw, ab );
					return zwab;
				}
	/* 79 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 0, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwab( xyzw, abcd ); }
	/* 80 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxbb( xyzw, abcd ); }
	/* 81 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxbb( xyzw, abcd ); }
	/* 82 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxbb( xyzw, abcd ); }
	/* 83 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxbb( xyzw, abcd ); }
	/* 84 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xybb( xyzw, abcd ); }
	/* 85 */ 	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yybb( xyzw, abcd );
					AKSIMD_V2F32 yy = vdup_lane_f32( vget_low_f32( xyzw ), 1 );
					AKSIMD_V2F32 bb = vdup_lane_f32( vget_low_f32( abcd ), 1 );
					AKSIMD_V4F32 yybb = vcombine_f32( yy, bb );
					return yybb;
				}
	/* 86 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zybb( xyzw, abcd ); }
	/* 87 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wybb( xyzw, abcd ); }
	/* 88 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzbb( xyzw, abcd ); }
	/* 89 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzbb( xyzw, abcd ); }
	/* 90 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzbb( xyzw, abcd ); }
	/* 91 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzbb( xyzw, abcd ); }
	/* 92 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwbb( xyzw, abcd ); }
	/* 93 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywbb( xyzw, abcd ); }
	/* 94 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwbb( xyzw, abcd ); }
	/* 95 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 1, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwbb( xyzw, abcd ); }
	/* 96 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxcb( xyzw, abcd ); }
	/* 97 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxcb( xyzw, abcd ); }
	/* 98 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxcb( xyzw, abcd ); }
	/* 99 */ 	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxcb( xyzw, abcd ); }
	/* 100 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xycb( xyzw, abcd ); }
	/* 101 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yycb( xyzw, abcd ); }
	/* 102 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zycb( xyzw, abcd ); }
	/* 103 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wycb( xyzw, abcd ); }
	/* 104 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzcb( xyzw, abcd ); }
	/* 105 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzcb( xyzw, abcd ); }
	/* 106 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzcb( xyzw, abcd ); }
	/* 107 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzcb( xyzw, abcd ); }
	/* 108 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwcb( xyzw, abcd ); }
	/* 109 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywcb( xyzw, abcd ); }
	/* 110 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwcb( xyzw, abcd ); }
	/* 111 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 2, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwcb( xyzw, abcd ); }
	/* 112 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxdb( xyzw, abcd ); }
	/* 113 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxdb( xyzw, abcd ); }
	/* 114 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_zxdb( xyzw, abcd );

					float32x2x2_t xz_yw = vtrn_f32( vget_low_f32( xyzw ), vget_high_f32( xyzw ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 zxdb = vcombine_f32( vrev64_f32( xz_yw.val[0] ), vrev64_f32( ac_bd.val[1] ) );

					return zxdb;
				}
	/* 115 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxdb( xyzw, abcd ); }
	/* 116 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xydb( xyzw, abcd ); }
	/* 117 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yydb( xyzw, abcd ); }
	/* 118 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zydb( xyzw, abcd ); }
	/* 119 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wydb( xyzw, abcd ); }
	/* 120 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzdb( xyzw, abcd ); }
	/* 121 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzdb( xyzw, abcd ); }
	/* 122 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzdb( xyzw, abcd ); }
	/* 123 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzdb( xyzw, abcd ); }
	/* 124 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwdb( xyzw, abcd ); }
	/* 125 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywdb( xyzw, abcd ); }
	/* 126 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwdb( xyzw, abcd ); }
	/* 127 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(1, 3, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwdb( xyzw, abcd ); }
	/* 128 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxac( xyzw, abcd ); }
	/* 129 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yxac( xyzw, abcd );
					AKSIMD_V2F32 ac = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) ).val[0];
					AKSIMD_V2F32 yx = vrev64_f32( vget_low_f32( xyzw ) );
					AKSIMD_V4F32 yxac = vcombine_f32( yx, ac );
					return yxac;
				}
	/* 130 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxac( xyzw, abcd ); }
	/* 131 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxac( xyzw, abcd ); }
	/* 132 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyac( xyzw, abcd ); }
	/* 133 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyac( xyzw, abcd ); }
	/* 134 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyac( xyzw, abcd ); }
	/* 135 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyac( xyzw, abcd ); }
	/* 136 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xzac( xyzw, abcd );
					float32x2x2_t xz_yw = vtrn_f32( vget_low_f32( xyzw ), vget_high_f32( xyzw ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 xzac = vcombine_f32( xz_yw.val[0], ac_bd.val[0] );
					return xzac;
				}
	/* 137 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yzac( xyzw, abcd );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V2F32 yz = vget_low_f32( yzwx );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 yzac = vcombine_f32( yz, ac_bd.val[0] );
					return yzac;
				}
	/* 138 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzac( xyzw, abcd ); }
	/* 139 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzac( xyzw, abcd ); }
	/* 140 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xwac( xyzw, abcd );
					AKSIMD_V2F32 xw = vrev64_f32( vext_f32( vget_high_f32( xyzw ) , vget_low_f32( xyzw ) , 1 ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 xwac = vcombine_f32( xw, ac_bd.val[0] );;
					return xwac;
				}
	/* 141 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_ywac( xyzw, abcd );
					float32x2x2_t xz_yw = vtrn_f32( vget_low_f32( xyzw ), vget_high_f32( xyzw ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 ywac = vcombine_f32( xz_yw.val[1], ac_bd.val[0] );
					return ywac;
				}
	/* 142 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwac( xyzw, abcd ); }
	/* 143 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 0, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwac( xyzw, abcd ); }
	/* 144 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxbc( xyzw, abcd ); }
	/* 145 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxbc( xyzw, abcd ); }
	/* 146 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxbc( xyzw, abcd ); }
	/* 147 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxbc( xyzw, abcd ); }
	/* 148 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xybc( xyzw, abcd ); }
	/* 149 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yybc( xyzw, abcd ); }
	/* 150 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zybc( xyzw, abcd ); }
	/* 151 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wybc( xyzw, abcd ); }
	/* 152 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzbc( xyzw, abcd ); }
	/* 153 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yzbc( xyzw, abcd );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V2F32 yz = vget_low_f32( yzwx );
					AKSIMD_V2F32 bc = vext_f32( vget_low_f32( abcd ) , vget_high_f32( abcd ) , 1 );
					AKSIMD_V4F32 yzbc = vcombine_f32( yz , bc );
					return yzbc;
				}
	/* 154 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzbc( xyzw, abcd ); }
	/* 155 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzbc( xyzw, abcd ); }
	/* 156 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xwbc( xyzw, abcd );
					AKSIMD_V2F32 xw = vrev64_f32( vext_f32( vget_high_f32( xyzw ) , vget_low_f32( xyzw ) , 1 ) );
					AKSIMD_V2F32 bc = vext_f32( vget_low_f32( abcd ) , vget_high_f32( abcd ) , 1 );
					AKSIMD_V4F32 xwbc = vcombine_f32( xw, bc );
					return xwbc;
				}
	/* 157 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_ywbc( xyzw, abcd );
					AKSIMD_V2F32 yw = vext_f32( vget_low_f32( xyzw ) , vrev64_f32( vget_high_f32( xyzw ) ), 1 );
					AKSIMD_V2F32 bc = vext_f32( vget_low_f32( abcd ) , vget_high_f32( abcd ) , 1 );
					AKSIMD_V4F32 ywbc = vcombine_f32( yw, bc );
					return ywbc;
				}
	/* 158 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwbc( xyzw, abcd ); }
	/* 159 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 1, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwbc( xyzw, abcd ); }
	/* 160 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{ 
					AKSIMD_V2F32 xx = vdup_lane_f32( vget_low_f32( xyzw ), 0 );
					AKSIMD_V2F32 cc = vdup_lane_f32( vget_high_f32( abcd ), 0 );
					AKSIMD_V4F32 xxcc = vcombine_f32( xx, cc );
					return xxcc; 
				}
	/* 161 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxcc( xyzw, abcd ); }
	/* 162 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxcc( xyzw, abcd ); }
	/* 163 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxcc( xyzw, abcd ); }
	/* 164 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xycc( xyzw, abcd ); }
	/* 165 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yycc( xyzw, abcd ); }
	/* 166 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zycc( xyzw, abcd ); }
	/* 167 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wycc( xyzw, abcd ); }
	/* 168 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzcc( xyzw, abcd ); }
	/* 169 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzcc( xyzw, abcd ); }
	/* 170 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzcc( xyzw, abcd ); }
	/* 171 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzcc( xyzw, abcd ); }
	/* 172 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwcc( xyzw, abcd ); }
	/* 173 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywcc( xyzw, abcd ); }
	/* 174 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwcc( xyzw, abcd ); }
	/* 175 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 2, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwcc( xyzw, abcd ); }
	/* 176 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxdc( xyzw, abcd ); }
	/* 177 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) 
				{ 
					AKSIMD_V2F32 yx = vrev64_f32( vget_low_f32( xyzw ) );
					AKSIMD_V2F32 dc = vrev64_f32( vget_high_f32( abcd ) );
					AKSIMD_V4F32 yxdc = vcombine_f32( yx, dc );
					return yxdc; 
				}
	/* 178 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxdc( xyzw, abcd ); }
	/* 179 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxdc( xyzw, abcd ); }
	/* 180 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xydc( xyzw, abcd ); }
	/* 181 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yydc( xyzw, abcd ); }
	/* 182 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zydc( xyzw, abcd ); }
	/* 183 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wydc( xyzw, abcd ); }
	/* 184 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzdc( xyzw, abcd ); }
	/* 185 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzdc( xyzw, abcd ); }
	/* 186 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzdc( xyzw, abcd ); }
	/* 187 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_wzdc( xyzw, abcd );
					AKSIMD_V2F32 wz = vrev64_f32( vget_high_f32( xyzw ) );
					AKSIMD_V2F32 dc = vrev64_f32( vget_high_f32( abcd ) );
					AKSIMD_V4F32 wzdc = vcombine_f32( wz , dc );
					return wzdc;
				}
	/* 188 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwdc( xyzw, abcd ); }
	/* 189 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywdc( xyzw, abcd ); }
	/* 190 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwdc( xyzw, abcd ); }
	/* 191 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(2, 3, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwdc( xyzw, abcd ); }
	/* 192 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxad( xyzw, abcd ); }
	/* 193 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxad( xyzw, abcd ); }
	/* 194 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxad( xyzw, abcd ); }
	/* 195 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxad( xyzw, abcd ); }
	/* 196 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xyad( xyzw, abcd ); }
	/* 197 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yyad( xyzw, abcd ); }
	/* 198 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zyad( xyzw, abcd ); }
	/* 199 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wyad( xyzw, abcd ); }
	/* 200 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xzad( xyzw, abcd );
					AKSIMD_V2F32 xz = vext_f32( vrev64_f32( vget_low_f32( xyzw ) ), vget_high_f32( xyzw ) , 1 );
					AKSIMD_V2F32 ad = vrev64_f32( vext_f32( vget_high_f32( abcd ) , vget_low_f32( abcd ) , 1 ) );
					AKSIMD_V4F32 xzad = vcombine_f32( xz , ad );
					return xzad;
				}
	/* 201 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yzad( xyzw, abcd );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V2F32 yz = vget_low_f32( yzwx );
					AKSIMD_V2F32 ad = vrev64_f32( vext_f32( vget_high_f32( abcd ) , vget_low_f32( abcd ) , 1 ) );
					AKSIMD_V4F32 yzad = vcombine_f32( yz , ad );
					return yzad;
				}
	/* 202 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzad( xyzw, abcd ); }
	/* 203 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzad( xyzw, abcd ); }
	/* 204 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xwad( xyzw, abcd );
					AKSIMD_V2F32 xw = vrev64_f32( vext_f32( vget_high_f32( xyzw ) , vget_low_f32( xyzw ) , 1 ) );
					AKSIMD_V2F32 ad = vrev64_f32( vext_f32( vget_high_f32( abcd ) , vget_low_f32( abcd ) , 1 ) );
					AKSIMD_V4F32 xwad = vcombine_f32( xw, ad );
					return xwad;
				}
	/* 205 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywad( xyzw, abcd ); }
	/* 206 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwad( xyzw, abcd ); }
	/* 207 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 0, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwad( xyzw, abcd ); }
	/* 208 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxbd( xyzw, abcd ); }
	/* 209 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxbd( xyzw, abcd ); }
	/* 210 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxbd( xyzw, abcd ); }
	/* 211 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxbd( xyzw, abcd ); }
	/* 212 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xybd( xyzw, abcd ); }
	/* 213 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yybd( xyzw, abcd ); }
	/* 214 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zybd( xyzw, abcd ); }
	/* 215 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wybd( xyzw, abcd ); }
	/* 216 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xzbd( xyzw, abcd );
					float32x2x2_t xz_yw = vtrn_f32( vget_low_f32( xyzw ), vget_high_f32( xyzw ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 xzbd = vcombine_f32( xz_yw.val[0], ac_bd.val[1] );
					return xzbd;				
				}
	/* 217 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_yzbd( xyzw, abcd );
					AKSIMD_V4F32 yzwx = vextq_f32( xyzw, xyzw, 1 );
					AKSIMD_V2F32 yz = vget_low_f32( yzwx );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 yzbd = vcombine_f32( yz, ac_bd.val[1] );
					return yzbd;
				}
	/* 218 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzbd( xyzw, abcd ); }
	/* 219 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzbd( xyzw, abcd ); }
	/* 220 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xwbd( xyzw, abcd );
					AKSIMD_V2F32 xw = vrev64_f32( vext_f32( vget_high_f32( xyzw ) , vget_low_f32( xyzw ) , 1 ) );
					AKSIMD_V2F32 bd = vext_f32( vget_low_f32( abcd ) , vrev64_f32( vget_high_f32( abcd ) ), 1 );

					AKSIMD_V4F32 xwbd = vcombine_f32( xw, bd );
					return xwbd;
				}
	/* 221 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_ywbd( xyzw, abcd );
					float32x2x2_t xz_yw = vtrn_f32( vget_low_f32( xyzw ), vget_high_f32( xyzw ) );
					float32x2x2_t ac_bd = vtrn_f32( vget_low_f32( abcd ), vget_high_f32( abcd ) );
					AKSIMD_V4F32 ywbd = vcombine_f32( xz_yw.val[1], ac_bd.val[1] );
					return ywbd;
				}
	/* 222 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwbd( xyzw, abcd ); }
	/* 223 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 1, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwbd( xyzw, abcd ); }
	/* 224 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxcd( xyzw, abcd ); }
	/* 225 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxcd( xyzw, abcd ); }
	/* 226 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxcd( xyzw, abcd ); }
	/* 227 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxcd( xyzw, abcd ); }
	/* 228 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_xycd( xyzw, abcd );
					AKSIMD_V2F32 xy = vget_low_f32( xyzw );
					AKSIMD_V2F32 cd = vget_high_f32( abcd );
					return vcombine_f32( xy, cd );
				}
	/* 229 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yycd( xyzw, abcd ); }
	/* 230 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zycd( xyzw, abcd ); }
	/* 231 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wycd( xyzw, abcd ); }
	/* 232 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzcd( xyzw, abcd ); }
	/* 233 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzcd( xyzw, abcd ); }
	/* 234 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzcd( xyzw, abcd ); }
	/* 235 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzcd( xyzw, abcd ); }
	/* 236 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwcd( xyzw, abcd ); }
	/* 237 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywcd( xyzw, abcd ); }
	/* 238 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_zwcd( xyzw, abcd );
					AKSIMD_V2F32 zw = vget_high_f32( xyzw );
					AKSIMD_V2F32 cd = vget_high_f32( abcd );
					AKSIMD_V4F32 zwcd = vcombine_f32( zw , cd );
					return zwcd;
				}
	/* 239 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 2, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wwcd( xyzw, abcd ); }
	/* 240 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 0, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xxdd( xyzw, abcd ); }
	/* 241 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 0, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yxdd( xyzw, abcd ); }
	/* 242 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 0, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zxdd( xyzw, abcd ); }
	/* 243 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 0, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wxdd( xyzw, abcd ); }
	/* 244 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 1, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xydd( xyzw, abcd ); }
	/* 245 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 1, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) 
				{	
					AKSIMD_V2F32 yy = vdup_lane_f32( vget_low_f32( xyzw ), 1 );			
					AKSIMD_V2F32 dd = vdup_lane_f32( vget_high_f32( abcd ), 1 );
					AKSIMD_V4F32 yydd = vcombine_f32( yy, dd );
					return yydd; 
				}
	/* 246 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 1, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zydd( xyzw, abcd ); }
	/* 247 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 1, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wydd( xyzw, abcd ); }
	/* 248 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 2, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xzdd( xyzw, abcd ); }
	/* 249 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 2, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_yzdd( xyzw, abcd ); }
	/* 250 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 2, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zzdd( xyzw, abcd ); }
	/* 251 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 2, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_wzdd( xyzw, abcd ); }
	/* 252 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 3, 0)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_xwdd( xyzw, abcd ); }
	/* 253 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 3, 1)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_ywdd( xyzw, abcd ); }
	/* 254 */	//template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 3, 2)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd ) { return akshuffle_zwdd( xyzw, abcd ); }
	/* 255 */	template<> AkForceInline AKSIMD_V4F32 SHUFFLE_V4F32<AKSIMD_SHUFFLE(3, 3, 3, 3)>( const AKSIMD_V4F32& xyzw, const AKSIMD_V4F32& abcd )
				{
					//return akshuffle_wwdd( xyzw, abcd );
					AKSIMD_V2F32 ww = vdup_lane_f32( vget_high_f32( xyzw ), 1 );
					AKSIMD_V2F32 dd = vdup_lane_f32( vget_high_f32( abcd ), 1 );
					AKSIMD_V4F32 wwdd = vcombine_f32( ww, dd );
					return wwdd;
				}
}

#endif // __AK_SIMD_SHUFFLE_H__
