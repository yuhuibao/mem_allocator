
#include "xmalloc.h"
#include <sys/mman.h>
#include <stdio.h>
// convert node* ptr to address of the mem block
#define BLOCK_MEM(ptr) ((void*)((unsigned long long)ptr+sizeof(header)))
#define BLOCK_HEADER(ptr) ((node*)((unsigned long long)ptr-sizeof(header)))
typedef struct node_t{
    size_t size; // size of the free mem for this block
    struct node_t* next;
    struct node_t* prev;
}node;
typedef struct header_t{
    int free;  // 1 allocated 0 free
    size_t size;  // size of allocated mem for users
}header;
typedef struct foot_t{
    size_t size;  // size of allocated mem for users
}footer;
node* head=NULL;

void
assert_ok(long rv, char* call)
{
    if (rv < 0) {
        fprintf(stderr, "failed call: %s\n", call);
        perror("error");
        exit(1);
    }
}
void fl_remove(node* curr){
    if(!curr->prev){
        if(curr->next){
            head = curr->next;
        } else {
            head = NULL;
        }
    } else{
        curr->prev->next = curr->next;
    }
    if(curr->next){
        curr->next->prev = curr->prev;
    }
}

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
/*splits the block ptr_b by creating a new block after size bytes,
and return this new block */
node* split(node* ptr_b, size_t size){
    node* newptr_b = (node*)((unsigned long long)BLOCK_MEM(ptr_b) + size + sizeof(footer));
    newptr_b->size =  ptr_b->size - size - sizeof(header) - sizeof(footer);
    // add header and footer to the allocated mem block
    header* block_allo = (header*)ptr_b;
    block_allo->size = size;
    block_allo->free = 1;
    footer* footer_allo = (footer*)((unsigned long long)newptr_b - sizeof(footer));
    footer_allo->size = size;
    return newptr_b;
}
/*traverse the free list to find the first block whose size >= bytes*/
void*
xmalloc(size_t bytes)
{
    void* block_mem;
    node *curr=head, *newptr;
    while(curr){
        if(curr->size >= bytes + sizeof(header) + sizeof(footer)){
            block_mem = BLOCK_MEM(curr);
            if(curr->size > bytes + sizeof(header) + sizeof(footer)){
                newptr = split(curr,bytes);
                // the new block just stick to the original place, no need to keep sorted
                fl_replace(newptr,curr);
                return block_mem;
            }
            fl_remove(curr);
            return block_mem;
        }else{
            curr = curr->next;
        }
    }
    /*unable to find a free block on the free list, so ask the kernel using mmap*/
    curr = mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    assert_ok((long)curr,"mmap");
    curr->next = NULL;
    curr->prev = NULL;
    curr->size = 4096;
    newptr = split(curr, bytes);
    fl_replace(newptr, curr);

    return BLOCK_MEM(curr);
}

void
xfree(void* ptr)
{
    unsigned long long p = (unsigned long long)ptr;
    /*locate the prev and next free blocks*/
    // header of the current block
    header* h = (header*) (p - sizeof(header));
    // header of the next block
    header* hn = (header*) (p + h->size + sizeof(footer));
    // previous footer
    footer* f = (footer*) (p - sizeof(header) - sizeof(footer));
    // previous header
    header* hp = (header*)((unsigned long long)f - f->size - sizeof(header));


}

void*
xrealloc(void* prev, size_t bytes)
{
    // TODO: write an optimized realloc
    return 0;
}
int 
main(){
    stats("begin main");
	char           *str, *str2;
	str = (char *)xmalloc(1);
	str2 = (char *)xmalloc(1);
	xfree(str);
	stats("1");
	str = (char *)xmalloc(2);
	stats("2");
	xfree(str2);
	xfree(str);
	stats("end main");
}