#include <Model.h>
#include <modelTool/ml.h>

#include <algorithm>
#include <fstream>
#include <string>
#include <imgui.h>

#include "Wireframe.h"
#include "Geometry.h"

char fileName[256] = "assets/input/test.obj";
float inputScale = 1.0f;
float intersectionMargin = 1.6f;
float hallWidth = 1.0;
float hallHeight = 2.0;
bool closeTips = true;

void Model::Bindings(bool& haveToGenerateModel)
{
	if (ImGui::InputText("File name", fileName, 256)) haveToGenerateModel = true;
	BIND(SliderFloat, "Input scale", &inputScale, 0.1f, 10.0f);
	BIND(SliderFloat, "Intersection margin", &intersectionMargin, 0.1f, 10.0f);
	BIND(SliderFloat, "Hall width", &hallWidth, 0.1f, 3.0f);
	BIND(SliderFloat, "Hall height", &hallHeight, 0.1f, 5.0f);
}


bool readOBJ(std::vector<gv>& vertices, std::vector<ge>& edges)
{
	std::ifstream is(fileName);
	if (is.fail())
		return false;

	std::string str;
	while (std::getline(is, str))
	{
		if (str.length() == 0)
			continue;

		if (str[0] == 'v')
		{
			int aq, ap, bq, bp, cq, cp;
			aq = 2;
			ap = aq;
			while (str[ap] != ' ')ap++;
			bq = ap + 1;
			bp = bq;
			while (str[bp] != ' ')bp++;
			cq = bp + 1;
			cp = cq;
			while (cp < str.length() && str[cp] != ' ' && str[cp] != '\n')cp++;
			vertices.emplace_back();
			vertices.back().pos.x = std::stof(str.substr(aq, ap - aq)) * inputScale;
			vertices.back().pos.y = std::stof(str.substr(bq, bp - bq)) * inputScale;
			vertices.back().pos.z = std::stof(str.substr(cq, cp - cq)) * inputScale;
		}
		else if (str[0] == 'l')
		{
			int aq, ap, bq, bp;
			aq = 2;
			ap = aq;
			while (str[ap] != ' ')ap++;
			bq = ap + 1;
			bp = bq;
			while (bp < str.length() && str[bp] != ' ' && str[bp] != '\n')bp++;
			edges.emplace_back();
			edges.back().a = std::stoi(str.substr(aq, ap - aq)) - 1;
			edges.back().b = std::stoi(str.substr(bq, bp - bq)) - 1;
			vertices[edges.back().a].conn.push_back(edges.size() - 1);
			vertices[edges.back().b].conn.push_back(edges.size() - 1);
		}
	}
	return true;
}

