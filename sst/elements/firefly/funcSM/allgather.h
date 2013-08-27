// Copyright 2013 Sandia Corporation. Under the terms
// of Contract DE-AC04-94AL85000 with Sandia Corporation, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2013, Sandia Corporation
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.

#ifndef COMPONENTS_FIREFLY_FUNCSM_ALLGATHER_H
#define COMPONENTS_FIREFLY_FUNCSM_ALLGATHER_H

#include "funcSM/api.h"
#include "funcSM/event.h"
#include "info.h"
#include "ctrlMsg.h"

namespace SST {
namespace Firefly {

class AllgatherFuncSM :  public FunctionSMInterface
{
    enum { WaitSendStart, WaitRecvStart, WaitSendData, WaitRecvData } m_state;

    static const int AllgatherTag = 0xf00d0000;

  public:
    AllgatherFuncSM( int verboseLevel, Output::output_location_t loc,
            Info* info, SST::Link*& progressLink, ProtocolAPI* );

    virtual void handleEnterEvent( SST::Event *e);
    virtual void handleProgressEvent( SST::Event *e );

    virtual const char* name() {
       return "Allgather"; 
    }

  private:

    SST::Link*&         m_toProgressLink;
    CtrlMsg*            m_ctrlMsg;
    GatherEnterEvent*   m_event;
    bool                m_pending;
    CtrlMsg::CommReq    m_sendReq; 
    CtrlMsg::CommReq    m_recvReq; 
    std::vector<CtrlMsg::CommReq>  m_recvReqV;

    unsigned char* chunkPtr( int rank ) {
        unsigned char* ptr = (unsigned char*) m_event->recvbuf;
        if ( m_event->recvcntPtr ) {
            ptr += ((int*)m_event->displsPtr)[rank]; 
        } else {
            ptr += rank * chunkSize( rank );
        }
        m_dbg.verbose(CALL_INFO,2,0,"rank %d, ptr %p\n", rank, ptr);
 
        return ptr;
    }

    size_t  chunkSize( int rank ) {
        size_t size;
        if ( m_event->recvcntPtr ) {
            size = m_info->sizeofDataType( m_event->recvtype ) *
                        ((int*)m_event->recvcntPtr)[rank]; 
        } else {
            size = m_info->sizeofDataType( m_event->recvtype ) *
                                                m_event->recvcnt;
        } 
        m_dbg.verbose(CALL_INFO,2,0,"rank %d, size %lu\n",rank,size);
        return size;
    }

    std::vector<int>    m_numChunks;
    std::vector<int>    m_sendStartChunk;
    std::vector<int>    m_dest;
    int                 m_rank; 
    int                 m_size; 
    unsigned int        m_currentStage;

    void initIoVec(std::vector<CtrlMsg::IoVec>& ioVec,
                                    int startChunk, int numChunks);
};
        
}
}

#endif
