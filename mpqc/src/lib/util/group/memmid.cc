//
// memmid.cc
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

#ifndef _util_group_memmid_cc
#define _util_group_memmid_cc

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdio.h> // for sprintf
#include <unistd.h>

#include <util/misc/formio.h>
#include <util/group/memmid.h>

///////////////////////////////////////////////////////////////////////
// The handler function and its data

static int ack_serial_number = 0;

void
MIDMemoryGrp::handler(long *mid)
{
  handler(data_request_buffer_, mid);
}

void
MIDMemoryGrp::handler(MemoryDataRequest& buffer, long *msgid_arg)
{
  int i;
  int mid;
  int dsize;
  int junk;
  double *source_data;
  double *data;

  if (msgid_arg) wait(*msgid_arg);

  const char *request_name = buffer.request_string();
  MemoryDataRequest::Request request = buffer.request();
  int offset = buffer.offset();
  int size = buffer.size();
  int node = buffer.node();
  int from_type = data_type_from_handler_;
  int to_type = data_type_to_handler_;

  const char *handlerstr;
  if (msgid_arg) handlerstr = "====:";
  else handlerstr = "----:";

  if (debug_) {
      char m[80];
      sprintf(m,"%d: %s", me_, handlerstr);
      buffer.print(m);
    }

  switch (request) {
  case MemoryDataRequest::Deactivate:
      mid = send(&junk, sizeof(junk), me(), from_type);
      wait(mid);
      break;
  case MemoryDataRequest::Sync:
      nsync_++;
      if (msgid_arg) activate();
      break;
  case MemoryDataRequest::Retrieve:
      mid = send(&data_[offset], size, node, from_type);
      wait(mid);
      if (debug_) {
          cout << scprintf(
              "%d: %s sent %d bytes at byte offset %d (mid = %d)\n",
              me_, handlerstr, size, offset, mid);
        }
      if (msgid_arg) activate();
      break;
  case MemoryDataRequest::Replace:
      mid = recv(&data_[offset], size, node, to_type);
      wait(mid);
      if (debug_) {
          cout << scprintf(
              "%d: %s replaced %d bytes at byte offset %d (mid = %d)\n",
              me_, handlerstr, size, offset, mid);
        }
      if (use_acknowledgments_) {
          junk = (me() & 0xff) + (ack_serial_number++ << 8);
          mid = send(&junk, sizeof(junk), node, from_type);
          wait(mid);
          if (debug_) {
              cout << scprintf("%d: %s sent ack %d:%d to %d\n",
                               me_, handlerstr, junk&0xff, junk>>8, node);
            }
        }
      if (msgid_arg) activate();
      break;
  case MemoryDataRequest::DoubleSum:
      dsize = size/sizeof(double);
      data = new double[dsize];
      mid = recv(data, size, node, to_type);
      wait(mid);
      if (debug_) {
          cout << scprintf("%d: %s summing %d bytes (%d doubles) (mid = %d)\n",
                           me_, handlerstr, size, size/sizeof(double), mid);
        }
      source_data = (double*) &data_[offset];
      for (i=0; i<dsize; i++) {
          source_data[i] += data[i];
        }
      delete[] data;
      if (use_acknowledgments_) {
          junk = (me() & 0xff) + (ack_serial_number++ << 8);
          mid = send(&junk, sizeof(junk), node, from_type);
          wait(mid);
          if (debug_) {
              cout << scprintf("%d: %s sent ack %d:%d to %d\n",
                               me_, handlerstr, junk&0xff, junk>>8, node);
            }
        }
      if (msgid_arg) activate();
      break;
  default:
      cerr << scprintf("%d: mid_memory_handler: bad request id\n", me_);
      sleep(1);
      abort();
    }
}

///////////////////////////////////////////////////////////////////////
// The MIDMemoryGrp class

#define CLASSNAME MIDMemoryGrp
#define PARENTS public ActiveMsgMemoryGrp
#include <util/class/classi.h>
void *
MIDMemoryGrp::_castdown(const ClassDesc*cd)
{
  void* casts[1];
  casts[0] =  ActiveMsgMemoryGrp::_castdown(cd);
  return do_castdowns(casts,cd);
}

