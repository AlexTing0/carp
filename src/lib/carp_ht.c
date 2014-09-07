#include "carp_ht.h"

static unsigned long carp_ht_rhash (const char *);
static unsigned long carp_ht_hash (const char *, long);
static short int carp_ht_used (carp_ht *);

int main () {
  carp_ht h;
  int status;

  carp_ht_init(&h, 10);

  carp_ht_set(&h, "Maxwell\0", 17);
  carp_ht_set(&h, "llewxaM\0", 18);
  carp_ht_set(&h, "axwellM\0", 19);
  carp_ht_set(&h, "a\0", 19);
  carp_ht_set(&h, "b\0", 19);
  carp_ht_set(&h, "c\0", 19);
  carp_ht_set(&h, "d\0", 19);
  carp_ht_set(&h, "e\0", 19);
  carp_ht_set(&h, "f\0", 19);
  carp_ht_set(&h, "g\0", 19);
  carp_ht_set(&h, "h\0", 19);

  carp_ht_print(&h);

  status = carp_ht_del(&h, "Maxwell\0");
  printf("del status: %d\n", status);

  carp_ht_print(&h);

  carp_ht_cleanup(&h);
}

// djb2 raw hash
static unsigned long carp_ht_rhash (const char *str) {
  assert(str != NULL);

  unsigned long hash = 5381;
  int c;

  while ((c = *str++))
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

static unsigned long carp_ht_hash (const char *str, long size) {
  assert(str != NULL);

  return carp_ht_rhash(str) % size;
}

static short int carp_ht_used (carp_ht *h) {
  long in_use = 0;

  for (long i = 0; i < h->size; i++) {
    carp_ht_entry *base = h->buckets[i];
    while (base) {
	in_use++;
	base = base->next;
    }
  }

  return in_use * 100 / h->size;
}

short int carp_ht_init (carp_ht *h, long size) {
  assert(h != NULL);
  assert(size > 0);

  h->size = size;
  h->buckets = malloc(size * sizeof(carp_ht_entry *));
  if (h->buckets == NULL)
    return 1;

  for (long i = 0; i < h->size; i++)
    h->buckets[i] = 0;

  return 0;
}

short int carp_ht_del (carp_ht *h, const char *key) {
  assert(h != NULL);
  assert(key != NULL);

  unsigned long hash = carp_ht_hash(key, h->size);
  carp_ht_entry *base = h->buckets[hash];
  carp_ht_entry *prev = NULL;

  // nothing in hashed bucket; error
  if (base == NULL) return 1;

  // first bucket is right bucket
  if (!strcmp(base->key, key)) {
    h->buckets[hash] = base->next;
    free(base);
  }

  else {
    // look for bucket in chain
    while (strcmp(base->key, key) != 0) {
      // not found
      if (base->next == NULL) return 2;

      prev = base;
      base = base->next;
    }

    // if base is last, then next is NULL
    // if base is not last, then next is next in chain
    prev->next = base->next;
    free(base);
  }

  return 0;
}


short int carp_ht_set (carp_ht *h, const char *key, long long value) {
  assert(h != NULL);
  assert(key != NULL);

  // too full? resize
  if (carp_ht_used(h) > 60) {
    if (carp_ht_resize(h) == 1)
      return 1;
    else if (carp_ht_set(h, key, value) != 0)
      return 1;
  }

  carp_ht_entry *base = carp_ht_get(h, key);

  // unused bucket
  if (base == NULL) {
    unsigned long hash = carp_ht_hash(key, h->size);
    h->buckets[hash] = calloc(1, sizeof *base);
    base = h->buckets[hash];
  }
  // in the chain?
  else {
    while (base->next) {
      if (strcmp(base->next->key, key) != 0) {
	base->next->value = value;
	return 0;
      }

      base = base->next;
    }

    base->next = calloc(1, sizeof base->next);
    base = base->next;
  }

  strncpy(base->key, key, strlen(key));
  base->value = value;
  base->next = NULL;

  return 0;
}

carp_ht_entry *carp_ht_get (carp_ht *h, const char *key) {
  assert(h != NULL);
  assert(key != NULL);

  unsigned long hash = carp_ht_hash(key, h->size);
  carp_ht_entry *base = h->buckets[hash];

  while (base && strcmp(base->key, key) != 0) {
    if (hash >= h->size) return NULL; // not found

    base = base->next;
  }

  return base;
}

short int carp_ht_resize (carp_ht *h) {
  // TODO: This probably still leaks memory, did not try to free entry lists...
  assert(h != NULL);

  long newsize = 2 * h->size + 1;
  carp_ht newh = { newsize, NULL };

  newh.buckets = calloc(newsize, sizeof(*newh.buckets));
  if (newh.buckets == NULL)
    return 1;

  for (long i = 0; i < h->size; i++)
    if (h->buckets[i]) {
      carp_ht_entry *base = h->buckets[i];

      while (base) {
	const char *key = base->key;
	unsigned long hash = carp_ht_hash(key, h->size);

	carp_ht_set(&newh, base->key, base->value);
	base = base->next;
      }
    }

  carp_ht_cleanup(h);
  h->size = newsize;
  h->buckets = newh.buckets;
  return 0;
}

void carp_ht_print (carp_ht *h) {
  assert(h != NULL);

  printf("{ %d%% full (size %ld)\n", carp_ht_used(h), h->size);;

  //TODO: Does not print along collision lists.
  for (long int i = 0; i < h->size; i++)
    if (h->buckets[i]) {
      carp_ht_entry *base = h->buckets[i];
      do {
	printf("  [%ld] \"%s\": %lld,", i, base->key, base->value);
      } while ((base = base->next));
      printf("\n");
    }

  printf("}\n\n");
}

void carp_ht_cleanup (carp_ht *h) {
  // TODO: Definitely leaks memory.
  assert(h != NULL);

  for (long i = 0; i < h->size; i++)
    if (h->buckets[i]) {
      carp_ht_entry *base = h->buckets[i];

       while (base) {
	carp_ht_entry *next = base->next;
	free(base);
        base = next;
      }
    }

  free(h->buckets);
}
