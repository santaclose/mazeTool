#pragma once
#include <vector>
#include <modelTool/vl.h>

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