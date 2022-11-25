/*
 * cache.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "cache.h"
#include "main.h"

/* cache configuration parameters */
static int cache_split = 0;
static int cache_usize = DEFAULT_CACHE_SIZE;
static int cache_isize = DEFAULT_CACHE_SIZE;
static int cache_dsize = DEFAULT_CACHE_SIZE;
static int cache_block_size = DEFAULT_CACHE_BLOCK_SIZE;
static int words_per_block = DEFAULT_CACHE_BLOCK_SIZE / WORD_SIZE;
static int cache_assoc = DEFAULT_CACHE_ASSOC;
static int cache_writeback = DEFAULT_CACHE_WRITEBACK;
static int cache_writealloc = DEFAULT_CACHE_WRITEALLOC;

/* cache model data structures */
static Pcache icache;
static Pcache dcache;
// cgg created ucached as a unified cache, I didn't
// cgg know if unified should be one cache so I just assumed it did
// static Pcache ucache;

static cache c1;
static cache c2;
static cache_stat cache_stat_inst;
static cache_stat cache_stat_data;
static cache_stat cache_stat_uni;

/************************************************************/
void set_cache_param(param, value) int param;
int value;
{

  switch (param)
  {
  case CACHE_PARAM_BLOCK_SIZE:
    cache_block_size = value;
    words_per_block = value / WORD_SIZE;
    break;
  case CACHE_PARAM_USIZE:
    cache_split = FALSE;
    cache_usize = value;
    break;
  case CACHE_PARAM_ISIZE:
    cache_split = TRUE;
    cache_isize = value;
    break;
  case CACHE_PARAM_DSIZE:
    cache_split = TRUE;
    cache_dsize = value;
    break;
  case CACHE_PARAM_ASSOC:
    cache_assoc = value;
    break;
  case CACHE_PARAM_WRITEBACK:
    cache_writeback = TRUE;
    break;
  case CACHE_PARAM_WRITETHROUGH:
    cache_writeback = FALSE;
    break;
  case CACHE_PARAM_WRITEALLOC:
    cache_writealloc = TRUE;
    break;
  case CACHE_PARAM_NOWRITEALLOC:
    cache_writealloc = FALSE;
    break;
  default:
    printf("error set_cache_param: bad parameter value\n");
    exit(-1);
  }
}

// cgg
int extractBits(int value, int numBits, int startBit)
{
  int bits = (value >> startBit) & ((1u << numBits) - 1);

  return bits;
}
/************************************************************/

