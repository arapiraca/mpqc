//
// message.h
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

#ifdef __GNUC__
#pragma interface
#endif

#ifndef _util_group_message_h
#define _util_group_message_h

#include <math.h>
#include <util/class/class.h>
#include <util/state/state.h>
#include <util/state/classdRAVLMap.h>
#include <util/keyval/keyval.h>
#include <util/group/topology.h>

template <class T>
class GrpReduce {
  public:
    virtual ~GrpReduce() {};
    virtual void reduce(T*target, T*data, int n) = 0;
};

template <class T>
class GrpSumReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpMinReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpMaxReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpArithmeticAndReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpArithmeticOrReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpArithmeticXOrReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpProductReduce: public GrpReduce<T> {
  public:
    void reduce(T*target, T*data, int nelement);
};

template <class T>
class GrpFunctionReduce: public GrpReduce<T> {
  private:
    void (*func_)(T*target,T*data,int nelement);
  public:
    GrpFunctionReduce(void(*func)(T*,T*,int)):func_(func) {}
    void reduce(T*target, T*data, int nelement);
};

DescribedClass_REF_fwddec(MessageGrp);

//. The \clsnm{MessageGrp} abstract class provides
//. a mechanism for moving data and objects between
//. nodes in a parallel machine.
class MessageGrp: public DescribedClass {
#define CLASSNAME MessageGrp
#include <util/class/classda.h>
  private:
    // These are initialized by the initialize() member (see below).
    int me_;
    int n_;
    int nclass_;
    int gop_max_;
    ClassDescPintRAVLMap classdesc_to_index_;
    ClassDescP *index_to_classdesc_;
  protected:
    //. The classdesc\_to\_index\_ and index\_to\_classdesc\_ arrays
    //. cannot be initialized by the MessageGrp CTOR, because
    //. the MessageGrp specialization has not yet been initialized
    //. and communication is not available.  CTOR's of specializations
    //. of MessageGrp must call the initialize member in their body
    //. to complete the initialization process.
    void initialize(int me, int n);

    RefMachineTopology topology_;

    int debug_;
  public:
    //. Constructors and destructors.
    MessageGrp();
    MessageGrp(const RefKeyVal&);
    virtual ~MessageGrp();
    
    //. Returns the number of processors.
    int n() { return n_; }
    //. Returns my processor number.  In the range [0,n()).
    int me() { return me_; }

    //. The default message group contains the primary message group to
    //. be used by an application.
    static void set_default_messagegrp(const RefMessageGrp&);
    static MessageGrp* get_default_messagegrp();

    //. Create a message group.  This routine looks for a -messagegrp
    //argument, then the environmental variable MESSAGEGRP to decide which
    //specialization of \clsnm{MessageGrp} would be appropriate.  The
    //argument to -messagegrp should be either string for a
    //\clsnmref{ParsedKeyVal} constructor or a classname.  If this returns
    //null, it is up to the programmer to create a \clsnm{MessageGrp}.
    static MessageGrp* initial_messagegrp(int &argc, char** argv);

    //. Send messages sequentially to the target processor.
    virtual void send(int target, double* data, int ndata);
    virtual void send(int target, int* data, int ndata);
    virtual void send(int target, char* data, int nbyte);
    virtual void send(int target, unsigned char* data, int nbyte);
    virtual void send(int target, short* data, int ndata);
    virtual void send(int target, long* data, int ndata);
    virtual void send(int target, float* data, int ndata);
    void send(int target, double data) { send(target,&data,1); }
    void send(int target, int data) { send(target,&data,1); }
    virtual void raw_send(int target, void* data, int nbyte) = 0;

    //. Send typed messages to the target processor.
    virtual void sendt(int target, int type, double* data, int ndata);
    virtual void sendt(int target, int type, int* data, int ndata);
    virtual void sendt(int target, int type, char* data, int nbyte);
    virtual void sendt(int target, int type, unsigned char* data, int nbyte);
    virtual void sendt(int target, int type, short* data, int ndata);
    virtual void sendt(int target, int type, long* data, int ndata);
    virtual void sendt(int target, int type, float* data, int ndata);
    void sendt(int target, int type, double data) {sendt(target,type,&data,1);}
    void sendt(int target, int type, int data) {sendt(target,type&data,1);}
    virtual void raw_sendt(int target, int type, void* data, int nbyte) = 0;