MIDMemoryGrp::MIDMemoryGrp(const RefMessageGrp& msg):
  ActiveMsgMemoryGrp(msg)
{
  if (debug_) cout << scprintf("%d: MIDMemoryGrp CTOR\n", me());
  data_request_type_ = 113;
  data_type_to_handler_ = 114;
  data_type_from_handler_ = 115;
  nsync_ = 0;
  use_acknowledgments_ = 0;
  use_active_messages_ = 1;
  active_ = 0;
}

MIDMemoryGrp::MIDMemoryGrp(const RefKeyVal& keyval):
  ActiveMsgMemoryGrp(keyval)
{
  if (debug_) cout << scprintf("%d: MIDMemoryGrp KeyVal CTOR\n", me());
  
  nsync_ = 0;

  data_request_type_ = keyval->intvalue("request_type");
  if (keyval->error() != KeyVal::OK) data_request_type_ = 113;
  data_type_to_handler_ = keyval->intvalue("to_type");
  if (keyval->error() != KeyVal::OK) data_type_to_handler_ = 114;
  data_type_from_handler_ = keyval->intvalue("from_type");
  if (keyval->error() != KeyVal::OK) data_type_from_handler_ = 115;

  use_acknowledgments_ = keyval->booleanvalue("ack");
  if (keyval->error() != KeyVal::OK) use_acknowledgments_ = 0;
  use_active_messages_ = keyval->booleanvalue("active");
  if (keyval->error() != KeyVal::OK) use_active_messages_ = 1;
  active_ = 0;
}

MIDMemoryGrp::~MIDMemoryGrp()
{
  if (debug_) cout << scprintf("%d: ~MIDMemoryGrp\n", me());
}

void
MIDMemoryGrp::print_memreq(MemoryDataRequest &req,
                           const char *msg, int target)
{
  if (msg == 0) msg = "";

  char newmsg[80];
  if (target == -1) {
      sprintf(newmsg, "%d: %s", me(), msg);
    }
  else {
      sprintf(newmsg, "%d->%d: %s", me(), target, msg);
    }
  req.print(newmsg);
}

void
MIDMemoryGrp::activate()
{
  if (!active_) {
      if (use_active_messages_) {
          data_request_mid_ = postrecv(data_request_buffer_.data(),
                                       data_request_buffer_.nbytes(),
                                       data_request_type_);
        }
      else {
          data_request_mid_ = recv(data_request_buffer_.data(),
                                   data_request_buffer_.nbytes(),
                                   -1,
                                   data_request_type_);
        }
      if (debug_) {
          cout << scprintf("%d: activated memory request handler mid = %d\n",
                           me_, data_request_mid_);
        }
      if (data_request_mid_ == -1) {
          cerr << scprintf("%d: MIDMemoryGrp::activate(): bad mid\n");
          abort();
        }
      active_ = 1;
    }
}

void
MIDMemoryGrp::deactivate()
{
  if (active_) {
      if (debug_) cout << scprintf("%d: MIDMemoryGrp::deactivate()\n", me_);
      sync();
      active_ = 0;
    }
}

void
MIDMemoryGrp::retrieve_data(void *data, int node, int offset, int size)
{
  MemoryDataRequestQueue q;

  long oldlock = lockcomm();

  MemoryDataRequest buf(MemoryDataRequest::Retrieve,
                         me(), offset, size);
  if (debug_) print_memreq(buf, "retrieve:", node);
  int mid = send(buf.data(), buf.nbytes(), node, data_request_type_);
  do_wait("retrieve: send req", mid, q, buf.nbytes(), node);

  mid = recv(data, size, node, data_type_from_handler_);
  do_wait("retrieve: recv dat", mid, q, size, node);

  flush_queue(q);

  unlockcomm(oldlock);
}

void
MIDMemoryGrp::replace_data(void *data, int node, int offset, int size)
{
  MemoryDataRequestQueue q;

  long oldlock = lockcomm();

  MemoryDataRequest buf(MemoryDataRequest::Replace,
                        me(), offset, size);
  if (debug_) print_memreq(buf, "replace:", node);
  int mid = send(buf.data(), buf.nbytes(), node, data_request_type_);
  do_wait("replace: send req", mid, q, buf.nbytes(), node);

  mid = send(data, size, node, data_type_to_handler_);
  do_wait("replace: send dat", mid, q, size, node);

  if (use_acknowledgments_) {
      int junk;
      mid = recv(&junk, sizeof(junk), node, data_type_from_handler_);
      do_wait("replace: recv ack", mid, q, sizeof(junk), node);
      if (debug_)
          cout << scprintf("%d: replace: got ack %d:%d\n",me_,junk&0xff,junk>>8);
    }

  flush_queue(q);

  unlockcomm(oldlock);
}

