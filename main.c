
/*
 * main.c
 */

#include <stdlib.h>
#include <stdio.h>
#include "cache.h"
#include "main.h"

void myprint(Pcache_line plptr)
{
  if (plptr != NULL){
    printf("%d->", plptr->tag);
    myprint(plptr->LRU_next);
  } else 
  printf("NULL\n");
  
}

static FILE *traceFile;
int main(argc, argv)
int argc;
char **argv;
{
/*Pcache tcache = (Pcache) malloc(sizeof(cache));
tcache->associativity = 1;
tcache->index_mask = 2;

printf("assoc = %d\n", tcache->associativity);
printf("index_mas = %d\n", tcache->index_mask);
tcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line));
tcache->LRU_head[0] = NULL;//(Pcache_line)malloc(sizeof(cache_line));
//tcache->LRU_head[0]->tag = 1;
//tcache->LRU_head[0]->LRU_next = NULL;

tcache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line));
tcache->LRU_tail[0] = NULL;

Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
item->tag = 2;
insert(tcache->LRU_head, tcache->LRU_tail, item);

item = (Pcache_line)malloc(sizeof(cache_line));
item->tag = 3;
insert(tcache->LRU_head, tcache->LRU_tail, item);

item = (Pcache_line)malloc(sizeof(cache_line));
item->tag =4;
insert(tcache->LRU_head, tcache->LRU_tail, item);


myprint(tcache->LRU_head[0]);


//create direct map


int n_sets = 3;
int m = 1;
int n = n_sets;

Pcache xcache = (Pcache) malloc(sizeof(cache));
xcache->associativity = 1;
xcache->index_mask = 2;
xcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*n);

//
printf("DIRECT MAP\n");
for (int i=0; i < n; i++)
{
  xcache->LRU_head[i] = (Pcache_line)malloc(sizeof(cache_line));
  xcache->LRU_head[i]->tag = i;
  xcache->LRU_head[i]->LRU_next = NULL;
  xcache->LRU_head[i]->LRU_prev = NULL;
  myprint(xcache->LRU_head[i]);
  
}

//full associative
n_sets = 3;
m = n_sets;
n = 1;
Pcache fcache = (Pcache) malloc(sizeof(cache));
fcache->associativity = 1;
fcache->index_mask = 2;
fcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line));
fcache->LRU_head[0] = NULL;

fcache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line));
fcache->LRU_head[0] = NULL;


//
printf("FULLY ASSOC\n");
for (int i=0; i < m; i++)
{
  Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
  item->tag = i;
  
  insert(fcache->LRU_head, fcache->LRU_tail, item);
  
}
 myprint(fcache->LRU_head[0]);

//set associative
n_sets = 3;
m = 4;
n = 3;
Pcache scache = (Pcache) malloc(sizeof(cache));
scache->associativity = 1;
scache->index_mask = 2;
scache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line)*n);
scache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line)*n);


//
printf("SET ASSOC\n");
for (int i=0; i < n; i++)
{
  scache->LRU_head[i] = NULL;
  scache->LRU_tail[i] = NULL;
  for (int j=0; j < m; j++)
  {
    Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
    item->tag = j*i + j;
  
    insert(&scache->LRU_head[i], &scache->LRU_tail[i], item);

  }
  myprint(scache->LRU_head[i]);

}
 //myprint(scache->LRU_head[0]);
 // myprint(scache->LRU_head[1]);



Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
item->tag = 2;
item->LRU_next = NULL;
item->LRU_prev = NULL;

Pcache_line item2 = (Pcache_line)malloc(sizeof(cache_line));
item2->tag = 3;


item2->LRU_next = NULL;
item2->LRU_prev = NULL;

insert(tcache->LRU_head,tcache->LRU_tail, item);
insert(tcache->LRU_head,tcache->LRU_tail, item2);


myprint(tcache->LRU_head[0]);
*/




  
  parse_args(argc, argv);
  printf("111111");
  init_cache();
  printf("hhhhhhhh");
  play_trace(traceFile);
  print_stats();
}

/************************************************************/
void parse_args(argc, argv) int argc;
char **argv;
{
  int arg_index, i, value;

  if (argc < 2)
  {
    printf("usage:  sim <options> <trace file>\n");
    exit(-1);
  }

  /* parse the command line arguments */
  for (i = 0; i < argc; i++)
    if (!strcmp(argv[i], "-h"))
    {
      printf("\t-h:  \t\tthis message\n\n");
      printf("\t-bs <bs>: \tset cache block size to <bs>\n");
      printf("\t-us <us>: \tset unified cache size to <us>\n");
      printf("\t-is <is>: \tset instruction cache size to <is>\n");
      printf("\t-ds <ds>: \tset data cache size to <ds>\n");
      printf("\t-a <a>: \tset cache associativity to <a>\n");
      printf("\t-wb: \t\tset write policy to write back\n");
      printf("\t-wt: \t\tset write policy to write through\n");
      printf("\t-wa: \t\tset allocation policy to write allocate\n");
      printf("\t-nw: \t\tset allocation policy to no write allocate\n");
      exit(0);
    }

  arg_index = 1;
  while (arg_index != argc - 1)
  {

    /* set the cache simulator parameters */

    if (!strcmp(argv[arg_index], "-bs"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_BLOCK_SIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-us"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_USIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-is"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_ISIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-ds"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_DSIZE, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-a"))
    {
      value = atoi(argv[arg_index + 1]);
      set_cache_param(CACHE_PARAM_ASSOC, value);
      arg_index += 2;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wb"))
    {
      set_cache_param(CACHE_PARAM_WRITEBACK, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wt"))
    {
      set_cache_param(CACHE_PARAM_WRITETHROUGH, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-wa"))
    {
      set_cache_param(CACHE_PARAM_WRITEALLOC, value);
      arg_index += 1;
      continue;
    }

    if (!strcmp(argv[arg_index], "-nw"))
    {
      set_cache_param(CACHE_PARAM_NOWRITEALLOC, value);
      arg_index += 1;
      continue;
    }

    printf("error:  unrecognized flag %s\n", argv[arg_index]);
    exit(-1);
  }

  dump_settings();

  /* open the trace file */
  traceFile = fopen(argv[arg_index], "r");

  return;
}
/************************************************************/

/************************************************************/
void play_trace(inFile)
    FILE *inFile;
{
  unsigned addr, data, access_type;
  int num_inst;
  //printf("play_trace");
  num_inst = 0;
  while (read_trace_element(inFile, &access_type, &addr))
  {
    switch (access_type)
    {
    case TRACE_DATA_LOAD:
    case TRACE_DATA_STORE:
    case TRACE_INST_LOAD:
      perform_access(addr, access_type);
      break;

    default:
      printf("skipping access, unknown type(%d)\n", access_type);
    }

    num_inst++;
    if (!(num_inst % PRINT_INTERVAL))
      printf("processed %d references\n", num_inst);
  }

  flush();
}
/************************************************************/

/************************************************************/
int read_trace_element(inFile, access_type, addr)
FILE *inFile;
unsigned *access_type, *addr;
{
  int result;
  char c;

  result = fscanf(inFile, "%u %x%c", access_type, addr, &c);
 
  while (c != '\n')
  {
    result = fscanf(inFile, "%c", &c);
    if (result == EOF)
      break;
  }
  if (result != EOF)
    return (1);
  else
    return (0);
}
/************************************************************/
