#pragma once
#include <stddef.h>
#include <stdint.h>

struct HNode {
  HNode *next = NULL;
  uint64_t hcode = 0;
};

struct HTab {
  HNode **tab = NULL; // array of slots
  size_t mask = 0;    // used to do fast bitwise mod operation -- 
                      // mask is the bucket count minus one. Because bucket counts are powers of
                      // two, hcode & mask is equivalent to hcode % bucket_count and selects a
                      // valid bucket index.
  size_t size = 0;    // number of keys (currently)
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;
};

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_insert(HMap *hmap, HNode *node);
HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *));
void hm_clear(HMap *hmap);
size_t hm_size(HMap *hmap);
void hm_foreach(HMap *hmap, bool (*f)(HNode *, void *), void *arg);