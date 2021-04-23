#include <glm/glm.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "../extern/poisson/PoissonGenerator.h"

#include "poisson.h"
	
void Poisson::generate(int32_t seed, uint32_t max)
{
	points.clear();

	PoissonGenerator::DefaultPRNG PRNG(seed);
	const auto positions = PoissonGenerator::generatePoissonPoints(max, PRNG, false);

	for (const auto &point : positions) {
		points.push_back(glm::vec2(point.x, point.y));
	}
}
