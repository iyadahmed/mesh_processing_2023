#include <algorithm> // std::transform
#include <array>
#include <charconv> // std::from_chars
#include <cmath>
#include <cstddef> // size_t
#include <cstdint>
#include <exception>
#include <format>
#include <fstream>
#include <functional> // std::equal_to
#include <iomanip>    // std::quoted
#include <iostream>
#include <limits>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

constexpr size_t BINARY_STL_HEADER_SIZE = 80;

struct Vec3f {
  float x;
  float y;
  float z;

  Vec3f cross(const Vec3f &other) const {
    return Vec3f{
        y * other.z - z * other.y,
        z * other.x - x * other.z,
        x * other.y - y * other.x,
    };
  }

  Vec3f operator/(float s) const { return Vec3f{x / s, y / s, z / s}; }
  friend Vec3f operator/(float s, const Vec3f &v) { return Vec3f{s / v.x, s / v.y, s / v.z}; }
  Vec3f operator*(float s) const { return Vec3f{x * s, y * s, z * s}; }
  friend Vec3f operator*(float s, const Vec3f &v) { return v * s; }
  float calc_magnitude() const { return std::sqrt(x * x + y * y + z * z); }
  void normalize() {
    float m = calc_magnitude();
    x /= m;
    y /= m;
    z /= m;
  }
  Vec3f operator-(const Vec3f &other) const { return Vec3f{x - other.x, y - other.y, z - other.z}; }
  Vec3f operator+(const Vec3f &other) const { return Vec3f{x + other.x, y + other.y, z + other.z}; }
};

struct Triangle {
  Vec3f normal;
  std::array<Vec3f, 3> vertices;
};

struct PLY_Property_Definition {
  enum class Type {
    List,
    Scalar,
  };
  Type type;
  std::string name;
};

struct PLY_Element_Definition {
  std::string name;
  size_t count;
  std::vector<PLY_Property_Definition> property_definitions;
};

struct String_Hash {
  using is_transparent =
      void; // enable "heterogeneous lookup" for this hash to avoid creating temporary strings, thanks SonarLint!
  size_t operator()(std::string_view s) const { return std::hash<std::string_view>{}(s); }
};

template <typename T> using String_Map = std::unordered_map<std::string, T, String_Hash, std::equal_to<>>;

struct PLY_Property {
  /* scalar properties are vectors of size 1, list properties are larger
   * a double should be able to hold all types of numeric data supported by PLY specification
   */
  std::vector<double> values;
};

struct PLY_Element {
  String_Map<PLY_Property> property_map;
};

struct Parsed_PLY {
  String_Map<std::vector<PLY_Element>> elements_map;
};

class PLY_Expected_Element_Definition_Error : public std::exception {
public:
  const char *what() const throw() final {
    return "Expected at least one element definition before property definition";
  }
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
    ifs >> token;
    if (token == "facet") {
      Triangle t;
      ifs >> token; // expecting "normal"
      ifs >> t.normal.x >> t.normal.y >> t.normal.z;
      ifs >> token; // expecting "outer"
      ifs >> token; // expecting "loop"
      for (int i = 0; i < 3; i++) {
        ifs >> token; // expecting "vertex"
        ifs >> t.vertices[i].x >> t.vertices[i].y >> t.vertices[i].z;
      }
      ifs >> token; // expecting "endloop"
      ifs >> token; // expecting "endfacet"
      triangles.push_back(t);
    }
  }
}

static size_t calc_file_size(std::ifstream &ifs) {
  auto original_pos = ifs.tellg();
  ifs.seekg(0, std::ifstream::end);
  auto end = ifs.tellg();
  ifs.seekg(0, std::ifstream::beg);
  size_t file_size = end - ifs.tellg();
  ifs.seekg(original_pos, std::ifstream::beg);
  return file_size;
}

static void read_stl(std::ifstream &ifs, std::vector<Triangle> &triangles) {
  size_t file_size = calc_file_size(ifs);
  if (file_size == 0) {
    std::cout << "Empty file" << std::endl;
    return;
  }
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

// https://en.cppreference.com/mwiki/index.php?title=cpp/string/basic_string/getline&oldid=152682#Notes
static void skip_line(std::ifstream &ifs) { ifs.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); }

static void parse_ply_property_definition_ascii(const PLY_Property_Definition &pd, std::ifstream &ifs,
                                                String_Map<PLY_Property> &property_map) {
  std::vector<double> &values = property_map[pd.name].values;
  if (pd.type == PLY_Property_Definition::Type::List) {
    size_t num_values;
    ifs >> num_values;
    for (size_t j = 0; j < num_values; j++) {
      ifs >> values.emplace_back();
    }
  } else if (pd.type == PLY_Property_Definition::Type::Scalar) {
    ifs >> values.emplace_back();
  }
}