    //. Receive messages sent sequentually from the sender.
    virtual void recv(int sender, double* data, int ndata);
    virtual void recv(int sender, int* data, int ndata);
    virtual void recv(int sender, char* data, int nbyte);
    virtual void recv(int sender, unsigned char* data, int nbyte);
    virtual void recv(int sender, short* data, int ndata);
    virtual void recv(int sender, long* data, int ndata);
    virtual void recv(int sender, float* data, int ndata);
    void recv(int sender, double data) { recv(sender,&data,1); }
    void recv(int sender, int data) { recv(sender,&data,1); }
    virtual void raw_recv(int sender, void* data, int nbyte) = 0;

    //. Receive messages sent by type.
    virtual void recvt(int type, double* data, int ndata);
    virtual void recvt(int type, int* data, int ndata);
    virtual void recvt(int type, char* data, int nbyte);
    virtual void recvt(int type, unsigned char* data, int nbyte);
    virtual void recvt(int type, short* data, int ndata);
    virtual void recvt(int type, long* data, int ndata);
    virtual void recvt(int type, float* data, int ndata);
    void recvt(int type, double data) { recvt(type,&data,1); }
    void recvt(int type, int data) { recvt(type,&data,1); }
    virtual void raw_recvt(int type, void* data, int nbyte) = 0;

    //. Ask if a given typed message has been received.
    virtual int probet(int type) = 0;

    //. Do broadcasts of various types of data.
    virtual void bcast(double* data, int ndata, int from = 0);
    virtual void bcast(int* data, int ndata, int from = 0);
    virtual void bcast(char* data, int nbyte, int from = 0);
    virtual void bcast(unsigned char* data, int nbyte, int from = 0);
    virtual void bcast(short* data, int ndata, int from = 0);
    virtual void bcast(long* data, int ndata, int from = 0);
    virtual void bcast(float* data, int ndata, int from = 0);
    virtual void raw_bcast(void* data, int nbyte, int from = 0);
    void bcast(double& data, int from = 0) { bcast(&data, 1, from); }
    void bcast(int& data, int from = 0) { bcast(&data, 1, from); }

    //. Collect data distributed on the nodes to a big array replicated
    // on each node.
    virtual void raw_collect(const void *part, const int *lengths,
                             void *whole, int bytes_per_datum=1);
    void collect(const double *part, const int *lengths, double *whole);

    //. Do various global reduction operations.
    virtual void sum(double* data, int n, double* = 0, int target = -1);
    virtual void sum(int* data, int n, int* = 0, int target = -1);
    virtual void sum(char* data, int n, char* = 0, int target = -1);
    virtual void sum(unsigned char* data, int n,
                     unsigned char* = 0, int target = -1);
    void sum(double& data) { sum(&data, 1); }
    void sum(int& data) { sum(&data, 1); }
    virtual void max(double* data, int n, double* = 0, int target = -1);
    virtual void max(int* data, int n, int* = 0, int target = -1);
    virtual void max(char* data, int n, char* = 0, int target = -1);
    virtual void max(unsigned char* data, int n,
                     unsigned char* = 0, int target = -1);
    void max(double& data) { max(&data, 1); }
    void max(int& data) { max(&data, 1); }
    virtual void min(double* data, int n, double* = 0, int target = -1);
    virtual void min(int* data, int n, int* = 0, int target = -1);
    virtual void min(char* data, int n, char* = 0, int target = -1);
    virtual void min(unsigned char* data, int n,
                     unsigned char* = 0, int target = -1);
    void min(double& data) { min(&data, 1); }
    void min(int& data) { min(&data, 1); }
    virtual void reduce(double*, int n, GrpReduce<double>&,
                        double*scratch = 0, int target = -1);
    virtual void reduce(int*, int n, GrpReduce<int>&,
                        int*scratch = 0, int target = -1);
    virtual void reduce(char*, int n, GrpReduce<char>&,
                        char*scratch = 0, int target = -1);
    virtual void reduce(unsigned char*, int n, GrpReduce<unsigned char>&,
                        unsigned char*scratch = 0, int target = -1);
    virtual void reduce(short*, int n, GrpReduce<short>&,
                        short*scratch = 0, int target = -1);
    virtual void reduce(float*, int n, GrpReduce<float>&,
                        float*scratch = 0, int target = -1);
    virtual void reduce(long*, int n, GrpReduce<long>&,
                        long*scratch = 0, int target = -1);
    void reduce(double& data, GrpReduce<double>& r) { reduce(&data, 1, r); }
    void reduce(int& data, GrpReduce<int>& r) { reduce(&data, 1, r); }

