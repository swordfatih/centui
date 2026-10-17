#include <spdlog/spdlog.h>

import starter;
import std;

int main()
{
    spdlog::info("{}", starter::starter::to_string());
}