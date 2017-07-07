// Copyright 2013-2017 Sandia Corporation. Under the terms
// of Contract DE-NA0003525 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013-2017, Sandia Corporation
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
#include "memNICSub.h"

#include <sst/core/simulation.h>

using namespace SST;
using namespace SST::MemHierarchy;
using namespace SST::Interfaces;

/* Constructor */
MemNICSub::MemNICSub(Component * parent, Params &params) : MemLinkBase(parent, params) {
    
    // Get network parameters and create link control
    std::string linkName = params.find<std::string>("port", "");
    if (linkName == "") 
        dbg.fatal(CALL_INFO, -1, "Param not specified(%s): port - the name of the port that the MemNIC is attached to. This should be set internally by components creating the memNIC.\n",
                getName().c_str());

    // Error checking for much of this is done by the link control
    bool found;
    std::string linkBandwidth = params.find<std::string>("network_bw", "80GiB/s");
    int num_vcs = 1; // MemNIC does not use VCs
    std::string linkInbufSize = params.find<std::string>("network_input_buffer_size", "1KiB");
    std::string linkOutbufSize = params.find<std::string>("network_output_buffer_size", "1KiB");
    info.addr = params.find<uint64_t>("network_address", 0, found);
    if (!found) {
        dbg.fatal(CALL_INFO, -1, "Param not specified(%s): network_address - network address for this component.\n", getName().c_str());
    }

    link_control = (SimpleNetwork*)parent->loadSubComponent("merlin.linkcontrol", parent, params); // But link control doesn't use params so manually initialize
    link_control->initialize(linkName, UnitAlgebra(linkBandwidth), num_vcs, UnitAlgebra(linkInbufSize), UnitAlgebra(linkOutbufSize));

    // Get source/destination parameters
    // Each NIC has a class ID and talks to those with IDs in sources and destinations
    // If no source/destination provided, source = class ID - 1, destination = class ID + 1
    info.id = params.find<uint32_t>("class", 0, found);
    if (!found) {
        dbg.fatal(CALL_INFO, -1, "Param not specified(%s): class - class ID (or hierarchy level) for this NIC's component.\n", getName().c_str());
    }
    std::stringstream sources, destinations;
    sources.str(params.find<std::string>("sources", ""));
    destinations.str(params.find<std::string>("destinations", ""));
    uint32_t id;
    while (sources >> id) {
        sourceIDs.insert(id);
        while (sources.peek() == ',' || sources.peek() == ' ')
            sources.ignore();
    }

    if (sourceIDs.empty())
        sourceIDs.insert(info.id - 1);

    while (destinations >> id) {
        destIDs.insert(id);
        while (destinations.peek() == ',' || destinations.peek() == ' ')
            destinations.ignore();
    }
    if (destIDs.empty())
        destIDs.insert(info.id + 1);

    // Packet size
    UnitAlgebra packetSize = UnitAlgebra(params.find<std::string>("min_packet_size", "8B"));
    if (!packetSize.hasUnits("B"))
        dbg.fatal(CALL_INFO, -1, "Invalid param(%s): min_packet_size - must have units of bytes (B). SI units OK. You specified '%s'\n.",
                getName().c_str(), packetSize.toString().c_str());

    packetHeaderBytes = packetSize.getRoundedValue();

    // Set link control to call recvNotify on event receive
    link_control->setNotifyOnReceive(new SimpleNetwork::Handler<MemNICSub>(this, &MemNICSub::recvNotify));

    dbg.debug(_L10_, "%s memNIC info is: Name: %s, addr: %" PRIu64 ", id: %" PRIu32 "\n",
            getName().c_str(), info.name.c_str(), info.addr, info.id);

}