    //. Synchronize all of the processors.
    virtual void sync();

    //. Returns information about the last message received or probed.
    virtual int last_source() = 0;
    virtual int last_size() = 0;
    virtual int last_type() = 0;

    //. Each message group maintains an association of ClassDesc with
    //. a global index so SavableState information can be sent between
    //. nodes without needing to send the classname and look up the
    //. ClassDesc with each transfer.  These routines return information
    //. about that mapping.
    int classdesc_to_index(const ClassDesc*);
    const ClassDesc* index_to_classdesc(int);
};
DescribedClass_REF_dec(MessageGrp);

//. \clsnm{ProcMessageGrp} provides a concrete specialization
//. of \clsnmref{MessageGrp} that supports only one node.
class ProcMessageGrp: public MessageGrp {
#define CLASSNAME ProcMessageGrp
#define HAVE_KEYVAL_CTOR
#include <util/class/classd.h>
  private:
    // Information about the last message received or probed.
    int last_type_;
    int last_source_;
    int last_size_; // the size in bytes

    void set_last_type(int a) { last_type_ = a; }
    void set_last_source(int a) { last_source_ = a; }
    void set_last_size(int a) { last_size_ = a; }
  public:
    ProcMessageGrp();
    ProcMessageGrp(const RefKeyVal&);
    ~ProcMessageGrp();
    void raw_send(int target, void* data, int nbyte);
    void raw_sendt(int target, int type, void* data, int nbyte);
    void raw_recv(int sender, void* data, int nbyte);
    void raw_recvt(int type, void* data, int nbyte);
    void raw_bcast(void* data, int nbyte, int from);
    int probet(int type);
    void sync();
 
    int last_source();
    int last_size();
    int last_type();
};

//. The \clsnm{intMessageGrp} is an abstract derived class of
//. \clsnmref{MessageGrp}.
//. It uses integer message types to send and receive messages.
//. Message group specializations that use the MPI library
//. and the Paragon NX can be conveniently implemented in terms
//. of this.
class intMessageGrp: public MessageGrp {
#define CLASSNAME intMessageGrp
#include <util/class/classda.h>
  protected:
    int msgtype_nbit; // the total number of bits available
    int ctl_nbit; // control information bits
    int seq_nbit; // sequence information bits
    int typ_nbit; // type information bits
    int src_nbit; // source information bits

    // Masks for the fields in the type.
    int ctl_mask;
    int seq_mask;
    int typ_mask;
    int src_mask;

    // Shifts to construct a message type.
    int ctl_shift;
    int seq_shift;
    int typ_shift;
    int src_shift;

    int msgtype_typ(int msgtype);
    int typ_msgtype(int usrtype);
    int seq_msgtype(int source, int seq);

    // The next sequence number for each node is stored in these.
    int *source_seq;
    int *target_seq;
    
    //. These routines must be implemented by specializations.
    virtual void basic_send(int target, int type, void* data, int nbyte) = 0;
    virtual void basic_recv(int type, void* data, int nbyte) = 0;
    virtual int basic_probe(int type) = 0;

    intMessageGrp();
    intMessageGrp(const RefKeyVal&);
    void initialize(int me, int n, int nbits);
  public:
    ~intMessageGrp();

    void raw_send(int target, void* data, int nbyte);
    void raw_recv(int sender, void* data, int nbyte);
    void raw_sendt(int target, int type, void* data, int nbyte);
    void raw_recvt(int type, void* data, int nbyte);

    int probet(int);
};

#ifndef __GNUG__
#  include <util/group/message.cc>
#endif

#endif


// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
