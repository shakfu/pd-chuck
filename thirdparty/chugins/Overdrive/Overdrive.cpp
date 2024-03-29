// Copyright (c) 2005 krzYszcz and others.
// This software is copyrighted by Miller Puckette and others.  The following
// terms (the "Standard Improved BSD License") apply to all files associated with
// the software unless explicitly disclaimed in individual files:
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above  
//    copyright notice, this list of conditions and the following 
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
// 3. The name of the author may not be used to endorse or promote
//    products derived from this software without specific prior 
//    written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
// TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,   
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
// IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

//-----------------------------------------------------------------------------
// Entaro ChucK Developer!
// This is a Chugin boilerplate, generated by chuginate!
//-----------------------------------------------------------------------------

// this should align with the correct versions of these ChucK files
#include "chuck_dl.h"
#include "chuck_def.h"

// general includes
#include <stdio.h>
#include <limits.h>
#include <math.h>

// declaration of chugin constructor
CK_DLL_CTOR(overdrive_ctor);
// declaration of chugin desctructor
CK_DLL_DTOR(overdrive_dtor);

CK_DLL_MFUN(overdrive_setDrive);
CK_DLL_MFUN(overdrive_getDrive);

CK_DLL_TICK(overdrive_tick);

// this is a special offset reserved for Chugin internal data
t_CKINT overdrive_data_offset = 0;


// class definition for internal Chugin data
class Overdrive
{
public:
  // constructor
  Overdrive( t_CKFLOAT fs) :
    m_drive(1)
  {
  }
  
  SAMPLE tick( SAMPLE in )
  {
    if (m_drive == 1.0f)
      return in;
    if (in >= 1.0f)
      return 1.0f;
    if (in > 0.0f)
      return (1.0f - powf (1.0f - in, m_drive));
    if (in >= -1.0f)
      return (powf (1.0f + in, m_drive) - 1.0f);
    else return -1.0f;
  }
  
  float setDrive( t_CKFLOAT p )
  {
    if (p<0)
      {
	printf("Overdrive: drive value must be greater than zero.\n");
	return p;
      }
    m_drive = p;
    return p;
  }
  
  float getDrive() { return m_drive; }
  
private:
  // instance data
  float m_drive;
};


// query function: chuck calls this when loading the Chugin
// NOTE: developer will need to modify this function to
// add additional functions to this Chugin
CK_DLL_QUERY( Overdrive )
{
  // hmm, don't change this...
  QUERY->setname(QUERY, "Overdrive");
  
  // begin the class definition
  // can change the second argument to extend a different ChucK class
  QUERY->begin_class(QUERY, "Overdrive", "UGen");
  
  // register the constructor (probably no need to change)
  QUERY->add_ctor(QUERY, overdrive_ctor);
  // register the destructor (probably no need to change)
  QUERY->add_dtor(QUERY, overdrive_dtor);
  
  // for UGen's only: add tick function
  QUERY->add_ugen_func(QUERY, overdrive_tick, NULL, 1, 1);
  
  // NOTE: if this is to be a UGen with more than 1 channel, 
  // e.g., a multichannel UGen -- will need to use add_ugen_funcf()
  // and declare a tickf function using CK_DLL_TICKF
  
  // example of adding setter method
  QUERY->add_mfun(QUERY, overdrive_setDrive, "float", "drive");
  // example of adding argument to the above method
  QUERY->add_arg(QUERY, "float", "arg");
  
  // example of adding getter method
  QUERY->add_mfun(QUERY, overdrive_getDrive, "float", "drive");
  
  // this reserves a variable in the ChucK internal class to store 
  // referene to the c++ class we defined above
  overdrive_data_offset = QUERY->add_mvar(QUERY, "int", "@o_data", false);
  
  // end the class definition
  // IMPORTANT: this MUST be called!
  QUERY->end_class(QUERY);
  
  // wasn't that a breeze?
  return TRUE;
}


// implementation for the constructor
CK_DLL_CTOR(overdrive_ctor)
{
  // get the offset where we'll store our internal c++ class pointer
  OBJ_MEMBER_INT(SELF, overdrive_data_offset) = 0;
  
  // instantiate our internal c++ class representation
  Overdrive * bcdata = new Overdrive(API->vm->srate(VM));
  
  // store the pointer in the ChucK object member
  OBJ_MEMBER_INT(SELF, overdrive_data_offset) = (t_CKINT) bcdata;
}


// implementation for the destructor
CK_DLL_DTOR(overdrive_dtor)
{
  // get our c++ class pointer
  Overdrive * bcdata = (Overdrive *) OBJ_MEMBER_INT(SELF, overdrive_data_offset);
  // check it
  if( bcdata )
    {
      // clean up
      delete bcdata;
      OBJ_MEMBER_INT(SELF, overdrive_data_offset) = 0;
      bcdata = NULL;
    }
}


// implementation for tick function
CK_DLL_TICK(overdrive_tick)
{
  // get our c++ class pointer
  Overdrive * c = (Overdrive *) OBJ_MEMBER_INT(SELF, overdrive_data_offset);
  
  // invoke our tick function; store in the magical out variable
  if(c) *out = c->tick(in);
  
  // yes
  return TRUE;
}


// example implementation for setter
CK_DLL_MFUN(overdrive_setDrive)
{
  // get our c++ class pointer
  Overdrive * bcdata = (Overdrive *) OBJ_MEMBER_INT(SELF, overdrive_data_offset);
  // set the return value
  RETURN->v_float = bcdata->setDrive(GET_NEXT_FLOAT(ARGS));
}


// example implementation for getter
CK_DLL_MFUN(overdrive_getDrive)
{
  // get our c++ class pointer
  Overdrive * bcdata = (Overdrive *) OBJ_MEMBER_INT(SELF, overdrive_data_offset);
  // set the return value
  RETURN->v_float = bcdata->getDrive();
}
