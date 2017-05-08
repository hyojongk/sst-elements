// Copyright 2017 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2017, Sandia Corporation
// All rights reserved.
//
// Portions are copyright of other developers:
// See the file CONTRIBUTORS.TXT in the top level directory
// the distribution for more information.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#include <sst_config.h>
#include "crossSim.h"

#include <sst/core/element.h>
#include <assert.h>
#include "sst/core/event.h"

#include <Python.h>

#if PY_MAJOR_VERSION != 3
#error NEEDS PYTHON 3
#endif

using namespace SST;
using namespace SST::crossSim;

crossSimComponent::crossSimComponent(ComponentId_t id, Params& params) :
  Component(id) 
{
    ///
    /*program = Py_DecodeLocale("SST Test", NULL);
    if (program == NULL) {
        fprintf(stderr, "Fatal error: cannot decode argv[0]\n");
        exit(1);
    }
    Py_SetProgramName(program);   optional but recommended */
    Py_Initialize();
    ///



    bool found;
    
    rng = new SST::RNG::MarsagliaRNG(11, 272727);
    
    // get parameters
    workPerCycle = params.find<int64_t>("workPerCycle", 0, found);
    if (!found) {
        Simulation::getSimulation()->getSimulationOutput().fatal(CALL_INFO, -1,"couldn't find work per cycle\n");
    }
    
    commFreq = params.find<int64_t>("commFreq", 0, found);
    if (!found) {
        Simulation::getSimulation()->getSimulationOutput().fatal(CALL_INFO, -1,"couldn't find communication frequency\n");
    }
    
    commSize = params.find<int64_t>("commSize", 16);
    
    // init randomness
    srand(1);
    neighbor = rng->generateNextInt32() % 4;
    
    // tell the simulator not to end without us
    registerAsPrimaryComponent();
    primaryComponentDoNotEndSim();
    
    // configure out links
    N = configureLink("Nlink", new Event::Handler<crossSimComponent>(this,
                               &crossSimComponent::handleEvent));
    S = configureLink("Slink", new Event::Handler<crossSimComponent>(this,
                               &crossSimComponent::handleEvent));
    E = configureLink("Elink", new Event::Handler<crossSimComponent>(this,
                               &crossSimComponent::handleEvent));
    W = configureLink("Wlink", new Event::Handler<crossSimComponent>(this,
                               &crossSimComponent::handleEvent));
    
    assert(N);
    assert(S);
    assert(E);
    assert(W);
    
    //set our clock
    registerClock("1GHz", new Clock::Handler<crossSimComponent>(this, 
                  &crossSimComponent::clockTic));
}

crossSimComponent::~crossSimComponent() 
{
    //PyMem_RawFree(program);

	delete rng;
}

crossSimComponent::crossSimComponent() : Component(-1)
{
    // for serialization only
}

// incoming events are scanned and deleted
void crossSimComponent::handleEvent(Event *ev) 
{
    printf("recv\n");
    Event *event = ev;
    if (event) {
        delete event;
    } else {
        printf("Error! Bad Event Type!\n");
    }
}

// each clock tick we do 'workPerCycle' iterations of a simple loop.
// We have a 1/commFreq chance of sending an event of size commSize to
// one of our neighbors.
bool crossSimComponent::clockTic( Cycle_t ) 
{

    ///

    PyRun_SimpleString("import sys\n\n"
                       "print(sys.version)\n");
    Py_Finalize();
    ///


    volatile int v = 0;
    for (int i = 0; i < workPerCycle; ++i) {
        v++;
    }

    // communicate?
    if ((rng->generateNextInt32() % commFreq) == 0) {
        // yes, communicate
        // create event
        Event *e;// = new Event();
        // fill payload with commSize bytes
        // find target
        neighbor = neighbor+1;
        neighbor = neighbor % 4;
        // send
        switch (neighbor) {
        case 0:
            N->send(e); 
            break;
        case 1:
            S->send(e);  
            break;
        case 2:
            E->send(e);  
            break;
        case 3:
            W->send(e);  
            break;
        default:
            printf("bad neighbor\n");
        }
        //printf("sent\n");
    }
    
    // return false so we keep going
    return false;
}

// Element Libarary / Serialization stuff
    


/*
  Needed temporarily because the Factory doesn't know if this is an
  SST library without it.

extern "C" {
    ElementLibraryInfo crossSim_eli = {
        "CrossSim",
        "Analog Cross Bar Simulator",
        NULL,
        NULL,   // Events
        NULL,   // Introspectors
        NULL,
        NULL,
        NULL, // partitioners,
        NULL,  // Python Module Generator
        NULL // generators,
    };
}
 */

