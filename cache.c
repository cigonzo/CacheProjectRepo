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

int extractBits(int value, int numBits, int startBit)
{
  int bits = (value >> startBit) & ((1u << numBits) - 1);

  return bits;
}
/************************************************************/

/************************************************************/
void init_cache()
{
  // have to figure out tag, index, offset bits

  // the offset bits are always the log 2 of block size
  int index_mask_offset = LOG2(cache_block_size);

  // always allocate dcache because unified uses dcache

  // set all dcache stats to zero
  cache_stat_data.accesses = 0;
  cache_stat_data.copies_back = 0;
  cache_stat_data.demand_fetches = 0;
  cache_stat_data.misses = 0;
  cache_stat_data.replacements = 0;

  // first allocate cache
  dcache = (Pcache)malloc(sizeof(cache));
  dcache->associativity = cache_assoc;

  // use unified size if user selected unified -us
  if (cache_split)
    dcache->size = cache_dsize;
  else
    dcache->size = cache_usize;

  // the cache stores the offset bits
  dcache->index_mask_offset = index_mask_offset;

  // check to see if this is a direct map or set associative
  if (dcache->associativity == 1)
  {
    // allocate direct map data structure

    // doing direct map

    // the number of sets is the cache size divided by block size
    dcache->n_sets = dcache->size / cache_block_size;

    // the number of index bits is the log 2 of the number of sets
    // that is, need "n" number of index bits to reference the number of sets
    int num_index_bits = LOG2(dcache->n_sets);

    // have to calculate the index mask in order to pull out the index bits from the
    // address. The mask is just setting 1's where the index bits reside in the address.
    // Since there are index_mask_offset bits we have to start extracting from that bit.

    // for example,
    // an integer is 32 bits
    // num_index_bits = 9 bits
    // index_mask_offset = 4 bits
    // there fore the tag bits is what is left over, tag bits = 32 - 9 - 4
    int mask = 0xFFFFFFFF;
    //            1098 7654 3210 9876 5432 10             <---- bit number index 10's place
    // mask bits   3322 2222 2222 1111 1111 1198 7654 3210 <---- bit number index 1's place
    // mask is now 1111 1111 1111 1111 1111 1111 1111 1111
    // extractBits(mask, 9+4, 0)-> extract 13 bits starting at index 0
    mask = extractBits(mask, num_index_bits + dcache->index_mask_offset, 0); // extract out the index/offset bits mask

    //            1098 7654 3210 9876 5432 10             <---- bit number 10's place
    // mask bits   3322 2222 2222 1111 1111 1198 7654 3210 <---- bit number 1' place
    // mask is now 0000 0000 0000 0000 0001 1111 1111 1111
    // it is going to mask the last 13 bits, index bits + offset bits
    dcache->index_mask = mask;

    // for direct mapping need the head of the linked list
    dcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);
    /* allocating pointers
    -------------
    | 0 |--->NULL
    | 1 |--->NULL
    | 2 |--->NULL
    -------------
    */

    // each set needs 1 cache line
    //  allocates cache line array
    for (int i = 0; i < dcache->n_sets; i++)
    {
      // creates cache line for each pointer
      dcache->LRU_head[i] = (Pcache_line)malloc(sizeof(cache_line));
      /*
      -------------
      | 0 | ----> cache line
      | 1 | ----> cache line
      | 2 |-----> cache line
      -------------
    */
      // used 999999 to indicate that this is an empty cache
      // I could not figure out how to not allocate before
      // running perform access
      dcache->LRU_head[i]->tag = 999999; // empty cache
      dcache->LRU_head[i]->LRU_next = NULL;
      dcache->LRU_head[i]->LRU_prev = NULL;
    }
  }

  else if (dcache->associativity > 1)
  {
    // allocate set associative data structure

    // number of sets is the number of sets like direct map but divided by the number
    // of associativity, because each set will have "n" cache lines. In direct map
    // each set had only one cache line.
    dcache->n_sets = dcache->size / cache_block_size;
    dcache->n_sets = dcache->n_sets / cache_assoc; //   for set associative one cache line per set of associates

    // the set_contents keeps track of how many cache lines in each set so
    // we know when the cache is full. need to check this when inserting cach lines
    // into the cache
    dcache->set_contents = (int *)malloc(sizeof(int) * dcache->n_sets);
    // allocates linked list
    dcache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);
    dcache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * dcache->n_sets);

    // sets linked list to NULL to start, nothing in the cache
    for (int i = 0; i < dcache->n_sets; i++)
    {
      dcache->LRU_head[i] = NULL;
      dcache->LRU_tail[i] = NULL;
      dcache->set_contents[i] = 0;
    }
  }

  /* initialize the cache, and cache statistics data structures */
  // this is the same as the dcache above but just for the icache
  if (cache_split)
  {

    // split cache means 2 caches i cache and d cache

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
      //   direct mapping
      icache->n_sets = icache->size / cache_block_size;

      int num_index_bits = LOG2(icache->n_sets);
      int mask = 0xFFFFFFFF;                                                   //   start with all 1's
      mask = extractBits(mask, num_index_bits + icache->index_mask_offset, 0); //   extract out the index/offset bits mask

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
      //   create for set associative

      icache->n_sets = icache->size / cache_block_size;
      icache->n_sets = icache->n_sets / cache_assoc; //   for set associative one cache line per set of associates
      icache->set_contents = (int *)malloc(sizeof(int) * icache->n_sets);

      icache->LRU_head = (Pcache_line *)malloc(sizeof(Pcache_line) * icache->n_sets);
      icache->LRU_tail = (Pcache_line *)malloc(sizeof(Pcache_line) * icache->n_sets);

      for (int i = 0; i < icache->n_sets; i++)
      {
        icache->LRU_head[i] = NULL;
        icache->LRU_tail[i] = NULL;
        icache->set_contents[i] = 0;
      }
    }
  }
 
}
/************************************************************/

