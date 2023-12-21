
#ifndef LL_COMMON_H_
#define LL_COMMON_H_

// gcc and clang
#define DECLTYPE(x) __typeof__(x)

#ifndef DECLTYPE
  #error "could not find a way to decltype"
#endif

#define LL_APPEND(head, node)                                                  \
  do {                                                                         \
    if((head) == NULL) {                                                       \
      (head) = (node);                                                         \
    } else {                                                                   \
      DECLTYPE(head)                                                           \
      tmp = (head);                                                            \
      while(tmp->next != NULL) {                                               \
        tmp = tmp->next;                                                       \
      }                                                                        \
      tmp->next = (node);                                                      \
    }                                                                          \
  } while(0)

#define LL_FOREACH(root, name)                                                 \
  for(DECLTYPE(root) name = (root); name != NULL; name = name->next)

#endif
