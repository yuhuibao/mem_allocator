
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include "xmalloc.h"
// convert node* ptr to address of the mem block
#define BLOCK_MEM(ptr) ((void*)((unsigned long long)ptr + sizeof(header)))
#define BLOCK_HEADER(ptr) ((node*)((unsigned long long)ptr - sizeof(header)))
#define PAGE 4096

typedef struct node_t {
  int isfree;
  size_t size;  // size of allocated mem for users but actually allocated size
                // is size+header+footer
  struct node_t* next;
  struct node_t* prev;
} node;

typedef struct header_t {
  int isFree;
  size_t size;  // size of allocated mem for users
} header;

typedef struct foot_t {
  size_t size;  // size of allocated mem for users
} footer;

node* head = NULL;

node* create_new_page();

void assert_ok(long rv, char* call) {
  if (rv < 0) {
    fprintf(stderr, "failed call: %s\n", call);
    perror("error");
    exit(1);
  }
}

void fl_remove(node* curr) {
  if (!curr->prev) {
    if (curr->next) {
      head = curr->next;
    } else {
      head = NULL;
    }
  } else {
    curr->prev->next = curr->next;
  }
  if (curr->next) {
    curr->next->prev = curr->prev;
  }
}

/*
void fl_replace(node* new, node* old){
    if(!old->prev){
        head = new;
        head->prev = NULL;
    }else{
        old->prev->next = new;
        new->prev = old->prev;
    }
    if(old->next){
        old->next->prev = new;
    }
    new->next = old->next;
}
*/

/*add the new free block to the head of the free list*/
void fl_insert(node* n) {
  if (!head) {
    head = n;
    n->prev = NULL;
    n->next = NULL;
  } else {
    n->next = head;
    head->prev = n;
    head = n;
    n->prev = NULL;
  }
}

/*splits the block ptr_b by creating a new block after size bytes,
and return this new block */
node* split(node* ptr_b, size_t size) {
  void* block_mem = BLOCK_MEM(ptr_b);
  node* newptr_b =
      (node*)((unsigned long long)block_mem + size + sizeof(footer));
  newptr_b->size = ptr_b->size - size - sizeof(header) - sizeof(footer);
  newptr_b->isfree = 1;
  // add footer to the new free block
  footer* new_f =
      (footer*)((unsigned long long)newptr_b + sizeof(header) + newptr_b->size);
  new_f->size = newptr_b->size;
  // add header and footer to the allocated mem block
  header* block_allo = (header*)ptr_b;
  block_allo->size = size;
  block_allo->isFree = 0;
  footer* footer_allo =
      (footer*)((unsigned long long)newptr_b - sizeof(footer));
  footer_allo->size = size;
  return newptr_b;
}

/*traverse the free list to find the first block whose size >= bytes*/
void* xmalloc(size_t bytes) {
  void *block_mem, *ptr;
  node *curr = head, *newptr;

  while (curr) {
    if (curr->size < bytes) {
      curr = curr->next;
      continue;
    }

    block_mem = BLOCK_MEM(curr);
    fl_remove(curr);

    if (curr->size == bytes) {
      return block_mem;
    }

    newptr = split(curr, bytes);
    // insert the new free block
    fl_insert(newptr);

    printf("allocate, %ld, %p\n", bytes, block_mem);

    return block_mem;
  }

  curr = create_new_page();

  // printf("allocated size for users %ld\n",curr->size);
  newptr = split(curr, bytes);
  fl_insert(newptr);

  ptr = BLOCK_MEM(curr);

  printf("allocate, %ld, %p\n", bytes, ptr);

  return ptr;
}

node* create_new_page() {
  void* ptr;
  node* curr;

  ptr =
      mmap(0, PAGE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert_ok((long)ptr, "mmap");

  /*set fake footer and fake header*/
  footer* fake_footer = (footer*)ptr;
  fake_footer->size = ULONG_MAX;

  header* fake_header =
      (header*)((unsigned long long)ptr + PAGE - sizeof(header));
  fake_header->size = ULONG_MAX;
  fake_header->isFree = 0;

  curr = (node*)((unsigned long long)ptr + sizeof(footer));
  curr->next = NULL;
  curr->prev = NULL;
  curr->size =
      PAGE - sizeof(header) - sizeof(footer) - sizeof(header) - sizeof(footer);

  return curr;
}

void xfree(void* ptr) {
  node *next, *prev;
  header *hn, *hp = NULL;
  unsigned long long p = (unsigned long long)ptr;
  /*locate the prev and next free blocks*/
  // header of the current block
  header* h = (header*)(p - sizeof(header));
  node* curr = (node*)h;
  curr->isfree = 1;
  curr->size = h->size;
  // header of the next block
  hn = (header*)(p + h->size + sizeof(footer));

  if (hn->isFree) {  // combine with the next free block
    next = (node*)hn;
    curr->next = next->next;
    curr->prev = next->prev;
    if (next->next) {
      next->next->prev = curr;
    }
    if (next->prev) {
      next->prev->next = curr;
    }

    curr->size += next->size + sizeof(header) + sizeof(footer);

    ((footer*)((unsigned long long)curr + sizeof(header) + curr->size))->size =
        curr->size;
  }
  // previous footer
  footer* f = (footer*)(p - sizeof(header) - sizeof(footer));
  if (f->size < ULONG_MAX) {
    // previous header
    hp = (header*)((unsigned long long)f - f->size - sizeof(header));
    if (hp->isFree) {  // combine with the previous free block
      prev = (node*)hp;
      prev->size += curr->size + sizeof(header) + sizeof(footer);
      ((footer*)((unsigned long long)prev + sizeof(header) + prev->size))
          ->size = prev->size;
    }
  }

  if ((!hp || hp->isFree != 1) && hn->isFree != 1) {
    // add the new free block to the head of the free list
    fl_insert(curr);
  }
  printf("free, %ld, %p\n",h->size,ptr);
}

/* stats prints some debug information regarding the
 * current program break and the blocks on the free list */
// Calling sbrk() with an increment of 0 can be used to find the current
// location of the program break.
void stats(char* prefix) {
  // printf("[%s] program break: %10p\n", prefix, sbrk(0));
  //   node* ptr = head;
  //   printf("[%s] free list: \n", prefix);
  //   int c = 0;
  //   while (ptr) {
  //     printf("(%d) <%10p> (size: %ld)\n", c, ptr, ptr->size);
  //     ptr = ptr->next;
  //     c++;
  //   }
}

void* xrealloc(void* prev, size_t bytes) {
  // TODO: write an optimized realloc
  return 0;
}

/*int
main(){
    stats("begin main");
    printf("sizeof header %ld\n",sizeof(header));
    printf("sizeof footer %ld\n",sizeof(footer));
        char           *str, *str2;
        str = (char *)xmalloc(16);
        str2 = (char *)xmalloc(16);
    stats("0");
        xfree(str);
        stats("1");
        str = (char *)xmalloc(32);
        stats("2");
        xfree(str2);
    stats("3");
        xfree(str);
        stats("end main");
    return 0;
}
*/