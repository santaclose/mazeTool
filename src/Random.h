#pragma once
#include <cstdlib>

namespace Random {

    inline void setSeed(unsigned int seed)
    {
        srand(seed);
    }
	inline float value()
    {
        // returns a random real in [0,1).
        return rand() / (RAND_MAX + 1.0);
    }
    inline int ivalue(int max)
    {
        // returns a random int in [0, max - 1]
        return rand() % max;
    }
}