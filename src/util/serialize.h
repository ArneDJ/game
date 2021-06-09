#pragma once
#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/json.hpp"
#include "extern/cereal/archives/xml.hpp"

namespace glm
{

template<class Archive> void serialize(Archive &archive, glm::vec3 &v) { archive(v.x, v.y, v.z); }
template<class Archive> void serialize(Archive &archive, glm::quat &q) { archive(q.x, q.y, q.z, q.w); }

};