void Model::GenerateModel()
{
	std::vector<gv> vertices;
	std::vector<ge> edges;

	if (!readOBJ(vertices, edges))
		return;

	unsigned int** modelVertexMatrix = new unsigned int*[vertices.size()];
	for (int i = 0; i < vertices.size(); i++)
		modelVertexMatrix[i] = new unsigned int[vertices.size()];

	for (int i = 0; i < vertices.size(); i++) // geometry for each intersection
	{
		std::vector<std::pair<int, float>> vertexAngle;
		for (const int conn : vertices[i].conn)
		{
			int adjVertex = edges[conn].a == i ? edges[conn].b : edges[conn].a;
			vertexAngle.emplace_back();
			vertexAngle.back().first = adjVertex;
			vertexAngle.back().second = atan2((vertices[adjVertex].pos.z - vertices[i].pos.z), (vertices[adjVertex].pos.x - vertices[i].pos.x));
		}
		// sort pair vector
		std::sort(vertexAngle.begin(), vertexAngle.end(), [](auto& left, auto& right) {
			return left.second < right.second;
			});

		int index = 0;
		vec firstPos;
		vec firstDir;
		std::vector<unsigned int> ceilingVerts;
		std::vector<unsigned int> floorVerts;
		for (const std::pair<int, float>& p : vertexAngle)
		{
			vec direction = (-vertices[i].pos + vertices[p.first].pos);
			direction.y = 0.0;
			direction.Normalize();
			vec right = direction.Cross(vec::up).Normalized();

			float halfHallWidth = hallWidth * 0.5f;

			vec entrancePosA = vertices[i].pos - right * halfHallWidth;
			vec entrancePosB = vertices[i].pos + right * halfHallWidth;
			if (vertexAngle.size() != 1)
			{
				entrancePosA += direction * intersectionMargin;
				entrancePosB += direction * intersectionMargin;
			}

			if (index == 0)
			{
				firstPos = entrancePosA;
				firstDir = direction;
			}

			floorVerts.push_back(ml::vertex(entrancePosA));
			modelVertexMatrix[i][p.first] = floorVerts[floorVerts.size() - 1];
			floorVerts.push_back(ml::vertex(entrancePosB));
			ceilingVerts.push_back(ml::vertex(entrancePosA + vec::up * hallHeight));
			ceilingVerts.push_back(ml::vertex(entrancePosB + vec::up * hallHeight));
			if (index == vertexAngle.size() - 1)
			{
				floorVerts.push_back(ml::vertex(Geometry::intersectLines(entrancePosB, firstPos, -direction, -firstDir)));
				ceilingVerts.push_back(ml::vertex(Geometry::intersectLines(entrancePosB, firstPos, -direction, -firstDir) + vec::up * hallHeight));
			}
			else
			{
				vec nextDirection = (-vertices[i].pos + vertices[vertexAngle[index + 1].first].pos);
				nextDirection.y = 0.0;
				nextDirection.Normalize();
				vec nextRight = nextDirection.Cross(vec::up).Normalized();
				vec nextPos = vertices[i].pos + nextDirection * intersectionMargin - nextRight * halfHallWidth;
				floorVerts.push_back(ml::vertex(Geometry::intersectLines(entrancePosB, nextPos, -direction, -nextDirection)));
				ceilingVerts.push_back(ml::vertex(Geometry::intersectLines(entrancePosB, nextPos, -direction, -nextDirection) + vec::up * hallHeight));
			}

			ml::setMaterial("wall");
			unsigned int quad[4];
			quad[0] = floorVerts.back();
			quad[1] = ceilingVerts.back();
			quad[2] = ceilingVerts[ceilingVerts.size() - 2];
			quad[3] = floorVerts[floorVerts.size() - 2];
			ml::face(quad, 4);
			if (index > 0)
			{
				quad[0] = floorVerts[floorVerts.size() - 4];
				quad[1] = ceilingVerts[ceilingVerts.size() - 4];
				quad[2] = ceilingVerts[ceilingVerts.size() - 3];
				quad[3] = floorVerts[floorVerts.size() - 3];
				ml::face(quad, 4, true);
			}
			if (index == vertexAngle.size() - 1)
			{
				quad[0] = floorVerts[floorVerts.size() - 1];
				quad[1] = ceilingVerts[ceilingVerts.size() - 1];
				quad[2] = ceilingVerts[0];
				quad[3] = floorVerts[0];
				ml::face(quad, 4, true);
			}

			index++;
		}
		ml::concaveFace(&ceilingVerts[0], ceilingVerts.size(), false);
		ml::setMaterial("floor");
		ml::concaveFace(&floorVerts[0], floorVerts.size(), true);
	}

	for (int i = 0; i < vertices.size(); i++) // hall faces connecting each intersection
	{
		for (const int conn : vertices[i].conn)
		{
			ml::setMaterial("wall");
			int adjVertex = edges[conn].a == i ? edges[conn].b : edges[conn].a;
			unsigned int gate1 = modelVertexMatrix[i][adjVertex];
			unsigned int gate2 = modelVertexMatrix[adjVertex][i];
			unsigned int quad[4];
			quad[0] = gate1;
			quad[1] = gate2 + 1;
			quad[2] = gate2 + 3;
			quad[3] = gate1 + 2;
			ml::face(quad, 4);
			quad[0] = gate1 + 2;
			quad[1] = gate1 + 3;
			quad[2] = gate2 + 2;
			quad[3] = gate2 + 3;
			ml::face(quad, 4, true);
			ml::setMaterial("floor");
			quad[0] = gate1;
			quad[1] = gate1 + 1;
			quad[2] = gate2;
			quad[3] = gate2 + 1;
			ml::face(quad, 4);
		}
	}
}