void
MIDMemoryGrp::sum_data(double *data, int node, int offset, int size)
{
  MemoryDataRequestQueue q;

  long oldlock = lockcomm();

  MemoryDataRequest buf(MemoryDataRequest::DoubleSum,
                        me(), offset, size);
  if (debug_) print_memreq(buf, "sum:", node);
  int mid = send(buf.data(), buf.nbytes(), node, data_request_type_);
  do_wait("sum: send req", mid, q, buf.nbytes(), node);

  mid = send(data, size, node, data_type_to_handler_);
  do_wait("sum: send dat", mid, q, size, node);

  if (use_acknowledgments_) {
      int junk;
      recv(&junk, sizeof(junk), node, data_type_from_handler_);
      do_wait("sum: recv ack", mid, q, sizeof(junk), node);
      if (debug_)
          cout << scprintf("%d: sum: got ack %d:%d\n",me_, junk&0xff, junk>>8);
    }

  flush_queue(q);

  unlockcomm(oldlock);
}

void
MIDMemoryGrp::sync()
{
  int i;
  int mid;

  long oldlock = lockcomm();

  if (debug_) {
      cout << scprintf(
          "%d: MIDMemoryGrp::sync() entered, active = %d\n", me_, active_);
    }

  if (me() == 0) {
      // keep processing requests until all nodes have sent a
      // sync request
      if (debug_) {
          cout << scprintf("%d: outside while loop nsync_ = %d, n() = %d\n",
                           me_, nsync_, n());
        }
      while (nsync_ < n() - 1) {
          if (debug_) {
              cout << scprintf(
                  "%d: waiting for sync or other msg data_req_mid = %d\n",
                  me_, data_request_mid_);
            }
          if (!active_) {
              cerr << me_ << ": MIDMemoryGrp::sync(): not active (1)" << endl;
            }
          mid = wait(data_request_mid_);
          if (debug_) {
              cout << scprintf("%d: got mid = %d (data_request_mid_ = %d)\n",
                     me_, mid, data_request_mid_);
            }
          if (mid == data_request_mid_) {
              got_data_request_mid();
              handler();
              activate();
            }
          else {
              cerr << scprintf(
                  "%d: WARNING: MIDMemoryGrp::sync(1): stray message id = %d",
                  me_, mid) << endl;
            }
        }
      nsync_ = 0;

      if (debug_)
          cout << scprintf("%d: notifying nodes that sync is complete\n", me_);
      // tell all nodes that they can proceed
      MemoryDataRequest buf(MemoryDataRequest::Sync, me());
      for (i=1; i<n(); i++) {
          if (debug_) print_memreq(buf, "sync:", i);
          mid = send(buf.data(), buf.nbytes(), i, data_request_type_);
          if (debug_) 
              cout << scprintf("%d: node %d can proceed (mid = %d)\n",me_,i,mid);
          mid = wait(mid);
          if (debug_) 
              cout << scprintf("%d: node %d got proceed message\n", me_, i, mid);
        }
    }
  else {
      // let node 0 know that i'm done
      MemoryDataRequest buf(MemoryDataRequest::Sync, me());
      if (debug_) print_memreq(buf, "sync:", 0);
      mid = send(buf.data(), buf.nbytes(), 0, data_request_type_);
      if (debug_) 
          cout << scprintf("%d: sending sync (mid = %d)\n", me_, mid);
      int tmpmid;
      do {
          if (!active_) {
              cerr << me_ << ": MIDMemoryGrp::sync(): not active (2)" << endl;
            }
          tmpmid = wait(mid, data_request_mid_);
          if (tmpmid == data_request_mid_) {
              if (debug_) 
                  cout << scprintf("%d: sync: wait: handling request %d-%d\n",
                                   me_, data_request_buffer_.node(),
                                   data_request_buffer_.serial_number());
              got_data_request_mid();
              handler();
              activate();
            }
          else if (tmpmid != mid) {
              cerr << scprintf(
                  "%d: MIDMemoryGrp::sync(2): stray message id = %d\n",
                  me_, tmpmid);
              sleep(1);
              abort();
            }
        } while (tmpmid != mid);
      // watch for the done message from 0 or request messages
      while (!nsync_) {
          if (debug_)
              cout << scprintf("%d: waiting for sync\n", me_);
          if (!active_) {
              cerr << me_ << ": MIDMemoryGrp::sync(): not active (3)" << endl;
            }
          mid = wait(data_request_mid_);
          if (debug_) 
              cout << scprintf("%d: in sync got mid = %d\n", me_, mid);
          if (mid == data_request_mid_) {
              got_data_request_mid();
              handler();
              activate();
            }
          else {
              cerr << scprintf(
                  "%d: WARNING: MIDMemoryGrp::sync(3): stray message"
                  "id = %d\n", me_, mid);
            }
        }
      nsync_ = 0;
    }

  if (debug_)
      cout << scprintf("%d: MIDMemoryGrp::sync() done\n", me_);

  unlockcomm(oldlock);
}

