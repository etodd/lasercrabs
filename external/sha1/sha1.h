/*
	Original C Code
		-- Steve Reid <steve@edmweb.com>
	Small changes to fit into bglibs
		-- Bruce Guenter <bruce@untroubled.org>
	Translation to simpler C++ Code
		-- Volker Grabsch <vog@notjusthosting.com>
	Safety fixes
		-- Eugene Hopkinson <slowriot at voxelstorm dot com>
	Stupid stylistic gamedev changes
		-- Evan Todd <evan@etodd.io>
*/

#pragma once

namespace sha1
{

    struct Digest
    {
    	uint32_t digest[5];
    	std::string buffer;
    	uint64_t transforms;

    	SHA1();
    	void update(const std::string &s);
    	void update(std::istream &is);
    	std::string final();
    };


}