static void parse_ply_element_definition_ascii(const PLY_Element_Definition &ed, std::ifstream &ifs,
                                               Parsed_PLY &parsed_ply) {
  std::vector<PLY_Element> &elements = parsed_ply.elements_map[ed.name];
  for (size_t i = 0; i < ed.count; i++) {
    PLY_Element &e = elements.emplace_back();
    for (const PLY_Property_Definition &pd : ed.property_definitions) {
      parse_ply_property_definition_ascii(pd, ifs, e.property_map);
    }
  }
}

static Parsed_PLY read_ply(std::ifstream &ifs) {
  std::string token;
  ifs >> token; // expecting "ply"
  ifs >> token; // expecting "format"
  ifs >> token; // expecting "ascii" or "binary_little_endian" or "binary_big_endian"
  std::string format = token;
  ifs >> token; // expecting "1.0" or version number

  std::vector<PLY_Element_Definition> element_definitions;
  while (ifs.good() && token != "end_header") {
    ifs >> token;
    if (token == "comment") {
      skip_line(ifs);
    } else if (token == "element") {
      PLY_Element_Definition ed;
      ifs >> ed.name;
      ifs >> ed.count;
      element_definitions.push_back(ed);
    } else if (token == "property") {
      PLY_Property_Definition pd{.type = PLY_Property_Definition::Type::Scalar};
      ifs >> token;
      if (token == "list") {
        ifs >> token >> token; // Skip two tokens, list count type and list item type
        pd.type = PLY_Property_Definition::Type::List;
      }
      ifs >> pd.name;
      if (element_definitions.empty()) {
        throw PLY_Expected_Element_Definition_Error();
      }
      element_definitions.back().property_definitions.push_back(pd);
    }
  }
  Parsed_PLY parsed_ply;
  if (format == "ascii") {
    for (const PLY_Element_Definition &ed : element_definitions) {
      parse_ply_element_definition_ascii(ed, ifs, parsed_ply);
    }
  }
  return parsed_ply;
}

static void read_ply(std::ifstream &ifs, std::vector<Triangle> &triangles) {
  Parsed_PLY parsed_ply = read_ply(ifs);
  std::vector<Vec3f> vertices;
  for (const PLY_Element &e : parsed_ply.elements_map.at("vertex")) {
    // TODO: reduce key lookups by storing properties as a map to **vector of vectors**, instead of storing
    // properties in a vector of elements as a map to **vector of scalars**
    vertices.emplace_back(static_cast<float>(e.property_map.at("x").values[0]),
                          static_cast<float>(e.property_map.at("y").values[0]),
                          static_cast<float>(e.property_map.at("z").values[0]));
  }

  for (const PLY_Element &e : parsed_ply.elements_map.at("face")) {
    auto vertex_indices_it = e.property_map.find("vertex_indices");
    if (vertex_indices_it == e.property_map.end()) {
      vertex_indices_it = e.property_map.find("vertex_index");
    }
    if (vertex_indices_it == e.property_map.end()) {
      throw std::out_of_range(R"(Could not find face property "vertex_index" nor "vertex_indices" in PLY file)");
    }
    const std::vector<double> &vertex_indices = vertex_indices_it->second.values;
    if (vertex_indices.size() != 3) {
      throw std::domain_error(std::format("Expected face to have 3 vertices, but found {}", vertex_indices.size()));
    }
    const Vec3f &v0 = vertices[static_cast<size_t>(vertex_indices[0])];
    const Vec3f &v1 = vertices[static_cast<size_t>(vertex_indices[1])];
    const Vec3f &v2 = vertices[static_cast<size_t>(vertex_indices[2])];
    Vec3f normal = (v1 - v0).cross(v2 - v0);
    normal.normalize();
    triangles.push_back({normal, {v0, v1, v2}});
  }
}

// Based on: https://en.cppreference.com/mwiki/index.php?title=cpp/string/byte/tolower&oldid=152869#Notes
static std::string str_tolower(std::string s) {
  std::ranges::transform(s, s.begin(), [](unsigned char c) { return std::tolower(c); });
  return s;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    std::cerr << "Expected arguments: /path/to/mesh/file" << std::endl;
    return 1;
  }

  std::string filepath = argv[1];

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

  // We convert to lower case so that comparing suffix later is case-insensitive
  filepath = str_tolower(filepath);

  std::vector<Triangle> triangles;
  if (filepath.ends_with(".stl")) {
    read_stl(ifs, triangles);
  } else if (filepath.ends_with(".ply")) {
    read_ply(ifs, triangles);
  } else {
    std::cerr << "Unsupported format" << std::endl;
    return 1;
  }

  std::cout << "Number of triangles: " << triangles.size() << std::endl;

  return 0;
}