/************************************************************/
void init_cache()
{
  // cgg have to figure out tag, index, offset bits
  int index_mask_offset = LOG2(cache_block_size);

  // cgg always allocate dcache because unified uses dcache
  cache_stat_data.accesses = 0;
  cache_stat_data.copies_back = 0;
  cache_stat_data.demand_fetches = 0;
  cache_stat_data.misses = 0;
  cache_stat_data.replacements = 0;

  dcache = (Pcache)malloc(sizeof(cache));
  dcache->associativity = cache_assoc;
  if (cache_split)
    dcache->size = cache_dsize;
  else
    dcache->size = cache_usize;

  dcache->index_mask_offset = index_mask_offset;

  if (dcache->associativity == 1)
  {
    // cgg direct mapping
    dcache->n_sets = dcache->size / cache_block_size;

    int num_index_bits = LOG2(dcache->n_sets);
    int mask = 0xFFFFFFFF;                                                   // cgg start with all 1's
    mask = extractBits(mask, num_index_bits + dcache->index_mask_offset, 0); // cgg extract out the index/offset bits mask

    dcache->index_mask = mask;

    dcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);

    for (int i = 0; i < dcache->n_sets; i++)
    {
      dcache->LRU_head[i] = (Pcache_line)malloc(sizeof(cache_line));
      dcache->LRU_head[i]->tag = 999999;
      dcache->LRU_head[i]->LRU_next = NULL;
      dcache->LRU_head[i]->LRU_prev = NULL;
    }
  }
  else if (dcache->associativity > 1)
  {
    printf("ASSOC > 1\n");
    // cgg create for set associative
    dcache->n_sets = dcache->size / cache_block_size;
    dcache->n_sets = dcache->n_sets / cache_assoc; // cgg for set associative one cache line per set of associates
    dcache->set_contents = (int *)malloc(sizeof(int) * dcache->n_sets);

    dcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);
    dcache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);

    for (int i = 0; i < dcache->n_sets; i++)
    {
      dcache->LRU_head[i] = NULL;
      dcache->LRU_tail[i] = NULL;
      dcache->set_contents[i] = 0;

      // cgg every associative has a cache line
      //for (int j = 0; j < dcache->associativity; j++)
      //{
      //  Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
      //  item->tag = 999999;

      //  insert(&dcache->LRU_head[i], &dcache->LRU_tail[i], item);
      //}
    }
  }

  /* initialize the cache, and cache statistics data structures */
  if (cache_split)
  {
    printf("CACHE SPLIT \n");
    //split cache means 2 caches
    cache_stat_inst.accesses = 0;
    cache_stat_inst.copies_back = 0;
    cache_stat_inst.demand_fetches = 0;
    cache_stat_inst.misses = 0;
    cache_stat_inst.replacements = 0;

    icache = (Pcache)malloc(sizeof(cache));
    icache->associativity = cache_assoc;
    icache->size = cache_isize;

    icache->index_mask_offset = index_mask_offset;

    if (icache->associativity == 1)
    {
      // cgg direct mapping
      icache->n_sets = icache->size / cache_block_size;

      int num_index_bits = LOG2(icache->n_sets);
      int mask = 0xFFFFFFFF;                                                   // cgg start with all 1's
      mask = extractBits(mask, num_index_bits + icache->index_mask_offset, 0); // cgg extract out the index/offset bits mask

      icache->index_mask = mask;

      icache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * icache->n_sets);

      for (int i = 0; i < icache->n_sets; i++)
      {

        icache->LRU_head[i] = (Pcache_line)malloc(sizeof(cache_line));
        icache->LRU_head[i]->tag = 999999;
        icache->LRU_head[i]->LRU_next = NULL;
        icache->LRU_head[i]->LRU_prev = NULL;
      }
    }
    else if (icache->associativity > 1)
    {
      // cgg create for set associative

      icache->n_sets = icache->size / cache_block_size;
      icache->n_sets = icache->n_sets / cache_assoc; // cgg for set associative one cache line per set of associates
      icache->set_contents = (int *)malloc(sizeof(int) * icache->n_sets);

      icache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * icache->n_sets);
      icache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * icache->n_sets);

      for (int i = 0; i < icache->n_sets; i++)
      {
        icache->LRU_head[i] = NULL;
        icache->LRU_tail[i] = NULL;
        icache->set_contents[i] = 0;

        // cgg every associative has a cache line
        //for (int j = 0; j < icache->associativity; j++)
        //{
        //  Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
        //  item->tag = 999999;

        //  insert(&icache->LRU_head[i], &icache->LRU_tail[i], item);
        //}
      }
    }
  }
  printf("done calling init cache\n");
}
/************************************************************/

/************************************************************/
// cgg directMap cache function
void setAssoc(int index_mask_offset, int addr, int access_type, Pcache ucache, cache_stat *cacheStat)
{
  // cgg need to implement
  cacheStat->accesses++;
  int num_index_bits = LOG2(ucache->n_sets);
  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + ucache->index_mask_offset), num_index_bits + ucache->index_mask_offset);
  // printf("tag  = %-12x\n", addr_tag);

  unsigned index_u = (addr & ucache->index_mask) >> ucache->index_mask_offset;
  printf("addr,index,tag: %-12x %-8x %-12x\n", addr, index_u, addr_tag);

  
  if (ucache->LRU_head[index_u] != NULL && ucache->LRU_head[index_u]->tag == addr_tag)
  {
    // HIT
  }
  else
  {
    // MISS
    printf("MISSSS\n");
    if (access_type == 0 || access_type == 2)
    {
      // this is a read
      for (int i = 0; i < words_per_block; i++)
      {
        cacheStat->demand_fetches++;
      }
      // insert tag into cache

      if (ucache->set_contents[index_u] < ucache->associativity)
      {
        // cache has room so insert
        Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
        item->tag = addr_tag;

        insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], item);
        ucache->set_contents[index_u] = ucache->set_contents[index_u] + 1;
        ucache->LRU_head[index_u]->dirty = 0;
        cacheStat->replacements++;
      }
      else
      {
        // cache is full so LRU replace
      }
    }
    else
    {
      // MISS write
      if (!cache_writeback)
      {
        //write through
        if (ucache->set_contents[index_u] < ucache->associativity)
        {
          // cache has room so insert
          Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
          item->tag = addr_tag;

          insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], item);
          ucache->set_contents[index_u] = ucache->set_contents[index_u] + 1;
          ucache->LRU_head[index_u]->dirty = 0;
          cacheStat->replacements++;
        }
        else
        {
          // cache is full so LRU replace
        }
      }
      else
      {
        // write back policy
        ucache->LRU_head[index_u]->dirty = 1;
        if (ucache->LRU_head[index_u]->tag != addr_tag)
          cacheStat->replacements++;
        ucache->LRU_head[index_u]->tag = addr_tag;
      }
    }
  }
}

