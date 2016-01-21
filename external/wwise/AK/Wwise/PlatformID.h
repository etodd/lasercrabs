//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

/// \file
/// Unique identifiers for platforms in the Wwise authoring application.

#ifndef AK_WWISE_PLATFORMID_H
#define AK_WWISE_PLATFORMID_H

#if defined( _MSC_VER )
	#define AK_ID_DECLARE extern const _declspec( selectany )
#else
	#define AK_ID_DECLARE const
#endif

struct BasePlatformID
{
	BasePlatformID():guid( GUID_NULL ){}

	// The Create function is a functionnal replacement for the BasePlatformID() constructor taking a GUID in parameters.
	// This is to avoid situations where someone would pass a GUID instead of a BasePlatformID in a function call while migrating, which would
	// compile but would lead the user into serious errors.
	static BasePlatformID Create( const GUID& in_BasePlatformID  )
	{
		BasePlatformID ret;
		ret.guid = in_BasePlatformID;
		return ret;
	}

	static BasePlatformID Empty()
	{
		return BasePlatformID::Create(GUID_NULL);
	}

	bool IsNull() const
	{
		return (guid == GUID_NULL) ? true : false;
	}

	bool operator == ( const BasePlatformID& in_rOther ) const
	{
		return (in_rOther.guid == guid)? true : false;
	}
	bool operator != ( const BasePlatformID& in_rOther ) const
	{
		return (in_rOther.guid != guid)? true : false;
	}
	GUID guid;

	struct GUIDLessNative
	{
		bool operator()( const BasePlatformID& in_rLeft, const BasePlatformID& in_rRight ) const
		{
			return ( ::memcmp( &in_rLeft, &in_rRight, sizeof BasePlatformID ) < 0 );
		}
	};
};

namespace PlatformID
{
    // {6E0CB257-C6C8-4c5c-8366-2740DFC441EB}
    AK_ID_DECLARE GUID Windows_unsafeguid = { 0x6E0CB257, 0xC6C8, 0x4c5c, { 0x83, 0x66, 0x27, 0x40, 0xDF, 0xC4, 0x41, 0xEB } };
	AK_ID_DECLARE BasePlatformID Windows = BasePlatformID::Create( Windows_unsafeguid );

    // {E0C09284-6F61-43dc-9C9D-D8047E47AB3B}
    AK_ID_DECLARE GUID Xbox360_unsafeguid = { 0xE0C09284, 0x6F61, 0x43dc, { 0x9C, 0x9D, 0xD8, 0x04, 0x7E, 0x47, 0xAB, 0x3B } };
	AK_ID_DECLARE BasePlatformID Xbox360 = BasePlatformID::Create( Xbox360_unsafeguid );

	// {D85DACB3-8FDB-4aba-8C8A-1F46AFB35366}
    AK_ID_DECLARE GUID PS3_unsafeguid = { 0xD85DACB3, 0x8FDB, 0x4aba, { 0x8C, 0x8A, 0x1F, 0x46, 0xAF, 0xB3, 0x53, 0x66 } };
	AK_ID_DECLARE BasePlatformID PS3 = BasePlatformID::Create( PS3_unsafeguid );

    // {A11C9D5D-C4ED-42af-99E3-0376D0E11620}
	AK_ID_DECLARE GUID Wii_unsafeguid = { 0xa11c9d5d, 0xc4ed, 0x42af, { 0x99, 0xe3, 0x3, 0x76, 0xd0, 0xe1, 0x16, 0x20 } };
	AK_ID_DECLARE BasePlatformID Wii = BasePlatformID::Create( Wii_unsafeguid );

	// {9C6217D5-DD11-4795-87C1-6CE02853C540}
	AK_ID_DECLARE GUID Mac_unsafeguid = { 0x9c6217d5, 0xdd11, 0x4795, { 0x87, 0xc1, 0x6c, 0xe0, 0x28, 0x53, 0xc5, 0x40 } };
	AK_ID_DECLARE BasePlatformID Mac = BasePlatformID::Create( Mac_unsafeguid );

	// {2DB55050-4B70-4243-A22C-434D776A0753}
	AK_ID_DECLARE GUID VitaSW_unsafeguid = { 0x2db55050, 0x4b70, 0x4243, { 0xa2, 0x2c, 0x43, 0x4d, 0x77, 0x6a, 0x7, 0x53 } };
	AK_ID_DECLARE BasePlatformID VitaSW = BasePlatformID::Create( VitaSW_unsafeguid );
	
	// {091EBCFF-5E04-4260-B025-566206272EA6}
	AK_ID_DECLARE GUID VitaHW_unsafeguid = { 0x91ebcff, 0x5e04, 0x4260, { 0xb0, 0x25, 0x56, 0x62, 0x6, 0x27, 0x2e, 0xa6 } };
	AK_ID_DECLARE BasePlatformID VitaHW = BasePlatformID::Create( VitaHW_unsafeguid );
	