void
MIDMemoryGrp::do_wait(const char *msg, int mid,
                      MemoryDataRequestQueue &q, size_t expectedsize,
                      int node)
{
  long oldlock = lockcomm();

  int tmpmid;
  do {
      if (debug_) 
          cout << scprintf("%d: %s: wait: waiting\n", me_, msg);
      if (!active_) {
          cerr << me_ << ": MIDMemoryGrp::do_wait(): not active" << endl;
        }
      tmpmid = wait(data_request_mid_, mid);
      if (tmpmid == data_request_mid_) {
          got_data_request_mid();
          if (me() < data_request_buffer_.node()) {
              if (debug_) 
                  cout << scprintf("%d: %s: wait: queueing request %d-%d\n",
                                   me_, msg,
                                   data_request_buffer_.node(),
                                   data_request_buffer_.serial_number());
              q.push(data_request_buffer_);
              if (q.n() == MemoryDataRequestQueue::MaxDepth/2) {
                  cout << me_ << ": " << msg
                       << " wait: queue half full while waiting for node "
                       << node << endl;
                }
            }
          else {
              if (debug_) 
                  cout << scprintf("%d: %s: wait: handling request %d-%d\n",
                                   me_, msg,
                                   data_request_buffer_.node(),
                                   data_request_buffer_.serial_number());
              handler();
            }
          activate();
        }
      else if (tmpmid != mid) {
          cerr << scprintf("%d: MIDMemoryGrp: %s: stray message id = %d\n",
                           me_, msg, tmpmid);
          sleep(1);
          abort();
        }
      else {
          if (debug_)
              cout << scprintf("%d: %s: wait: wait complete\n", me_, msg);
        }
    } while (tmpmid != mid);

  unlockcomm(oldlock);
}

void
MIDMemoryGrp::flush_queue(MemoryDataRequestQueue &q)
{
  long oldlock = lockcomm();

  for (int i=0; i<q.n(); i++) {
      if (debug_) 
          cout << scprintf("%d: processing queued request %d-%d\n",
                           me_,
                           data_request_buffer_.node(),
                           data_request_buffer_.serial_number());
      handler(q[i]);
    }
  q.clear();

  unlockcomm(oldlock);
}
void
MIDMemoryGrp::got_data_request_mid()
{
  data_request_mid_ = -1;
  active_ = 0;
}

void
MIDMemoryGrp::catchup()
{
  while (!use_active_messages_ && probe(data_request_mid_)) {
      got_data_request_mid();
      if (debug_) cout << scprintf("%d: catchup: handling request %d-%d",
                                   me_,
                                   data_request_buffer_.node(),
                                   data_request_buffer_.serial_number())
                       << endl;
      handler();
      activate();
    }
}

int
MIDMemoryGrp::probe(long)
{
    cerr << "MIDMemoryGrp:: probe not implemeneted for " << class_name()
         << endl;
    abort();
    return 0;
}

#endif

/////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: c++
// eval: (c-set-style "CLJ")
// End:
