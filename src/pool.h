#include "array.h"

template<typename T, int chunk_size = 8192>
struct Pool
{
	Array<T> chunks;
	~Pool();
}
