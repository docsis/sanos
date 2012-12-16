//
// sh.c
//
// Shell
//
// Copyright (C) 2011 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 
// 1. Redistributions of source code must retain the above copyright 
//    notice, this list of conditions and the following disclaimer.  
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.  
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission. 
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
// SUCH DAMAGE.
// 

#ifndef SH_H
#define SH_H

#include <os.h>
#include <crtbase.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <inifile.h>
#include <glob.h>

#include "stmalloc.h"
#include "input.h"
#include "chartype.h"
#include "node.h"
#include "parser.h"

#define SHELL

#define MAX_COMMAND_LEN 8

#ifdef SHELL
#define shellcmd(name) __declspec(dllexport) int cmd_##name(int argc, char *argv[])
#else
#define shellcmd(name) int main(int argc, char *argv[])
#endif

typedef int (*main_t)(int argc, char *argv[]);

struct arg {
  struct arg *next;
  char *value;
};

struct args {
  struct arg *first;
  struct arg *last;
  int num;
};

struct var {
  struct var *next;
  int hash;
  char *name;
  char *value;
};

struct job {
  struct shell *shell;
  struct job *parent;
  struct var *vars;
  struct args args;
  int fd[3];
  int exitcode;
  int handle;
  int pid;
  main_t main;
  struct job *next;
};

struct shell {
  struct job *top;
  struct job *jobs;
};

#endif
