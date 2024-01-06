
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

#define LL_FOREACH_ENUMERATE(root, name, idx)                                  \
  for(int idx = -1, loop_once_ = 1; loop_once_; loop_once_ = 0)                \
    for(DECLTYPE(root) name = (root); name != NULL && ((idx++), 1);            \
        name = name->next)

#define STACK_PUSH(head, elm)                                                  \
  do {                                                                         \
    (elm)->next = (head);                                                      \
    (head) = (elm);                                                            \
  } while(0)
#define STACK_POP(head, res)                                                   \
  do {                                                                         \
    (res) = (head);                                                            \
    (head) = (head)->next;                                                     \
  } while(0)
#define STACK_PEEK(head) (head)
#define STACK_EMPTY(head) ((head) == NULL)

#endif
