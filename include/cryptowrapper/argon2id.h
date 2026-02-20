#pragma once
#include <vector>
#include <argon2.h>
#include <openssl/rand.h>
#include <iostream>

std::vector<uint8_t> argonidhash(const std::vector<uint8_t>& input);