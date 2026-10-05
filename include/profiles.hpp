#ifndef PROFILES_HPP
#define PROFILES_HPP
// Named network profiles. Values are PER DIRECTION (RTT ~ 2x delay when applied on both sides).
#include "emulator.hpp"
#include <string>
#include <utility>
#include <vector>

using ProfileList = std::vector<std::pair<std::string, Impairment>>;
const ProfileList& profiles();                       // insertion-ordered, same as Python
bool hasProfile(const std::string& name);
const Impairment& getProfile(const std::string& name);   // throws std::out_of_range if unknown

#endif
