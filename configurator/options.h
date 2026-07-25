#pragma once
#include <string>
#include <vector>

struct Options {
    bool useLegacyAmmonia = false;
    bool disablePAC = false;
    bool pauseInjection = false;
    std::vector<std::string> enabledTweaks;
};

Options loadOptions();
bool saveOptions(const Options& opts);