// cgg directMap cache function
void directMap(int index_mask_offset, int addr, int access_type, Pcache ucache, cache_stat *cacheStat)
{
  // this code is direct mapping code

  cacheStat->accesses++;

  int num_index_bits = LOG2(ucache->n_sets);
  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + ucache->index_mask_offset), num_index_bits + ucache->index_mask_offset);
  // printf("tag  = %-12x\n", addr_tag);

  unsigned index_u = (addr & ucache->index_mask) >> ucache->index_mask_offset;
  printf("addr,index,tag: %-12x %-8x %-12x\n", addr, index_u, addr_tag);

  // printf("index_u = %d\n", index_u);
  // Then, check the cache line at this index, my cache.LRU head[index]. If the cache line has a
  // tag that matches the address’ tag, you have a cache hit. Otherwise, you have a cache miss1
  if (ucache->LRU_head[index_u] == NULL)
  {
    printf("ucache->LRU_head[index_u] ==  NULL\n");
    exit(1);
  }
  if (ucache->LRU_head[index_u]->tag == addr_tag)
  {
    printf("HIT\n");
    // cgg HIT
    if (access_type == 0 || access_type == 2)
    {
      // this is a read/load
      for (int i = 0; i < words_per_block; i++)
      {
        cacheStat->demand_fetches++;
      }
    }
    else
    {

      // cgg write/store
      if (ucache->LRU_head[index_u]->dirty == 1)
      {
        // cgg write data back to memory.
        cacheStat->copies_back++;             // cgg cache is dirty write back to memory
        ucache->LRU_head[index_u]->dirty = 0; // cgg write back makes it clean?
      }

      if (ucache->LRU_head[index_u]->tag != addr_tag)
        cacheStat->replacements++;
      ucache->LRU_head[index_u]->tag = addr_tag;
    }
  }
  else
  {

    // cgg MISS
    cacheStat->misses++;

    if (access_type == 0 || access_type == 2)
    {
      // this is a read
      for (int i = 0; i < words_per_block; i++)
      {
        cacheStat->demand_fetches++;
      }
      // insert tag into cache
      ucache->LRU_head[index_u]->dirty = 0;
      printf("ct = %d\n", ucache->LRU_head[index_u]->tag);

      if (ucache->LRU_head[index_u]->tag != addr_tag && ucache->LRU_head[index_u]->tag != 999999)
        cacheStat->replacements++;

      ucache->LRU_head[index_u]->tag = addr_tag;
    }
    else
    {
      // this is a write miss
      // cgg add an entry into the cache
      if (!cache_writeback)
      {
        ucache->LRU_head[index_u]->dirty = 0;

        ucache->LRU_head[index_u]->tag = addr_tag;
      }
      else
      {
        // write back policy
        ucache->LRU_head[index_u]->dirty = 1;
        if (ucache->LRU_head[index_u]->tag != addr_tag)
          cacheStat->replacements++;
        ucache->LRU_head[index_u]->tag = addr_tag;
      }
    }
  }
}
/************************************************************/

