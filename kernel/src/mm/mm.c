#include "mm.h"
#include "limine.h"
#include <stdint.h>
#include <KrnlAid/utils/printf.h>

freelist_entry_t *head = NULL;
freelist_entry_t *tail = NULL;

void* object_space = NULL;
const uint64_t object_space_size = 10; // IN PAGES

uint64_t usable_mem_size = 0;

void mm_init(void) {
    struct limine_memmap_response *response = memmap.response;

    for (uint64_t i = 0; i < response->entry_count; i++) {
        if(response->entries[i]->type == LIMINE_MEMMAP_USABLE ||
           response->entries[i]->type == LIMINE_MEMMAP_ACPI_RECLAIMABLE ||
           response->entries[i]->type == LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE) {

            freelist_entry_t *newhead = (freelist_entry_t *) HIGHER_HALF(response->entries[i]->base);
            newhead->size = response->entries[i]->length / PAGE_SIZE;

            if(head == NULL) {
                head = newhead;
                tail = newhead;
                continue;
            }

            head->next = newhead;
            head = newhead;

            usable_mem_size += response->entries[i]->length;
        }
    }
    head->next = NULL;

    kprintf("Usable memory(before object space allocation): %u B\n", usable_mem_size);

    //reserve object space
    object_space = (void*)mm_alloc_page();
    for(uint64_t i = 0; i < object_space_size - 1; i++) {
        mm_alloc_page(); //allocate the next page too!
    }

}

void* mm_alloc_page() {
    freelist_entry_t *toret = tail;
    if(tail == NULL) {
        return NULL;
    }
    if(tail->size > 1) {
        freelist_entry_t *new_tail = (freelist_entry_t*)((char*)tail + PAGE_SIZE);
        new_tail->size = tail->size - 1;
        new_tail->next = tail->next;
        tail = new_tail;
    } else {
        tail = tail->next;
    }
    usable_mem_size += PAGE_SIZE;
    return (void*)toret;
}

void mm_free_pages(void* page, uint64_t size) {
    if((uintptr_t)page < (uintptr_t)tail) {
        if((uintptr_t)page + (size * PAGE_SIZE) > (uintptr_t)tail) {
            return;
        }
        freelist_entry_t *newtail = (freelist_entry_t*)page;
        newtail->next = tail;
        newtail->size = size;
        tail = newtail;
    } else {
        freelist_entry_t *cur = tail;
        while(cur != NULL) {
            if((uintptr_t)cur->next > (uintptr_t)page) {
                freelist_entry_t *newent = (freelist_entry_t*)page;
                newent->next = cur->next;
                newent->size = size;
                cur->next = newent;
                return;
            }
            cur = cur->next;
        }
    }
}