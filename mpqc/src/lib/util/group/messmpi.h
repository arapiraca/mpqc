//
// messmpi.h
//
// Copyright (C) 1996 Limit Point Systems, Inc.
//
// Author: Curtis Janssen <cljanss@ca.sandia.gov>
// Maintainer: LPS
//
// This file is part of the SC Toolkit.
//
// The SC Toolkit is free software; you can redistribute it and/or modify
// it under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2, or (at your option)
// any later version.
//
// The SC Toolkit is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with the SC Toolkit; see the file COPYING.LIB.  If not, write to
// the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
//
// The U.S. Government is granted a limited license as per AL 91-7.
//

#ifndef _util_group_messmpi_h
#define _util_group_messmpi_h

#include <util/group/message.h>

class MPIMessageGrp: public MessageGrp {
#define CLASSNAME MPIMessageGrp
#define HAVE_CTOR
#define HAVE_KEYVAL_CTOR
#include <util/class/classd.h>
  protected:
    void* buf;
    int bufsize;

    int rnode;
    int rtag;
    int rlen;

#if HAVE_P4
    int nlocal;     // the number of processes on the master cluster
    int nremote;    // the number of remote clusters
    char *master;   // the name of the master cluster
    char * jobid;   // a unique job name selected by the user

    struct p4_cluster {
        char *hostname;  // name of the remote cluster
        int nslaves;     // the number of slaves on the remote cluster
    } *remote_clusters;

    struct p4_cluster * my_node_info(const char[], int&);
#endif
    
    void init();
  public:
    MPIMessageGrp();
    MPIMessageGrp(const RefKeyVal&);
    ~MPIMessageGrp();

    void raw_send(int target, void* data, int nbyte);
    void raw_recv(int sender, void* data, int nbyte);
    void raw_sendt(int target, int type, void* data, int nbyte);
    void raw_recvt(int type, void* data, int nbyte);

    int probet(int type);

    int last_source();
    int last_size();
    int last_type();

    void sync();

    void reduce(double*, int n, GrpReduce<double>&,
                double*scratch = 0, int target = -1);

    void raw_bcast(void* data, int nbyte, int from);
};

#endif

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