	// {3AF9B9B6-6EF1-47E9-B5FE-E30C9E602C77}
	AK_ID_DECLARE GUID PS4_unsafeguid = { 0x3af9b9b6, 0x6ef1, 0x47e9, { 0xb5, 0xfe, 0xe3, 0xc, 0x9e, 0x60, 0x2c, 0x77 } };
	AK_ID_DECLARE BasePlatformID PS4 = BasePlatformID::Create( PS4_unsafeguid );

	// {ECE03DB4-F948-462d-B2BB-A9173012B1F8}
	AK_ID_DECLARE GUID iOS_unsafeguid = { 0xece03db4, 0xf948, 0x462d, { 0xb2, 0xbb, 0xa9, 0x17, 0x30, 0x12, 0xb1, 0xf8 } };
	AK_ID_DECLARE BasePlatformID iOS = BasePlatformID::Create( iOS_unsafeguid );

	// {3B6B2F86-291B-4b95-90FD-6AD751FF6DCA}
	AK_ID_DECLARE GUID Nintendo3DS_unsafeguid = { 0x3b6b2f86, 0x291b, 0x4b95, { 0x90, 0xfd, 0x6a, 0xd7, 0x51, 0xff, 0x6d, 0xca } };
	AK_ID_DECLARE BasePlatformID Nintendo3DS = BasePlatformID::Create( Nintendo3DS_unsafeguid );

	// {7CB75869-58AD-4458-948A-1935CCB4AC66}
	AK_ID_DECLARE GUID WiiUSW_unsafeguid = { 0x7cb75869, 0x58ad, 0x4458, { 0x94, 0x8a, 0x19, 0x35, 0xcc, 0xb4, 0xac, 0x66 } };
	AK_ID_DECLARE BasePlatformID WiiUSW = BasePlatformID::Create( WiiUSW_unsafeguid );

	// {FF757AE1-FCE5-420d-9E8A-32139D436F74}
	AK_ID_DECLARE GUID WiiUHW_unsafeguid = { 0xff757ae1, 0xfce5, 0x420d, { 0x9e, 0x8a, 0x32, 0x13, 0x9d, 0x43, 0x6f, 0x74 } };
	AK_ID_DECLARE BasePlatformID WiiUHW = BasePlatformID::Create( WiiUHW_unsafeguid );

	// {a2d401de-b8b6-4feb-8142-137c34d507CA}
	AK_ID_DECLARE GUID Android_unsafeguid = { 0xa2d401de, 0xb8b6, 0x4feb, { 0x81, 0x42, 0x13, 0x7c, 0x34, 0xd5, 0x07, 0xCA } };
	AK_ID_DECLARE BasePlatformID Android = BasePlatformID::Create( Android_unsafeguid );

	// {B131584B-9961-4bb5-9C58-A3E9ABFFBBF6}
	AK_ID_DECLARE GUID XboxOne_unsafeguid = { 0xb131584b, 0x9961, 0x4bb5, { 0x9c, 0x58, 0xa3, 0xe9, 0xab, 0xff, 0xbb, 0xf6 } };
	AK_ID_DECLARE BasePlatformID XboxOne = BasePlatformID::Create( XboxOne_unsafeguid );

	// {BD0BDF13-3125-454f-8BFD-319537169F81}
	AK_ID_DECLARE GUID Linux_unsafeguid = { 0xbd0bdf13, 0x3125, 0x454f, { 0x8b, 0xfd, 0x31, 0x95, 0x37, 0x16, 0x9f, 0x81 } };
	AK_ID_DECLARE BasePlatformID Linux = BasePlatformID::Create( Linux_unsafeguid );

	// {869AD233-43F2-4c0f-9126-79F44C15E1D9}
	AK_ID_DECLARE GUID WindowsPhone_unsafeguid = { 0x869ad233, 0x43f2, 0x4c0f, { 0x91, 0x26, 0x79, 0xf4, 0x4c, 0x15, 0xe1, 0xd9 } };
	AK_ID_DECLARE BasePlatformID WindowsPhone = BasePlatformID::Create( WindowsPhone_unsafeguid );

	/// Returns true if the given platform has Big Endian byte ordering. 
	inline bool IsPlatformBigEndian( const BasePlatformID & in_guidPlatform )
	{
		return in_guidPlatform == PlatformID::Wii 
			|| in_guidPlatform == PlatformID::WiiUSW
			|| in_guidPlatform == PlatformID::WiiUHW
			|| in_guidPlatform == PlatformID::PS3 
			|| in_guidPlatform == PlatformID::Xbox360;
	}
}

#endif // AK_WWISE_PLATFORMID_H
