#pragma once

#include <string>

std::string absolutePath(const std::string&, const std::string&);
int64_t     configStringToInt(const std::string& VALUE);
std::string spawnSync(const std::string& cmd);
void        spawnAsync(const std::string& cmd);
