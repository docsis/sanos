//
// dbg.c
//
// Copyright (c) 2001 Michael Ringgaard. All rights reserved.
//
// Remote debugging support
//

#include <os/krnl.h>

#define DEBUGPORT         0x3F8 // COM1
#define MAX_DBG_CHUNKSIZE PAGESIZE

#define MAX_DBG_PACKETLEN (MAX_DBG_CHUNKSIZE + sizeof(union dbg_body))

int debugging = 0;
int debugger_active = 0;
static char dbgdata[MAX_DBG_PACKETLEN];
struct dbg_evt_trap last_trap;

void init_debug_port()
{
  // Turn off interrupts
  _outp(DEBUGPORT + 1, 0);
  
  // Set 115200 baud, 8 bits, no parity, one stopbit
  _outp(DEBUGPORT + 3, 0x80);
  _outp(DEBUGPORT + 0, 0x01); // 0x0C = 9600, 0x01 = 115200
  _outp(DEBUGPORT + 1, 0x00);
  _outp(DEBUGPORT + 3, 0x03);
  _outp(DEBUGPORT + 2, 0xC7);
  _outp(DEBUGPORT + 4, 0x0B);
}

static void dbg_send(void *buffer, int count)
{
  unsigned char *p = buffer;

  while (count-- > 0)
  {
    while ((_inp(DEBUGPORT + 5) & 0x20) == 0);
    _outp(DEBUGPORT, *p++);
    //kprintf("dbg_send: %02X\n", p[-1]);
  }
}

static void dbg_recv(void *buffer, int count)
{
  unsigned char *p = buffer;

  while (count-- > 0)
  {
    while ((_inp(DEBUGPORT + 5) & 0x01) == 0);
    *p++ = _inp(DEBUGPORT) & 0xFF;
    //kprintf("dbg_recv: %02X\n", p[-1]);
  }
}

static void dbg_send_packet(int cmd, unsigned char id, void *data, unsigned int len)
{
  unsigned int n;
  struct dbg_hdr hdr;
  unsigned char checksum;
  unsigned char *p;

  hdr.signature = DBG_SIGNATURE;
  hdr.cmd = (unsigned char) cmd;
  hdr.len = len;
  hdr.id = id;
  hdr.checksum = 0;

  checksum = 0;
  p = (unsigned char *) &hdr;
  for (n = 0; n < sizeof(struct dbg_hdr); n++) checksum += *p++;
  p = (unsigned char *) data;
  for (n = 0; n < len; n++) checksum += *p++;
  hdr.checksum = -checksum;

  dbg_send(&hdr, sizeof(struct dbg_hdr));
  dbg_send(data, len);
}

static void dbg_send_error(unsigned char errcode, unsigned char id)
{
  dbg_send_packet(errcode, id, NULL, 0);
}

static int dbg_recv_packet(struct dbg_hdr *hdr, void *data)
{
  unsigned int n;
  unsigned char checksum;
  unsigned char *p;

  while (1)
  {
    dbg_recv(&hdr->signature, 1);
    if (hdr->signature == DBG_SIGNATURE) break;
  }

  dbg_recv(&hdr->cmd, 1);
  dbg_recv(&hdr->id, 1);
  dbg_recv(&hdr->checksum, 1);
  dbg_recv((unsigned char *) &hdr->len, 4);
  if (hdr->len > MAX_DBG_PACKETLEN) return -EBUF;
  dbg_recv(data, hdr->len);

  checksum = 0;
  p = (unsigned char *) hdr;
  for (n = 0; n < sizeof(struct dbg_hdr); n++) checksum += *p++;
  p = (unsigned char *) data;
  for (n = 0; n < hdr->len; n++) checksum += *p++;
  if (checksum != 0) return -EIO;

  return hdr->len;
}

static void dbg_connect(struct dbg_hdr *hdr, union dbg_body *body)
{
  if (body->conn.version != DRPC_VERSION)
    dbg_send_error(DBGERR_VERSION, hdr->id);
  else
  {
    body->conn.version = DRPC_VERSION;
    body->conn.tid = last_trap.tid;
    body->conn.traptype = last_trap.traptype;
    body->conn.errcode = last_trap.errcode;
    body->conn.eip = last_trap.eip;
    body->conn.addr = last_trap.addr;
    dbg_send_packet(hdr->cmd + DBGCMD_REPLY, hdr->id, body, sizeof(struct dbg_connect));
  }
}

static void dbg_read_memory(struct dbg_hdr *hdr, union dbg_body *body)
{
  if (!mem_mapped(body->mem.addr, body->mem.size))
    dbg_send_error(DBGERR_INVALIDADDR, hdr->id);
  else
    dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body->mem.addr, body->mem.size);
}

