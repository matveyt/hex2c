#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// load Intel HEX or Binary file
// return binary image pointer or NULL
// note: 8-bit format only, may rewind(), caller must free()
uint8_t* ihex_load8(size_t* sz, size_t* base, size_t* entry, FILE* f);

// convert hex string to byte array
// return number of bytes converted
size_t ihex_blob(uint8_t* blob, size_t length, const char* str);
