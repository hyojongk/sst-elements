// Copyright 2009-2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2013, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.


#include "sst/core/serialization/element.h"
#include "sst/core/element.h"
#include "sst/core/component.h"

#include "cache.h"
#include "bus.h"
#include "trivialCPU.h"
#include "memController.h"

using namespace SST;
using namespace SST::MemHierarchy;

static Component*
create_Cache(SST::ComponentId_t id,
		SST::Component::Params_t& params)
{
	return new Cache( id, params );
}


static Component*
create_Bus(SST::ComponentId_t id,
		SST::Component::Params_t& params)
{
	return new Bus( id, params );
}


static Component*
create_trivialCPU(SST::ComponentId_t id,
		SST::Component::Params_t& params)
{
	return new trivialCPU( id, params );
}


static Component*
create_MemController(SST::ComponentId_t id,
		SST::Component::Params_t& params)
{
	return new MemController( id, params );
}

static const ElementInfoParam cache_params[] = {
    {"num_ways",        "Associativity of the cache."},
    {"num_rows",        "How many cache rows."},
    {"blocksize",       "Size of a cache block in bytes."},
    {"num_upstream",    "How many upstream ports there are. Typically 1 or 0."},
    {"next_level",      "Name of the next level cache"},
    {"access_time",     "Time taken to lookup data in the cache."},
    {NULL, NULL}
};

static const ElementInfoParam bus_params[] = {
    {"numPorts",    "Number of Ports on the bus."},
    {"busDelay",    "Delay time for the bus."},
    {NULL, NULL}
};

static const ElementInfoParam memctrl_params[] = {
    {"mem_size",        "Size of physical memory in MB"},
    {"rangeStart",      "Address Range where physical memory begins"},
    {"interleaveSize",  "Size of interleaved pages in KB."},
    {"interleaveStep",  "Distance between sucessive interleaved pages on this controller in KB."},
    {"memory_file",     "Optional backing-store file to pre-load memory, or store resulting state"},
    {"clock",           "Clock frequency of controller"},
    {"use_dramsim",     "0 to not use DRAMSim, 1 to use DRAMSim"},
    {"device_ini",      "Name of DRAMSim Device config file"},
    {"system_ini",      "Name of DRAMSim Device system file"},
    {"access_time",     "When not using DRAMSim, latency of memory operation."},
    {"request_width",   "Size of a DRAM request in bytes.  Should be a power of 2 - default 64"},
    {"direct_link_latency",   "Latency when using the 'direct_link', rather than 'snoop_link'"},
    {NULL, NULL}
};

static const ElementInfoParam cpu_params[] = {
    {"workPerCycle",    "How much work to do per cycle."},
    {"commFreq",        "How often to do a memory operation."},
    {"memSize",         "Size of physical memory."},
    {"do_write",        "Enable writes to memory (versus just reads)."},
    {"num_loadstore",   "Stop after this many reads and writes."},
    {NULL, NULL}
};


static const ElementInfoComponent components[] = {
	{ "Cache",
		"Cache Component",
		NULL,
		create_Cache,
        cache_params
	},
	{ "Bus",
		"Mem Hierarchy Bus Component",
		NULL,
		create_Bus,
        bus_params
	},
	{"MemController",
		"Memory Controller Component",
		NULL,
		create_MemController,
        memctrl_params
	},
	{"trivialCPU",
		"Simple Demo CPU for testing",
		NULL,
		create_trivialCPU,
        cpu_params
	},
	{ NULL, NULL, NULL, NULL }
};


extern "C" {
	ElementLibraryInfo memHierarchy_eli = {
		"memHierarchy",
		"Simple Memory Hierarchy",
		components,
	};
}