static void dbg_write_memory(struct dbg_hdr *hdr, union dbg_body *body)
{
  if (!mem_mapped(body->mem.addr, body->mem.size))
    dbg_send_error(DBGERR_INVALIDADDR, hdr->id);
  else
  {
    memcpy(body->mem.addr, body->mem.data, body->mem.size);
    dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, NULL, 0);
  }
}

static void dbg_suspend_thread(struct dbg_hdr *hdr, union dbg_body *body)
{
  int n;
  struct thread *t;

  for (n = 0; n < body->thr.count; n++)
  {
    t = get_thread(body->thr.threadids[n]);
    if (t == NULL)
      body->thr.threadids[n] = -ENOENT;
    else
      body->thr.threadids[n] = suspend_thread(t);
  }

  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, hdr->len);
}

static void dbg_resume_thread(struct dbg_hdr *hdr, union dbg_body *body)
{
  int n;
  struct thread *t;

  for (n = 0; n < body->thr.count; n++)
  {
    t = get_thread(body->thr.threadids[n]);
    if (t == NULL)
      body->thr.threadids[n] = -ENOENT;
    else
      body->thr.threadids[n] = resume_thread(t);
  }

  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, hdr->len);
}

static void dbg_get_thread_context(struct dbg_hdr *hdr, union dbg_body *body)
{
  struct thread *t;

  t = get_thread(body->ctx.tid);
  if (!t) 
  {
    dbg_send_error(DBGERR_INVALIDTHREAD, hdr->id);
    return;
  }

  if (!t->ctxt)
  {
    dbg_send_error(DBGERR_NOCONTEXT, hdr->id);
    return;
  }

  memcpy(&body->ctx.ctxt, t->ctxt, sizeof(struct context));
  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, sizeof(struct dbg_context));
}

static void dbg_set_thread_context(struct dbg_hdr *hdr, union dbg_body *body)
{
  struct thread *t;

  t = get_thread(body->ctx.tid);
  if (!t) 
  {
    dbg_send_error(DBGERR_INVALIDTHREAD, hdr->id);
    return;
  }

  if (!t->ctxt)
  {
    dbg_send_error(DBGERR_NOCONTEXT, hdr->id);
    return;
  }

  memcpy(t->ctxt, &body->ctx.ctxt, sizeof(struct context));
  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, NULL, 0);
}

static void dbg_get_selector(struct dbg_hdr *hdr, union dbg_body *body)
{
  int gdtidx = body->sel.sel >> 3;

  if (gdtidx < 0 || gdtidx >= MAXGDT) 
  {
    dbg_send_error(DBGERR_INVALIDSEL, hdr->id);
    return;
  }

  memcpy(&body->sel.seg, &syspage->gdt[gdtidx], sizeof(struct segment));
  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, sizeof(struct dbg_selector));
}

static void dbg_get_modules(struct dbg_hdr *hdr, union dbg_body *body)
{
  struct peb *peb = (struct peb *) PEB_ADDRESS;
  struct module *mod;
  int n = 0;
  
  mod = kmods.modules;
  if (kmods.modules)
  {
    while (1)
    {
      body->mod.mods[n].hmod = mod->hmod;
      body->mod.mods[n].name = mod->name;
      n++;

      mod = mod->next;
      if (mod == kmods.modules) break;
    }
  }

  if (page_mapped(peb) && peb->usermods)
  {
    mod = peb->usermods->modules;
    while (1)
    {
      body->mod.mods[n].hmod = mod->hmod;
      body->mod.mods[n].name = mod->name;
      n++;

      mod = mod->next;
      if (mod == peb->usermods->modules) break;
    }
  }

  if (n == 0) 
  {
    body->mod.mods[n].hmod = (hmodule_t) OSBASE;
    body->mod.mods[n].name = "krnl.dll";
    n++;
  }

  body->mod.count = n;
  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, sizeof(struct dbg_module) + n * 8);
}

static void dbg_get_threads(struct dbg_hdr *hdr, union dbg_body *body)
{
  int n = 0;
  struct thread *t = threadlist;
  while (1)
  {
    body->thr.threadids[n++] = t->id;
    t = t->next;
    if (t == threadlist) break;
  }
  body->thr.count = n;
  dbg_send_packet(hdr->cmd | DBGCMD_REPLY, hdr->id, body, sizeof(struct dbg_thread) + n * sizeof(tid_t));
}

