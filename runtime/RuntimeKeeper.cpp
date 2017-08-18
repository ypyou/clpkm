/*
  RuntimeKeeper.cpp

  Globals or so

*/



#include "RuntimeKeeper.hpp"
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <pwd.h>



namespace {

struct DefaultLoader : public CLPKM::RuntimeKeeper::ConfigLoader {
	CLPKM::RuntimeKeeper::state_t operator()(CLPKM::RuntimeKeeper& RT) override {
		// yaml-cpp throws exception on error
		try {
			std::string ConfigPath;
			if (const char* Home = getenv("HOME"); Home != nullptr)
				ConfigPath = Home;
			else
				// FIXME: this is not thread-safe
				ConfigPath = getpwuid(getuid())->pw_dir;
			ConfigPath += "/.clpkmrc";
			YAML::Node Config = YAML::LoadFile(ConfigPath);
			// TODO: work with daemon via UNIX domain socket or so
			if (Config["compiler"])
				RT.setCompilerPath(Config["compiler"].as<std::string>());
			if (Config["threshold"])
				RT.setCRThreshold(Config["threshold"].as<CLPKM::tlv_t>());
			// Override if specified from environment variable
			if (const char* Fine = getenv("CLPKM_PRIORITY"); Fine != nullptr)
				if (strcmp(Fine, "high") == 0)
					RT.setPriority(CLPKM::RuntimeKeeper::priority::HIGH);
			}
		catch (...) {
			return CLPKM::RuntimeKeeper::INVALID_CONFIG;
			}
		return CLPKM::RuntimeKeeper::SUCCEED;
		}
	};

}



// Customize initializer here if needed
CLPKM::RuntimeKeeper& CLPKM::getRuntimeKeeper(void) {
	DefaultLoader Loader;
	static RuntimeKeeper RT(Loader);
	return RT;
	}