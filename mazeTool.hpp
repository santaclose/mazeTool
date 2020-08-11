#include "../src/modelTool/ml.h"
#include <algorithm>
#include <fstream>
#include <string>
#define MODEL_SET
#define BINDINGS
#define PI 3.14159265358979323846264

float entranceDistance = 1.6f;
float hallWidth = 1.0;
float hallHeight = 2.0;
bool closeTips = true;

struct ge;
struct gv
{
	int id;
	std::vector<int> conn;
	vec pos;
};
struct ge
{
	int a;
	int b;
};

void bindings()
{
	if (ImGui::SliderFloat("entranceDistance", &entranceDistance, 0.1f, 10.0f))haveToGenerateModel = true;
	if (ImGui::SliderFloat("hallWidth", &hallWidth, 0.1f, 3.0f))haveToGenerateModel = true;
	if (ImGui::SliderFloat("hallHeight", &hallHeight, 0.1f, 5.0f))haveToGenerateModel = true;
}

inline float cross2d(const vec& a, const vec& b)
{
	return a.x * b.z - a.z * b.x;
}
inline float dot2d(const vec& a, const vec& b)
{
	return a.x * b.x + a.z * b.z;
}

vec intersectLines(const vec& pointA, const vec& pointB, const vec& dirA, const vec& dirB)
{
	if (abs(dot2d(dirA.Normalized(), dirB.Normalized())) > 0.95)
	{
		return (pointA + pointB) * 0.5;
	}

#define p pointA
#define q pointB
#define r dirA
#define s dirB
	vec qp = q - p;
	float rs = cross2d(r, s);

	vec result;
	if (rs != 0.0 && cross2d(qp, r) != 0.0)
	{
		float t = cross2d(qp, s) / rs;
		float u = cross2d(qp, r) / rs;
		result = (p + (r * t) + q + (s * u)) * 0.5;
	}
	else
	{
		result = (p + q) * 0.5;
	}
	return result;
#undef p
#undef q
#undef r
#undef s
}

void readOBJ(std::vector<gv>& vertices, std::vector<ge>& edges)
{
	std::ifstream is("assets/in.obj");
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
			vertices.back().pos.x = std::stof(str.substr(aq, ap - aq));
			vertices.back().pos.y = std::stof(str.substr(bq, bp - bq));
			vertices.back().pos.z = std::stof(str.substr(cq, cp - cq));
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
}

void generateModel()
{
	std::vector<gv> vertices;
	std::vector<ge> edges;

	readOBJ(vertices, edges);

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
				entrancePosA += direction * entranceDistance;
				entrancePosB += direction * entranceDistance;
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
				floorVerts.push_back(ml::vertex(intersectLines(entrancePosB, firstPos, -direction, -firstDir)));
				ceilingVerts.push_back(ml::vertex(intersectLines(entrancePosB, firstPos, -direction, -firstDir) + vec::up * hallHeight));
			}
			else
			{
				vec nextDirection = (-vertices[i].pos + vertices[vertexAngle[index + 1].first].pos);
				nextDirection.y = 0.0;
				nextDirection.Normalize();
				vec nextRight = nextDirection.Cross(vec::up).Normalized();
				vec nextPos = vertices[i].pos + nextDirection * entranceDistance - nextRight * halfHallWidth;
				floorVerts.push_back(ml::vertex(intersectLines(entrancePosB, nextPos, -direction, -nextDirection)));
				ceilingVerts.push_back(ml::vertex(intersectLines(entrancePosB, nextPos, -direction, -nextDirection) + vec::up * hallHeight));
			}

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
		ml::face(&floorVerts[0], floorVerts.size(), true);
		ml::face(&ceilingVerts[0], ceilingVerts.size(), false);

		/*std::cout << std::endl << vertices[i].id << ":\n";
		std::cout << vertices[i].pos.x << "," << vertices[i].pos.z << std::endl;
		for (const std::pair<int, float>& p : vertexAngle)
		{
			std::cout << "  " << vertices[p.first].pos.x << "," << vertices[p.first].pos.z << std::endl;
			std::cout << "  " << p.second * 180.0 / PI << std::endl;
		}*/
	}
	for (int i = 0; i < vertices.size(); i++) // hall faces connecting each intersection
	{
		for (const int conn : vertices[i].conn)
		{
			int adjVertex = edges[conn].a == i ? edges[conn].b : edges[conn].a;
			unsigned int gate1 = modelVertexMatrix[i][adjVertex];
			unsigned int gate2 = modelVertexMatrix[adjVertex][i];
			unsigned int quad[4];
			quad[0] = gate1;
			quad[1] = gate2 + 1;
			quad[2] = gate2 + 3;
			quad[3] = gate1 + 2;
			ml::face(quad, 4);
			quad[0] = gate1;
			quad[1] = gate1 + 1;
			quad[2] = gate2;
			quad[3] = gate2 + 1;
			ml::face(quad, 4);
			quad[0] = gate1 + 2;
			quad[1] = gate1 + 3;
			quad[2] = gate2 + 2;
			quad[3] = gate2 + 3;
			ml::face(quad, 4, true);
		}
	}
}
