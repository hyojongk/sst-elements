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
#include <iostream>
#include "crossSim.h"

#include <sst/core/element.h>
#include <assert.h>
#include "sst/core/event.h"

#include <Python.h>
#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#include <numpyconfig.h>
#include <arrayobject.h>
#include <assert.h>

#if PY_MAJOR_VERSION != 3
#error NEEDS PYTHON 3
#endif

using namespace SST;
using namespace SST::crossSim;

PyObject *crossSimComponent::c_s_mod = NULL;

crossSimComponent::crossSimComponent(ComponentId_t id, Params& params) :
  Component(id) 
{
    // python init-ing
    Py_Initialize();

    if( !Py_IsInitialized() ) {
        printf("Unable to initialize Python interpreter.");
    }
    

    /*PyRun_SimpleString("import math\n"
                       "from math import \n"
                       "print(math.__file__)\n"
                       "print(dir(math))\n"
                       "import cross_sim\n\n"
                       );//"print(sys.version)\n"); */
    
    // load cross_sim
    if (c_s_mod == NULL) {
        c_s_mod = PyImport_ImportModule("cross_sim");   
        assert(c_s_mod != NULL);
    }

    PyObject *paramFunc = PyObject_GetAttrString(c_s_mod, "Parameters");
    assert(paramFunc != NULL);
    assert(PyCallable_Check(paramFunc));

    // make params object
    PyObject *c_s_params = PyEval_CallObject(paramFunc, NULL);
    assert(c_s_params != NULL);

    // make core: get function
    PyObject *makeCoreFunc = PyObject_GetAttrString(c_s_mod, "MakeCore");
    assert(makeCoreFunc != NULL);
    assert(PyCallable_Check(makeCoreFunc));

    // build arguments
    PyObject *args = Py_BuildValue("()");
    assert(args);
    PyObject *kwargs = Py_BuildValue("{s:O}", "params", c_s_params);
    assert(kwargs);
    
    // construct
    PyObject *c_s_core = PyObject_Call(makeCoreFunc, args, kwargs);
    assert(c_s_core != NULL);
    Py_INCREF(c_s_core);

    // initialize matrix
    PyObject *mat_args = Py_BuildValue("(((dd)(dd)))", 1.0, 2.0, 2.0, 3.0); 
    assert(mat_args);
    PyObject *makeMatFunc = PyObject_GetAttrString(c_s_core, "set_matrix");
    assert(makeMatFunc != NULL);
    assert(PyCallable_Check(makeMatFunc));

    PyObject *setRet = PyObject_CallObject(makeMatFunc, mat_args);
    assert(setRet);
    assert(setRet == Py_None);
    Py_DECREF(setRet);

    // mvm
    PyObject *MVMFunc = PyObject_GetAttrString(c_s_core, "run_xbar_mvm");
    assert(MVMFunc);
    assert(PyCallable_Check(MVMFunc));
    PyObject *vec_args = Py_BuildValue("((dd))", 1.0, 2.0); 
    assert(vec_args);    
    setRet = PyObject_CallObject(MVMFunc, vec_args);
    assert(setRet);

    // print
    PyArrayObject *np_ret = reinterpret_cast<PyArrayObject*>(setRet);
    assert(np_ret);
    printf("NDIM %d\n", PyArray_NDIM(np_ret));
    PyArray_Descr *desc = PyArray_DTYPE(np_ret);
    printf("type %c\n", desc->type);
    printf("kind %c\n", desc->kind);
    npy_intp *sh = PyArray_SHAPE(np_ret);
    printf("shpe %ld\n", sh[0]);
    for (int i = 0; i < sh[0]; ++i) {
        float* dptr = (float*)PyArray_GETPTR1(np_ret, i);
        std::cout << i << ": " << *dptr << std::endl;
    }
    assert(0);


    // clean up
    Py_DECREF(c_s_params);
    Py_DECREF(args);
    Py_DECREF(mat_args);
    Py_DECREF(kwargs);

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

    printf("a\n");

    PyRun_SimpleString("import sys\n\n"
                       "print(sys.version)\n");
    printf("b\n");
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

void crossSimComponent::finish() {
    Py_Finalize();
    printf("fin\n");
}    

// Element Libarary / Serialization stuff
    


/*
  Needed temporarily because the Factory doesn't know if this is an
  SST library without it.
*/

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


