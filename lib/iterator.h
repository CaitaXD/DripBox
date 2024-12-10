#ifndef ITERATOR_H
#define ITERATOR_H

#include <stdbool.h>
#include <common.h>

struct iterator {
  bool (*next)(struct iterator *);
  void* (*current)(struct iterator *);
};

#define iterator_last(iterator__) iterator_last_impl((void*)(iterator__))
#define iterator_first(iterator__) iterator_first_impl((void*)(iterator__))

static void* iterator_last_impl(struct iterator *it) {
    if (it->next(it)) {
      void *current;
      do {
        current = it->current(it);
      } while (it->next(it));
      return current;
    }
    assert(false && "Iterator is empty");
    return NULL;
}

static void* iterator_first_impl(struct iterator *it)
{
  if (it->next(it)) {
    return it->current(it);
  }
  assert(false && "Iterator is empty");
  return NULL;
}

static void iterator_skip(struct iterator *it, int skip) {
    while (skip-- > 0) {
        if(!it->next(it)) {
            assert(false && "Iterator is empty");
            return;
        }
    }
}

static bool iterator_contains_impl(struct iterator *it, const void *element,
                                   const bool (*equals)(const void *, const void *))
{
  while (it->next(it)) {
    if (equals(it->current(it), element)) {
      return true;
    }
  }
  return false;
}

#endif //ITERATOR_H
