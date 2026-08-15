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
  size_t mask = 0;    // power of 2 array size, 2^n - 1
  size_t size = 0;    // number of keys
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