/************************************************************/
void perform_access(addr, access_type) unsigned addr, access_type;
{
  int index_mask_offset = LOG2(cache_block_size);
  int num_index_bits = LOG2(dcache->n_sets);

  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + dcache->index_mask_offset), num_index_bits + dcache->index_mask_offset);
  unsigned index_u = (addr & dcache->index_mask) >> dcache->index_mask_offset;

  /* test with tags.txt and spice.trace
   addr_tag = addr_tag << num_index_bits;
   addr_tag = addr_tag | index_u;
   printf("tag  = %-12x\n", addr_tag);
   printf("%-12x %-8x %-12x\n", addr, index_u, addr_tag);
 */
  if (cache_assoc == 1)
  {
    if (cache_split)
    {
      if (access_type == 0 || access_type == 1)
        directMap(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
      else
        directMap(index_mask_offset, addr, access_type, icache, &cache_stat_inst);
    }
    else
    {
      // unified cache
      printf("unified direct mapping\n");
      directMap(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
    }
  }
  else if (cache_assoc > 1)
  {
    // cgg set associative
    if (cache_split)
    {
      if (access_type == 0 || access_type == 1)
        setAssoc(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
      else
        setAssoc(index_mask_offset, addr, access_type, icache, &cache_stat_inst);
    }
    else
    {
      // unified cache
      printf("unified set assoc\n");
      setAssoc(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
    }
  }
}
/************************************************************/

/************************************************************/
void flush()
{

  /* flush the cache */
}
/************************************************************/

/************************************************************/
void delete (head, tail, item)
    Pcache_line *head,
    *tail;
Pcache_line item;
{
  if (item->LRU_prev)
  {
    item->LRU_prev->LRU_next = item->LRU_next;
  }
  else
  {
    /* item at head */
    *head = item->LRU_next;
  }

  if (item->LRU_next)
  {
    item->LRU_next->LRU_prev = item->LRU_prev;
  }
  else
  {
    /* item at tail */
    *tail = item->LRU_prev;
  }
}
/************************************************************/

/************************************************************/
/* inserts at the head of the list */
void insert(head, tail, item)
    Pcache_line *head,
    *tail;
Pcache_line item;
{
  item->LRU_next = *head;
  item->LRU_prev = (Pcache_line)NULL;

  if (item->LRU_next)
    item->LRU_next->LRU_prev = item;
  else
    *tail = item;

  *head = item;
}
/************************************************************/

/************************************************************/
void dump_settings()
{
  printf("*** CACHE SETTINGS ***\n");
  if (cache_split)
  {
    printf("  Split I- D-cache\n");
    printf("  I-cache size: \t%d\n", cache_isize);
    printf("  D-cache size: \t%d\n", cache_dsize);
  }
  else
  {
    printf("  Unified I- D-cache\n");
    printf("  Size: \t%d\n", cache_usize);
  }
  printf("  Associativity: \t%d\n", cache_assoc);
  printf("  Block size: \t%d\n", cache_block_size);
  printf("  Write policy: \t%s\n",
         cache_writeback ? "WRITE BACK" : "WRITE THROUGH");
  printf("  Allocation policy: \t%s\n",
         cache_writealloc ? "WRITE ALLOCATE" : "WRITE NO ALLOCATE");
}
/************************************************************/

/************************************************************/
void print_stats()
{
  printf("\n*** CACHE STATISTICS ***\n");

  printf(" INSTRUCTIONS\n");
  printf("  accesses:  %d\n", cache_stat_inst.accesses);
  printf("  misses:    %d\n", cache_stat_inst.misses);
  if (!cache_stat_inst.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
           (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses,
           1.0 - (float)cache_stat_inst.misses / (float)cache_stat_inst.accesses);
  printf("  replace:   %d\n", cache_stat_inst.replacements);

  printf(" DATA\n");
  printf("  accesses:  %d\n", cache_stat_data.accesses);
  printf("  misses:    %d\n", cache_stat_data.misses);
  if (!cache_stat_data.accesses)
    printf("  miss rate: 0 (0)\n");
  else
    printf("  miss rate: %2.4f (hit rate %2.4f)\n",
           (float)cache_stat_data.misses / (float)cache_stat_data.accesses,
           1.0 - (float)cache_stat_data.misses / (float)cache_stat_data.accesses);
  printf("  replace:   %d\n", cache_stat_data.replacements);

  printf(" TRAFFIC (in words)\n");
  printf("  demand fetch:  %d\n", cache_stat_inst.demand_fetches +
                                      cache_stat_data.demand_fetches);
  printf("  copies back:   %d\n", cache_stat_inst.copies_back +
                                      cache_stat_data.copies_back);
}
/************************************************************/