/************************************************************/
//   set associative cache function
void setAssoc(int index_mask_offset, int addr, int access_type, Pcache ucache, cache_stat *cacheStat)
{
  //   need to implement
  cacheStat->accesses++;
  int num_index_bits = LOG2(ucache->n_sets);
  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + ucache->index_mask_offset), num_index_bits + ucache->index_mask_offset);
  // printf("tag  = %-12x\n", addr_tag);

  unsigned index_u = (addr & ucache->index_mask) >> ucache->index_mask_offset;
  //printf("addr,index,tag: %-12x %-8x %-12x\n", addr, index_u, addr_tag);

  if (ucache->LRU_head[index_u] != NULL && ucache->LRU_head[index_u]->tag == addr_tag)
  {

    // HIT
    if (access_type == 0 || access_type == 2)
    {
      // read HIT
      Pcache_line temp = ucache->LRU_head[index_u];
      delete (&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], temp);
      insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], temp);
    }
    else
    {
      // write HIT
      //    write/store
      if (ucache->LRU_head[index_u]->dirty == 1)
      {
        //   write data back to memory.
        cacheStat->copies_back++;             //   cache is dirty write back to memory
        ucache->LRU_head[index_u]->dirty = 0; //   write back makes it clean?
      }

      if (ucache->LRU_head[index_u]->tag != addr_tag)
        cacheStat->replacements++;

      Pcache_line temp = ucache->LRU_head[index_u];
      delete (&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], temp);
      insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], temp);
    }
  }
  else
  {
    // MISS
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
        item->dirty = 0;

        insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], item);
        ucache->set_contents[index_u] = ucache->set_contents[index_u] + 1;
        ucache->LRU_head[index_u]->dirty = 0;
        cacheStat->replacements++;
      }
      else
      {
        // cache is full so LRU replace
        // delete tail and insert new item

        // delete tail
        delete (&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], ucache->LRU_tail[index_u]);

        // insert new item
        Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
        item->tag = addr_tag;
        item->dirty = 0;

        insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], item);
      }
    }
    else
    {
      // MISS write
      if (!cache_writeback)
      {
        // write through
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
          // cache is full so LRU replace, delete tail insert new item

          // delete tail
          delete (&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], ucache->LRU_tail[index_u]);

          // insert new item
          Pcache_line item = (Pcache_line)malloc(sizeof(cache_line));
          item->tag = addr_tag;
          item->dirty = 0;

          insert(&ucache->LRU_head[index_u], &ucache->LRU_tail[index_u], item);
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

//   directMap cache function
void directMap(int index_mask_offset, int addr, int access_type, Pcache ucache, cache_stat *cacheStat)
{
  // this code is direct mapping code
  // updates access count
  cacheStat->accesses++;

  // figures out index bits
  int num_index_bits = LOG2(ucache->n_sets);
  // gets tag out of address
  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + ucache->index_mask_offset), num_index_bits + ucache->index_mask_offset);

  // printf("tag  = %-12x\n", addr_tag);

  unsigned index_u = (addr & ucache->index_mask) >> ucache->index_mask_offset;
  //printf("addr,index,tag: %-12x %-8x %-12x\n", addr, index_u, addr_tag);

  // printf("index_u = %d\n", index_u);
  // Then, check the cache line at this index, my cache.LRU head[index]. If the cache line has a
  // tag that matches the address’ tag, you have a cache hit. Otherwise, you have a cache miss1
  if (ucache->LRU_head[index_u]->tag == addr_tag)
  {
    //printf("HIT\n");
    //   HIT
    if (access_type == 0 || access_type == 2)
    {
      // this is a read/load
      for (int i = 0; i < words_per_block; i++)
      {
        // increment fetch count
        cacheStat->demand_fetches++;
      }
    }
    else
    {

      //   write/store
      if (ucache->LRU_head[index_u]->dirty == 1)
      {
        //   write data back to memory.
        cacheStat->copies_back++;             //   cache is dirty write back to memory
        ucache->LRU_head[index_u]->dirty = 0; //   write back makes it clean?
      }

      if (ucache->LRU_head[index_u]->tag != addr_tag)
        cacheStat->replacements++;
      ucache->LRU_head[index_u]->tag = addr_tag;
    }
  }
  else
  {

    //   MISS
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
      //printf("ct = %d\n", ucache->LRU_head[index_u]->tag);

      if (ucache->LRU_head[index_u]->tag != addr_tag && ucache->LRU_head[index_u]->tag != 999999)
        cacheStat->replacements++;

      ucache->LRU_head[index_u]->tag = addr_tag;
    }
    else
    {
      // this is a write miss
      //   add an entry into the cache
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
  // find the offset bits
  int index_mask_offset = LOG2(cache_block_size);

  
  

  /* test with tags.txt and spice.trace
  // get the index bits so that the tag can be extracted out of the address
     int num_index_bits = LOG2(dcache->n_sets);

  // the number of tag bits is the left over from index bits and offset bits
  //  number of tag bits is 32 - (num_index_bits + offset bits)
  // extract out the tag by extracting out tag bits starting index bits + offset
  // for example,
  // addr = 0000 0000 0100 0000 1011 1100 0111 0100
  // number of index bits = 9
  // number of offset bits = 4
  // the tag bits are bits 14-31
  unsigned int addr_tag = extractBits(addr, 32 - (num_index_bits + dcache->index_mask_offset), num_index_bits + dcache->index_mask_offset);
  // addr_tag is now 0000 0000 0100 0000 101
  unsigned index_u = (addr & dcache->index_mask) >> dcache->index_mask_offset;

   addr_tag = addr_tag << num_index_bits;
   addr_tag = addr_tag | index_u;
   printf("tag  = %-12x\n", addr_tag);
   printf("%-12x %-8x %-12x\n", addr, index_u, addr_tag);
 */
  if (cache_assoc == 1)
  {
    if (cache_split)
    {
      // direct map split cache
      if (access_type == 0 || access_type == 1)
      {
        // do data cache
        directMap(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
      }
      else
      {
        // do instruction cache
        directMap(index_mask_offset, addr, access_type, icache, &cache_stat_inst);
      }
    }
    else
    {
      // direct map unified
      directMap(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
    }
  }
  else if (cache_assoc > 1)
  {
    //   set associative
    if (cache_split)
    {
      if (access_type == 0 || access_type == 1)
      {
        //do data cache
        setAssoc(index_mask_offset, addr, access_type, dcache, &cache_stat_data);
      }

      else
      {
        //do instruction cache
        setAssoc(index_mask_offset, addr, access_type, icache, &cache_stat_inst);
      }
    }
    else
    {
      // unified cache
      //do data cache
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
