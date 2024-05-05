#include "lib/kernel/list.h"
#include "lib/kernel/listpop.h"
#include "threads/malloc.h"
#include <stdio.h>
#include "tests/threads/tests.h"

// prototypes
void populate(struct list *l, int *a, int n), print_sorted(struct list *l), free_list(struct list *lst), test_list_sort(void);
bool elem_less_than(const struct list_elem *a, const struct list_elem *b, void *aux);


struct item
{
    struct list_elem elem;
    int priority;
};

void populate(struct list *l, int *a, int n)
{
    int i;
    for (i = 0; i < n; ++i)
    {
        struct item * new_item = malloc(sizeof(struct item));
        if (!new_item)
        {
            fail("failed to malloc new_item");
            return;
        }
        new_item->priority = a[i];
        list_push_back(l, (struct list_elem *) new_item);
    }
}

bool elem_less_than(const struct list_elem *a, const struct list_elem *b, void *aux)
{
    struct item *ia = list_entry(a, struct item, elem);
    struct item *ib = list_entry(b, struct item, elem);
    return (ia->priority < ib->priority);
}

void print_sorted(struct list *l)
{
    list_sort(l, elem_less_than, NULL);
    struct list_elem *pos;
    for (pos = list_begin(l);
         pos != list_end(l);
         pos = list_next(pos))
    {
        struct item *it;
        it = list_entry(pos, struct item, elem);
        printf("%d\n", it->priority);
    }
}

void free_list(struct list *lst)
{
    struct list_elem *pos;
    while (!list_empty(lst))
    {
        pos = list_pop_front(lst);
        struct item * it = list_entry(pos, struct item, elem);
        free(it);
    }
}

void test_list_sort(void)
{
    struct list item_list;
    list_init(&item_list);
    // testing populate
    populate(&item_list, ITEMARRAY, ITEMCOUNT);
    // testing print_sorted
    print_sorted(&item_list);
    //free everything
    free_list(&item_list);
}
