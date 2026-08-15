#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

struct HNode {
  HNode *next = NULL;
  uint64_t hcode = 0;
};

struct HTab {
  HNode **tab = NULL; // array of slots
  size_t mask = 0;    // used to do fast bitwise mod operation
  size_t size = 0;    // number of keys
};

struct HMap {
  HTab newer;
  HTab older;
  size_t migrate_pos = 0;
};

static void h_init(HTab *htab, size_t n) {
  assert(n > 0 && ((n - 1) & n) == 0);
  htab->tab = (HNode **)calloc(n, sizeof(HNode *));
  htab->mask = n - 1;
  htab->size = 0;
}

static size_t find_pos(HTab *htab, HNode *key) {
  return key->hcode & htab->mask;
}

static void h_insert(HTab *htab, HNode *node) {
  size_t pos = find_pos(htab, node);
  HNode *next = htab->tab[pos];
  node->next = next;
  htab->tab[pos] = node;
  htab->size++;
}

static HNode **h_lookup(HTab *htab, HNode *key, bool (*eq)(HNode *, HNode *)) {
  if (!htab->tab) {
    return NULL;
  }
  size_t pos = find_pos(htab, key);
  HNode **from = &htab->tab[pos];
  for (HNode *cur; (cur = *from) != NULL; from = &cur->next) {
    if (cur->hcode == key->hcode && eq(cur, key)) {
      return from;
    }
  }
  return NULL;
}

static HNode *h_detach(HTab *htab, HNode **from) {
  HNode *node = *from; // the target node
  *from = node->next;  // update incoming pointer to target
  htab->size--;
  return node;
}

static void hm_trigger_rehashing(HMap *hmap) {
  hmap->older = hmap->newer;
  h_init(&hmap->newer, (hmap->newer.mask + 1) * 2);
  hmap->migrate_pos = 0;
}

const size_t k_rehashing_work = 128;

static void hm_help_rehashing(HMap *hmap) {
  size_t nwork = 0;
  while (nwork < k_rehashing_work && hmap->older.size > 0) {
    HNode **from = &hmap->older.tab[hmap->migrate_pos];
    if (!*from) {
      hmap->migrate_pos++;
      continue;
    }
    h_insert(&hmap->newer, h_detach(&hmap->older, from));
    nwork++;
  }

  if (hmap->older.size == 0 && hmap->older.tab) {
    free(hmap->older.tab);
    hmap->older = HTab{};
  }
}

HNode *hm_lookup(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  HNode **from = h_lookup(&hmap->newer, key, eq);
  if (!from) {
    from = h_lookup(&hmap->older, key, eq);
  }
  return from ? *from : NULL;
}

HNode *hm_delete(HMap *hmap, HNode *key, bool (*eq)(HNode *, HNode *)) {
  hm_help_rehashing(hmap);
  if (HNode **from = h_lookup(&hmap->newer, key, eq)) {
    return h_detach(&hmap->newer, from);
  }
  if (HNode **from = h_lookup(&hmap->older, key, eq)) {
    return h_detach(&hmap->older, from);
  }
  return NULL;
}

// weird name to be honest but the idea is
// we think of load factor limit should be greater than 1 because
// each slot is intended to hold multiple items
// if load > 1 then 1 / load = cur / total so
//                  max_cur = max_load_factor * total
const size_t k_max_load_factor = 8;

void hm_insert(HMap *hmap, HNode *node) {
  if (!hmap->newer.tab) {
    h_init(&hmap->newer, 4); // initialise newer table if empty
  }
  h_insert(&hmap->newer, node); // always insert to the newer table
  if (!hmap->older.tab) {
    size_t capacity = (hmap->newer.mask + 1) * k_max_load_factor;
    if (hmap->newer.size >= capacity) {
      hm_trigger_rehashing(hmap);
    }
  }
  hm_help_rehashing(hmap); // migrate some keys
}