void MemNICSub::init(unsigned int phase) {
    link_control->init(phase);  // This MUST be called before anything else

    // On first init round, send our region out to all others
    if (phase == 1) {
        InitMemRtrEvent *ev = new InitMemRtrEvent(info);
        SimpleNetwork::Request * req = new SimpleNetwork::Request();
        req->dest = SimpleNetwork::INIT_BROADCAST_ADDR;
        req->src = info.addr;
        req->givePayload(ev);
        link_control->sendInitData(req);
    }

    if (phase > 1) {
        while (!initSendQueue.empty()) {
            link_control->sendInitData(initSendQueue.front());
            initSendQueue.pop();
        
        }
    }

    // Expect different kinds of init events
    // 1. MemNIC - record these as needed and do not inform parent
    // 2. MemEventBase - only notify parent if sender is a src or dst for us
    // We should know since network is in order and NIC does its init before the 
    // parents do
    while (SimpleNetwork::Request *req = link_control->recvInitData()) {
        Event * payload = req->takePayload();
        InitMemRtrEvent * imre = dynamic_cast<InitMemRtrEvent*>(payload);
        if (imre) {
            // Record name->address map for all other endpoints
            networkAddressMap.insert(std::make_pair(imre->info.name, imre->info.addr));
            
            dbg.debug(_L10_, "%s (memNIC) received imre. Name: %s, Addr: %" PRIu64 ", ID: %" PRIu32 ", start: %" PRIu64 ", end: %" PRIu64 ", size: %" PRIu64 ", step: %" PRIu64 "\n",
                    getName().c_str(), imre->info.name.c_str(), imre->info.addr, imre->info.id, imre->info.region.start, imre->info.region.end, imre->info.region.interleaveSize, imre->info.region.interleaveStep);

            if (sourceIDs.find(imre->info.id) != sourceIDs.end()) {
                dbg.debug(_L10_, "\tAdding to sourceEndpointInfo\n");
                sourceEndpointInfo.insert(std::make_pair(imre->info.name, imre->info));
            } else if (destIDs.find(imre->info.id) != destIDs.end()) {
                dbg.debug(_L10_, "\tAdding to destEndpointInfo\n");
                destEndpointInfo.insert(std::make_pair(imre->info.name, imre->info));
            }
            delete imre;
        } else {
            MemRtrEvent * mre = static_cast<MemRtrEvent*>(payload);
            MemEventInit *ev = static_cast<MemEventInit*>(mre->event);
            dbg.debug(_L10_, "%s (memNIC) received mre during init. %s\n", getName().c_str(), mre->event->getVerboseString().c_str());
            
            if (ev->getDst() == getName() && ev->getInitCmd() == MemEventInit::InitCommand::Region) {
                MemEventInitRegion * rEv = static_cast<MemEventInitRegion*>(ev);
                if (rEv->getSetRegion() && acceptRegion) {
                    info.region = rEv->getRegion();
                    dbg.debug(_L10_, "\tUpdating local region\n");
                }
                delete ev;
                delete mre;
            } else if (sourceEndpointInfo.find(mre->event->getSrc()) != sourceEndpointInfo.end() 
                        || destEndpointInfo.find(mre->event->getSrc()) != destEndpointInfo.end()) {
                dbg.debug(_L10_, "\tInserting in initQueue\n");
                initQueue.push(mre);
            }
        }
        delete req;
    }
}

void MemNICSub::sendInitData(MemEventInit * event) {
    MemRtrEvent *mre = new MemRtrEvent(event);
    SimpleNetwork::Request* req = new SimpleNetwork::Request();
    req->dest = SimpleNetwork::INIT_BROADCAST_ADDR;
    req->givePayload(mre);
    initSendQueue.push(req);
}

MemEventInit * MemNICSub::recvInitData() {
    if (initQueue.size()) {
        MemRtrEvent * mre = initQueue.front();
        initQueue.pop();
        MemEventInit *ev = static_cast<MemEventInit*>(mre->event);
        delete mre;
        return ev;
    }
    return nullptr;
}

/* 
 * Called by parent on a clock 
 * Returns whether anything sent this cycle
 */
