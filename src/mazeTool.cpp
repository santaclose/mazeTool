#include <Model.h>
#include <modelTool/ml.h>

#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <string>
#include <imgui.h>

#include "Wireframe.h"
#include "Geometry.h"
#include "Random.h"

#define FROM_FILE_METHOD 0
#define RANDOM_METHOD 1

int method = RANDOM_METHOD;

char fileName[256] = "assets/input/test.obj";
float inputScale = 1.0f;

int seed = 0;
int size = 2;
int height = 2;
float horizontalSeparation = 20.0f;
float verticalSeparation = 5.0f;

float intersectionMargin = 1.6f;
float hallWidth = 1.0;
float hallHeight = 2.0;
bool closeTips = true;

void Model::Bindings(bool& haveToGenerateModel)
{
	const char* methods[] = { "From file", "Random" };
	int prevMethod = method;
	ImGui::Combo("Method", &method, methods, 2);
	if (prevMethod != method)
		haveToGenerateModel = true;

	if (method == FROM_FILE_METHOD)
	{
		if (ImGui::InputText("File name", fileName, 256)) haveToGenerateModel = true;
		BIND(SliderFloat, "Input scale", &inputScale, 0.1f, 10.0f);
	}
	else
	{
		BIND(SliderInt, "Seed", &seed, 0, 100);
		BIND(SliderInt, "Size", &size, 2, 20);
		BIND(SliderInt, "Height", &height, 2, 20);
		BIND(SliderFloat, "Horizontal Separation", &horizontalSeparation, 10.0f, 50.0f);
		BIND(SliderFloat, "Vertical Separation", &verticalSeparation, 1.0f, 20.0f);
	}

	BIND(SliderFloat, "Intersection margin", &intersectionMargin, 0.1f, 10.0f);
	BIND(SliderFloat, "Hall width", &hallWidth, 0.1f, 3.0f);
	BIND(SliderFloat, "Hall height", &hallHeight, 0.1f, 5.0f);
}

void generateRandomly(std::vector<gv>& vertices, std::vector<ge>& edges)
{
	// generate initial graph with random weights
	std::unordered_map<int, float> weights;
	std::vector<gv> igv;
	std::vector<ge> ige;

	Random::setSeed(seed);
	vec cursor = vec::zero;
	vec floorDisplacement = (vec::forward + vec::right).Normalized();
	float halfDiagonalDistance = std::sqrt(horizontalSeparation * horizontalSeparation * 2) * 0.5f;

	for (int h = 0; h < height; h++)
	{
		for (int i = 0; i < size; i++)
		{
			for (int j = 0; j < size; j++)
			{
				igv.emplace_back();
				igv.back().pos = cursor +
					vec::right   * (i * horizontalSeparation) +
					vec::forward * (j * horizontalSeparation);

				// same level edges
				if (i > 0)
				{
					int prevVertexIndex = h * size * size + (i - 1) * size + j;
					int currentVertexIndex = h * size * size + (i) * size + j;
					weights[ige.size()] = Random::value();
					ige.emplace_back();
					ige.back().a = prevVertexIndex;
					ige.back().b = currentVertexIndex;
					igv[prevVertexIndex].conn.push_back(ige.size() - 1);
					igv[currentVertexIndex].conn.push_back(ige.size() - 1);
				}
				if (j > 0)
				{
					int prevVertexIndex = h * size * size + i * size + (j - 1);
					int currentVertexIndex = h * size * size + i * size + (j);
					weights[ige.size()] = Random::value();
					ige.emplace_back();
					ige.back().a = prevVertexIndex;
					ige.back().b = currentVertexIndex;
					igv[prevVertexIndex].conn.push_back(ige.size() - 1);
					igv[currentVertexIndex].conn.push_back(ige.size() - 1);
				}

				if (h == 0)
					continue;

				// edges connecting to the next lower level
				int prevVertexIndex = (h - 1) * size * size + i * size + j;
				int currentVertexIndex = (h) * size * size + i * size + j;
				weights[ige.size()] = Random::value();
				ige.emplace_back();
				ige.back().a = prevVertexIndex;
				ige.back().b = currentVertexIndex;
				igv[prevVertexIndex].conn.push_back(ige.size() - 1);
				igv[currentVertexIndex].conn.push_back(ige.size() - 1);

				int remainder = h % 4;
				int xDir, yDir;
				switch (remainder)
				{
				case 1:
					xDir = yDir = 1;
					break;
				case 2:
					xDir = -1;
					yDir = 1;
					break;
				case 3:
					xDir = yDir = -1;
					break;
				case 0:
					xDir = 1;
					yDir = -1;
					break;
				}

				if (i == (xDir == 1 ? size - 1 : 0) ||
					j == (yDir == 1 ? size - 1 : 0))
					continue;
				int cv = h * size * size + i * size + j;
				int bv = (h - 1) * size * size + (i + xDir) * size + (j + yDir);
				weights[ige.size()] = Random::value();
				ige.emplace_back();
				ige.back().a = bv;
				ige.back().b = cv;
				igv[bv].conn.push_back(ige.size() - 1);
				igv[cv].conn.push_back(ige.size() - 1);
			}
		}
		cursor += vec::up * verticalSeparation + floorDisplacement * halfDiagonalDistance;
		floorDisplacement = vec::up.Cross(floorDisplacement);
	}

	// use prim's algorithm to generate the final wireframe
	std::unordered_map<int, int> def2init;
	std::unordered_map<int, int> init2def;

	// start from a random vertex
	int r = Random::ivalue(igv.size());
	def2init[0] = r;
	init2def[r] = 0;
	vertices.emplace_back();
	vertices.back().pos = igv[r].pos;

	// grow the tree through the edge with the lowest weight
	while (vertices.size() != igv.size())
	{
		float minWeight = 2.0f;
		int connectionVertex = -1;
		int newVertex = -1;
		for (const std::pair<int, int>& p : def2init)
		{
			for (int c : igv[p.second].conn)
			{
				// if edge is already in our selection
				if (init2def.find(ige[c].a) != init2def.end() &&
					init2def.find(ige[c].b) != init2def.end())
					continue;

				if (weights[c] < minWeight)
				{
					minWeight = weights[c];
					connectionVertex = init2def.find(ige[c].b) != init2def.end() ? ige[c].b : ige[c].a;
					newVertex =		   init2def.find(ige[c].a) != init2def.end() ? ige[c].b : ige[c].a;
				}
			}
		}

		def2init[vertices.size()] = newVertex;
		init2def[newVertex] = vertices.size();
		vertices.emplace_back();
		vertices.back().pos = igv[newVertex].pos;
		vertices.back().conn.push_back(edges.size());
		vertices[init2def[connectionVertex]].conn.push_back(edges.size());
		edges.emplace_back();
		edges.back().a = vertices.size() - 1;
		edges.back().b = init2def[connectionVertex];
	}
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

	if (method == FROM_FILE_METHOD)
	{
		if (!readOBJ(vertices, edges))
			return;
	}
	else
	{
		generateRandomly(vertices, edges);
	}

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
