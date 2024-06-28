#include "imgeditor.h"

const struct ksmap *ksmap_find(const struct ksmap *maps, unsigned long key)
{
	const struct ksmap *k = maps;

	for (; k->str; k++)
		if (k->key == key)
			return k;

	return NULL;
}

