#include <array>
#include <charconv> // std::from_chars
#include <cstddef>  // size_t
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

constexpr size_t BINARY_STL_HEADER_SIZE = 80;

struct Vec3f {
  float x;
  float y;
  float z;
};

struct Triangle {
  Vec3f normal;
  std::array<Vec3f, 3> vertices;
};

static void read_binary_stl(uint32_t num_triangles, std::ifstream &ifs, std::vector<Triangle> &triangles) {
  triangles.reserve(triangles.size() + num_triangles);
  for (uint32_t i = 0; i < num_triangles; ++i) {
    Triangle t;
    ifs.read((char *)&t, sizeof(Triangle));
    triangles.push_back(t);
    uint16_t attribute_byte_count;
    ifs.read((char *)&attribute_byte_count, sizeof(uint16_t));
  }
}

static void read_ascii_stl(std::ifstream &ifs, std::vector<Triangle> &triangles) {
  while (ifs.good()) {
    std::string token;
    ifs >> std::ws >> token >> std::ws;
    if (token == "facet") {
      Triangle t;
      ifs >> std::ws >> token >> std::ws; // expecting "normal"
      ifs >> t.normal.x >> t.normal.y >> t.normal.z;
      ifs >> std::ws >> token >> std::ws; // expecting "outer"
      ifs >> std::ws >> token >> std::ws; // expecting "loop"
      for (int i = 0; i < 3; i++) {
        ifs >> std::ws >> token >> std::ws; // expecting "vertex"
        ifs >> t.vertices[i].x >> t.vertices[i].y >> t.vertices[i].z;
      }
      ifs >> std::ws >> token >> std::ws; // expecting "endloop"
      ifs >> std::ws >> token >> std::ws; // expecting "endfacet"
      triangles.push_back(t);
    }
  }
}

static void read_stl(size_t file_size, std::ifstream &ifs, std::vector<Triangle> &triangles) {
  ifs.seekg(BINARY_STL_HEADER_SIZE, std::ifstream::beg); // Seek right past the header

  uint32_t num_triangles = 0;
  ifs.read((char *)&num_triangles, sizeof(uint32_t));

  if (file_size ==
      (BINARY_STL_HEADER_SIZE + sizeof(uint32_t) + num_triangles * (sizeof(Triangle) + sizeof(uint16_t)))) {
    read_binary_stl(num_triangles, ifs, triangles);
  } else {
    ifs.seekg(0, std::ifstream::beg);
    read_ascii_stl(ifs, triangles);
  }
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Expected arguments: /path/to/mesh/file" << std::endl;
    return 1;
  }

  const char *filepath = argv[1];

  // What is the idiomatic C++17 standard approach to reading binary files?
  // https://stackoverflow.com/a/51353040/8094047

  std::ifstream ifs;
  ifs.exceptions(std::ifstream::badbit | std::ifstream::failbit); // Raise exceptions on failure

  try {
    ifs.open(filepath, std::ifstream::binary);
  } catch (const std::ifstream::failure &e) {
    std::cerr << "Failed to open file: " << filepath << std::endl;
    std::cerr << e.what() << std::endl;
    return 1;
  }

  ifs.seekg(0, std::ifstream::end);
  auto end = ifs.tellg();
  ifs.seekg(0, std::ifstream::beg);
  size_t file_size = end - ifs.tellg();

  if (file_size == 0) {
    std::cout << "Empty file" << std::endl;
    return 0;
  }
  std::vector<Triangle> triangles;
  read_stl(file_size, ifs, triangles);
  std::cout << "Number of triangles: " << triangles.size() << std::endl;

  return 0;
}
