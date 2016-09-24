/// \file 
///This file is used only to provide backward compatibility with the previous plugin system.
///Please read the SDK documentation regarding effect plugin registration.
///\sa quickstart_sample_integration_plugins
#include <AK/Plugin/AllPluginsFactories.h>

namespace AK
{
	namespace SoundEngine
	{
		/// This function is deprecated.  All registration is automatic now.  Kept for backward compatibility.
		static AKRESULT RegisterAllPlugins()
		{
			return AK_Success;
		}
	}
}
