
namespace physics {

class HeightField {
public:
	HeightField(const UTIL::Image<float> *image, const glm::vec3 &scale);
	HeightField(const UTIL::Image<uint8_t> *image, const glm::vec3 &scale);
public:
	btCollisionObject *object();
	const btCollisionObject *object() const;
private:
	std::unique_ptr<btHeightfieldTerrainShape> m_shape;
	std::unique_ptr<btCollisionObject> m_object;
};

};
