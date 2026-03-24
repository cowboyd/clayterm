#ifndef TRIE_H
#define TRIE_H

#include <stdint.h>

#define PARSE_OK 0
#define PARSE_ERR 1
#define PARSE_NEED_MORE 2

#define MAX_TRIE_NODES 1024

struct TrieNode {
  uint8_t byte;
  uint8_t is_leaf;
  uint16_t key;
  uint8_t mod;
  uint8_t pad;
  uint16_t child;
  uint16_t sibling;
};

typedef struct TrieNode Trie[MAX_TRIE_NODES];

void trie_init(Trie trie, int *len);

int trie_add(Trie trie, int *len, const char *seq, int slen, uint16_t key,
             uint8_t mod);

int trie_match(const Trie trie, const char *buf, int buflen, int *consumed,
               uint16_t *key, uint8_t *mod);

#endif
