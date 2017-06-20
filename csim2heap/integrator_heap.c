#include <string.h>
#include "integrator_heap.h"

struct StrictlyProperBlock * integrator_new(size_t const numBlocks)
{
	struct StrictlyProperBlock stackb = integrator(numBlocks);
	struct StrictlyProperBlock * heapb = malloc(sizeof(struct StrictlyProperBlock));
	if (heapb)
		memcpy(heapb, &stackb, sizeof(struct StrictlyProperBlock));
	return heapb;
}

void integrator_free(struct StrictlyProperBlock * b)
{
	free(b);
}