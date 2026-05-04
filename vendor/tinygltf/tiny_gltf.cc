#define TINYGLTF_IMPLEMENTATION
// stb_image implementation is provided by vendor/stb to avoid duplicate
// symbol definitions at link time. stb_image_write stays here since stb
// vendor lib does not provide it.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tinygltf/tiny_gltf.h"
