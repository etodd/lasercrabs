//////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2006 Audiokinetic Inc. / All Rights Reserved
//
//////////////////////////////////////////////////////////////////////

using System;
using System.Text;

namespace AK
{
	namespace Wwise
	{
		public static class Version
		{
			#region Wwise SDK Version - Numeric values

			/// <summary>
			/// Wwise SDK major version
			/// </summary>
            public const int Major = 2017;

			/// <summary>
			/// Wwise SDK minor version
			/// </summary>
            public const int Minor = 5;

			/// <summary>
			/// Wwise SDK sub-minor version
			/// </summary>
            public const int SubMinor = 12;

			/// <summary>
			/// Wwise SDK build number
			/// </summary>
            public const int Build = 6098;
			
			/// <summary>
			/// Wwise SDK build nickname
			/// </summary>
			public const string Nickname = "";

			#endregion Wwise SDK Version - Numeric values

			#region Wwise SDK Version - String values

			/// <summary>
			/// String representing the Wwise SDK version
			/// </summary>
            public static string VersionName
            {
                get
                {
                    return "v" + Major + "." + Minor + "." + SubMinor + (Nickname.Length == 0 ? "" : "_" + Nickname);
                }
            }

            /// <summary>
            /// String representing the Wwise SDK version
            /// </summary>
            public const string AssemblyVersion = "2017.5.12.6098";

			/// <summary>
			/// String representing the Wwise SDK copyright notice
			/// </summary>
            public const string CopyrightNotice = "\xA9 2006-2016. Audiokinetic Inc. All rights reserved.";

            #endregion Wwise SDK Version - String values
        }
	}
}