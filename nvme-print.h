#ifndef NVME_PRINT_H
#define NVME_PRINT_H

#include "nvme.h"
#include <inttypes.h>

enum {
	TERSE = 0x1u,	// only show a few useful fields
	HUMAN = 0x2u,	// interpret some values for humans
	VS    = 0x4u,	// print vendor specific data area
	RAW   = 0x8u,	// just dump raw bytes
};

char *nvme_status_to_string(__u32 status);

#endif
