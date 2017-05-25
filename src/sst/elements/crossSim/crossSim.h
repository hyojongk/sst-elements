// Copyright 2009-2017 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
// 
// Copyright (c) 2009-2017, Sandia Corporation
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef _CROSSSIM_H
#define _CROSSSIM_H

#include <sst/core/clock.h>
#include <sst/core/component.h>
#include <sst/core/elementinfo.h>
#include <sst/core/sst_types.h>
#include <sst/core/link.h>
#include <sst/core/rng/marsaglia.h>
#include <Python.h>

namespace SST {
namespace crossSim {

class crossSimComponent : public SST::Component 
{
public:
    crossSimComponent(SST::ComponentId_t id, SST::Params& params);
    ~crossSimComponent();

    void setup() { }
    void finish();

private:
    crossSimComponent();  // for serialization only
    crossSimComponent(const crossSimComponent&); // do not implement
    void operator=(const crossSimComponent&); // do not implement

    void handleEvent(SST::Event *ev);
    virtual bool clockTic(SST::Cycle_t);

    static PyObject *c_s_mod; // cross_sim module object
    PyObject *c_s_core;

    int workPerCycle;
    int commFreq;
    int commSize;
    int neighbor;

    SST::RNG::MarsagliaRNG* rng;
    SST::Link* N;
    SST::Link* S;
    SST::Link* E;
    SST::Link* W;

public:
    SST_ELI_REGISTER_COMPONENT(crossSimComponent,"crossSim","crossSimComponent","Model of an neural node using cross sim",COMPONENT_CATEGORY_PROCESSOR)

    SST_ELI_DOCUMENT_VERSION(0,1,0)

    SST_ELI_DOCUMENT_PARAMS(
        {"workPerCycle",  ".", NULL},
        {"commFreq",  ".", NULL},
        {"commSize",  ".", NULL},
    )

    SST_ELI_DOCUMENT_STATISTICS(
        {"pkts_received_net0",           "Total number of packets recived on NIC0", "count", 1},
    )

    SST_ELI_DOCUMENT_PORTS(
        {"Nlink",     "Network Link",  {} },
        {"Slink",     "Network Link",  {} },
        {"Elink",     "Network Link",  {} },
        {"Wlink",     "Network Link",  {} },
    )

    SST_ELI_DOCUMENT_SUBCOMPONENT_SLOTS(
    )
};

} // namespace crossSim
} // namespace SST

#endif /* _CROSSIMC_H */