bool MemNICSub::clock() {
    if (sendQueue.empty()) return true;
    while (!sendQueue.empty()) {
        SimpleNetwork::Request *head = sendQueue.front();
/* Debug info - record before we attempt send so that if send destroys anything we have it */
        MemEventBase * ev = (static_cast<MemRtrEvent*>(head->inspectPayload()))->event;
        std::string debugEvStr;
        uint64_t dst = head->dest;
        bool doDebug = false;
        if (ev) { 
            debugEvStr = ev->getBriefString();
            if (!DEBUG_ALL) doDebug = ev->doDebug(DEBUG_ADDR);
        }
        if (link_control->spaceToSend(0, head->size_in_bits) && link_control->send(head, 0)) {
#ifdef __SST_DEBUG_OUTPUT__
            if (!debugEvStr.empty() && (DEBUG_ALL || doDebug)) {
                dbg.debug(_L9_, "%s (memNIC), Sending message %s to dst addr %" PRIu64 "\n",
                        getName().c_str(), debugEvStr.c_str(), dst);
            }
#endif
            sendQueue.pop();
        } else {
            break;
        }
    }
    return false;
}

/* 
 * Event handler called by link control on event receive 
 * Return whether event can be received
 */
bool MemNICSub::recvNotify(int) {
    MemEventBase * me = recv();
    if (me) {
#ifdef __SST_DEBUG_OUTPUT__
        if (DEBUG_ALL || me->doDebug(DEBUG_ADDR)) 
            dbg.debug(_L9_, "%s, memNIC recv: src: %s. cmd: %s\n", 
                    getName().c_str(), me->getSrc().c_str(), CommandString[(int)me->getCmd()]);
#endif
        // Call parent's handler
        (*recvHandler)(me);
    }
    return true;
}


/* Send event to memNIC */
void MemNICSub::send(MemEventBase *ev) {
    SimpleNetwork::Request *req = new SimpleNetwork::Request();
    MemRtrEvent * mre = new MemRtrEvent(ev);
    req->src = info.addr;
    req->dest = lookupNetworkAddress(ev->getDst());
    req->size_in_bits = getSizeInBits(ev);
    req->vn = 0;
#ifdef __SST_DEBUG_OUTPUT__
    if (DEBUG_ALL || ev->doDebug(DEBUG_ADDR))
        dbg.debug(_L9_, "%s, memNIC adding to send queue: dst: %s, bits: %zu, cmd: %s\n",
                getName().c_str(), ev->getDst().c_str(), req->size_in_bits, CommandString[(int)ev->getCmd()]);
#endif
    req->givePayload(mre);
    sendQueue.push(req);
}


/** Helper functions **/


/* Translate destination string to network address */
uint64_t MemNICSub::lookupNetworkAddress(const std::string & dst) const {
    std::unordered_map<std::string,uint64_t>::const_iterator it = networkAddressMap.find(dst);
    if (it == networkAddressMap.end()) {
        dbg.fatal(CALL_INFO, -1, "%s (MemNIC), Network address for destination '%s' not found in networkAddressMap.\n", getName().c_str(), dst.c_str());
    }
    return it->second;
}


/* Calculate size in bits of an event */
size_t MemNICSub::getSizeInBits(MemEventBase *ev) {
    return 8 * (packetHeaderBytes + ev->getPayloadSize());
}


/* 
 * Polling-based receive function
 */
MemEventBase* MemNICSub::recv() {
    SimpleNetwork::Request *req = link_control->recv(0);
    if (req != nullptr) {
        MemRtrEvent *mre = static_cast<MemRtrEvent*>(req->takePayload());
        delete req;
        
        if (mre->hasClientData()) {
            MemEventBase * ev = static_cast<MemEventBase*>(mre->event);
            ev->setDeliveryLink(mre->getLinkId(), NULL);
            delete mre;
            return ev;
        } else { /* InitMemRtrEvent - someone updated their info */
            InitMemRtrEvent *imre = static_cast<InitMemRtrEvent*>(mre);
            if (networkAddressMap.find(imre->info.name) == networkAddressMap.end()) {
                dbg.fatal(CALL_INFO, -1, "%s (MemNIC), received information about previously unknown endpoint. This case is not handled. Endpoint name: %s\n",
                        getName().c_str(), imre->info.name.c_str());
            }
            if (sourceIDs.find(imre->info.id) != sourceIDs.end()) {
                sourceEndpointInfo.insert(std::make_pair(imre->info.name, imre->info));
            } else if (destIDs.find(imre->info.id) != destIDs.end()) {
                destEndpointInfo.insert(std::make_pair(imre->info.name, imre->info));
            }
            delete imre;
        }
    }
    return nullptr;
}
