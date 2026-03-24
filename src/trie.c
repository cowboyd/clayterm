#include "trie.h"
#include "mem.h"

void trie_init(Trie trie, int *len) {
  memset(&trie[0], 0, sizeof(struct TrieNode));
  *len = 1;
}

int trie_add(Trie trie, int *len, const char *seq, int slen, uint16_t key,
             uint8_t mod) {
  uint16_t cur = 0;

  for (int i = 0; i < slen; i++) {
    uint8_t b = (uint8_t)seq[i];
    uint16_t child = trie[cur].child;
    uint16_t prev = 0;
    int found = 0;

    while (child != 0) {
      if (trie[child].byte == b) {
        cur = child;
        found = 1;
        break;
      }
      prev = child;
      child = trie[child].sibling;
    }

    if (!found) {
      if (*len >= MAX_TRIE_NODES)
        return -1;
      uint16_t idx = (uint16_t)(*len);
      (*len)++;
      memset(&trie[idx], 0, sizeof(struct TrieNode));
      trie[idx].byte = b;

      if (prev != 0) {
        trie[prev].sibling = idx;
      } else {
        trie[cur].child = idx;
      }
      cur = idx;
    }
  }

  if (!trie[cur].is_leaf) {
    trie[cur].is_leaf = 1;
    trie[cur].key = key;
    trie[cur].mod = mod;
  }
  return 0;
}

int trie_match(const Trie trie, const char *buf, int buflen, int *consumed,
               uint16_t *key, uint8_t *mod) {
  uint16_t cur = 0;
  int last_leaf_depth = -1;
  uint16_t last_leaf_key = 0;
  uint8_t last_leaf_mod = 0;
  int has_children = 0;

  for (int i = 0; i < buflen; i++) {
    uint8_t b = (uint8_t)buf[i];
    uint16_t child = trie[cur].child;
    int found = 0;

    while (child != 0) {
      if (trie[child].byte == b) {
        cur = child;
        found = 1;
        break;
      }
      child = trie[child].sibling;
    }

    if (!found) {
      if (last_leaf_depth >= 0) {
        *consumed = last_leaf_depth;
        *key = last_leaf_key;
        *mod = last_leaf_mod;
        return PARSE_OK;
      }
      return PARSE_ERR;
    }

    if (trie[cur].is_leaf) {
      last_leaf_depth = i + 1;
      last_leaf_key = trie[cur].key;
      last_leaf_mod = trie[cur].mod;
    }
    has_children = (trie[cur].child != 0);
  }

  if (last_leaf_depth >= 0 && !has_children) {
    *consumed = last_leaf_depth;
    *key = last_leaf_key;
    *mod = last_leaf_mod;
    return PARSE_OK;
  }
  if (has_children) {
    if (last_leaf_depth >= 0) {
      *consumed = last_leaf_depth;
      *key = last_leaf_key;
      *mod = last_leaf_mod;
    }
    return PARSE_NEED_MORE;
  }
  if (last_leaf_depth >= 0) {
    *consumed = last_leaf_depth;
    *key = last_leaf_key;
    *mod = last_leaf_mod;
    return PARSE_OK;
  }
  return PARSE_ERR;
}
