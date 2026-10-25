#pragma once

#include "Image.h"

#include <string>

Image ReadBMP(const std::string& inppath);
void WriteBMP(const std::string& outpath, const Image& output);
