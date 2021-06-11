#pragma once
#include "extern/cereal/types/unordered_map.hpp"
#include "extern/cereal/types/vector.hpp"
#include "extern/cereal/types/memory.hpp"
#include "extern/cereal/archives/json.hpp"
#include "extern/cereal/archives/xml.hpp"

namespace glm
{

template<class Archive> 
void serialize(Archive &archive, glm::vec2 &v) 
{ 
	archive(
		cereal::make_nvp("x", v.x), 
		cereal::make_nvp("y", v.y) 
	); 
}

template<class Archive> 
void serialize(Archive &archive, glm::vec3 &v) 
{ 
	archive(
		cereal::make_nvp("x", v.x), 
		cereal::make_nvp("y", v.y), 
		cereal::make_nvp("z", v.z)
	); 
}

template<class Archive> 
void serialize(Archive &archive, glm::quat &q) 
{ 
	archive(
		cereal::make_nvp("x", q.x), 
		cereal::make_nvp("y", q.y), 
		cereal::make_nvp("z", q.z), 
		cereal::make_nvp("w", q.w)
	); 
}

};