static void dbg_main()
{
  struct dbg_hdr hdr;
  union dbg_body *body = (union dbg_body *) dbgdata;

  int rc;

  if (!debugging)
  {
    init_debug_port();
    debugging = 1;
    kprintf("dbg: waiting for remote debugger...\n");
  }

  while (1)
  {
    rc = dbg_recv_packet(&hdr, dbgdata);
    if (rc < 0)
    {
      kprintf("dbg: error %d receiving debugger command\n", rc);
      continue;
    }

    kprintf("dbg: command %d %d len=%d\n", hdr.id, hdr.cmd, hdr.len);

    switch (hdr.cmd)
    {
      case DBGCMD_CONNECT:
	dbg_connect(&hdr, body);
	break;

      case DBGCMD_CONTINUE:
        dbg_send_packet(DBGCMD_CONTINUE | DBGCMD_REPLY, hdr.id, NULL, 0);
	return;

      case DBGCMD_READ_MEMORY:
	dbg_read_memory(&hdr, body);
	break;

      case DBGCMD_WRITE_MEMORY:
	dbg_write_memory(&hdr, body);
	break;

      case DBGCMD_SUSPEND_THREAD:
	dbg_suspend_thread(&hdr, body);
	break;

      case DBGCMD_RESUME_THREAD:
	dbg_resume_thread(&hdr, body);
	break;

      case DBGCMD_GET_THREAD_CONTEXT:
	dbg_get_thread_context(&hdr, body);
	break;

      case DBGCMD_SET_THREAD_CONTEXT:
	dbg_set_thread_context(&hdr, body);
	break;

      case DBGCMD_GET_SELECTOR:
	dbg_get_selector(&hdr, body);
	break;

      case DBGCMD_GET_MODULES:
	dbg_get_modules(&hdr, body);
	break;

      case DBGCMD_GET_THREADS:
	dbg_get_threads(&hdr, body);
	break;

      default:
	dbg_send_error(DBGERR_INVALIDCMD, hdr.id);
    }
  }
}

void dumpregs(struct context *ctxt)
{
  kprintf("EAX  = %08X EBX  = %08X ECX  = %08X EDX  = %08X\n", ctxt->eax, ctxt->ebx, ctxt->ecx, ctxt->edx);
  kprintf("EDI  = %08X ESI  = %08X EBP  = %08X ESP  = %08X\n", ctxt->edi, ctxt->esi, ctxt->ebp, ctxt->esp);
  kprintf("CS   = %08X DS   = %08X ES   = %08X SS   = %08X\n", ctxt->ecs, ctxt->ds, ctxt->es, ctxt->ess);
  kprintf("EIP  = %08X EFLG = %08X TRAP = %08X ERR  = %08X\n", ctxt->eip, ctxt->eflags, ctxt->traptype, ctxt->errcode);
}

void shell();

void dbg_enter(struct context *ctxt, void *addr)
{
  kprintf("trap %d (%p)\n", ctxt->traptype, addr);
  kprintf("enter kernel debugger\n");
  current_thread()->ctxt = ctxt;
  dumpregs(ctxt);

  //if (ctxt->traptype != 3) panic("system halted");
  //shell();

  last_trap.tid = current_thread()->id;
  last_trap.traptype = ctxt->traptype;
  last_trap.errcode = ctxt->errcode;
  last_trap.eip = ctxt->eip;
  last_trap.addr = addr;

  sti();

  if (debugging)
  {
    dbg_send_packet(DBGEVT_TRAP, 0, &last_trap, sizeof(struct dbg_evt_trap));
  }

  dbg_main();

  if (current_thread()->suspend_count > 0)
    dispatch();
  else
    current_thread()->ctxt = NULL;
}

void dbg_notify_create_thread(struct thread *t, void *startaddr)
{
  struct dbg_evt_create_thread create;

  if (debugging)
  {
    create.tid = t->id;
    create.tib = t->tib;
    create.startaddr = startaddr;

    dbg_send_packet(DBGEVT_CREATE_THREAD, 0, &create, sizeof(struct dbg_evt_create_thread));
    dbg_main();
  }
}

void dbg_notify_exit_thread(struct thread *t)
{
  struct dbg_evt_exit_thread exit;

  if (debugging)
  {
    exit.tid = t->id;
    exit.exitcode = t->exitcode;

    dbg_send_packet(DBGEVT_EXIT_THREAD, 0, &exit, sizeof(struct dbg_evt_exit_thread));
    dbg_main();
  }
}

void dbg_notify_load_module(hmodule_t hmod)
{
  struct dbg_evt_load_module load;

  if (debugging)
  {
    load.hmod = hmod;

    dbg_send_packet(DBGEVT_LOAD_MODULE, 0, &load, sizeof(struct dbg_evt_load_module));
    dbg_main();
  }
}

void dbg_notify_unload_module(hmodule_t hmod)
{
  struct dbg_evt_unload_module unload;

  if (debugging)
  {
    unload.hmod = hmod;

    dbg_send_packet(DBGEVT_UNLOAD_MODULE, 0, &unload, sizeof(struct dbg_evt_unload_module));
    dbg_main();
  }
}

void dbg_output(char *msg)
{
  struct dbg_evt_output output;

  if (debugging && !debugger_active)
  {
    output.msgptr = msg;
    output.msglen = strlen(msg) + 1;

    dbg_send_packet(DBGEVT_OUTPUT, 0, &output, sizeof(struct dbg_evt_output));
    dbg_main();
  }
}