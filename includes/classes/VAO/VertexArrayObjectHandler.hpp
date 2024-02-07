#pragma once

#include <classes/VAO/VertexArrayObject.hpp>
#include <unordered_map>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

class VertexArrayObjectHandler {
public:
	VertexArrayObjectHandler();
    ~VertexArrayObjectHandler();

	void Draw();
	void DrawAll(glm::vec3 cameraPosition, glm::vec3 cameraDirection);
	VertexArrayObject *GetVAO(u_int VAO);
	
	u_int AddVAO(VertexArrayObject*);
	void RemoveVAO(u_int VAO);
	void Bind(u_int VAO);
	void Unbind();
private:
	std::unordered_map<u_int, VertexArrayObject*> vaoMap;
	u_int activeVAO = 0;
	u_int indexCount = 0;
};