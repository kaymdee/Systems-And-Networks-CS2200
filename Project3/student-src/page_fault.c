#include "paging.h"
#include "pagesim.h"
#include "swapops.h"
#include "stats.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

/*  --------------------------------- PROBLEM 6 --------------------------------------
    Checkout PDF section 7 for this problem

    Page fault handler.

    When the CPU encounters an invalid address mapping in a page table,
    it invokes the OS via this handler.

    Your job is to put a mapping in place so that the translation can
    succeed. You can use free_frame() to make an available frame.
    Update the page table with the new frame, and don't forget
    to fill in the frame table.

    Lastly, you must fill your newly-mapped page with data. If the page has
    never been mapped before, you must read an empty page from disk space and
    update the diskmap. Otherwise, the data will ahve been swapped to the disk
    when the page was evicted last. Call swap_read() to pull the back in from
    swap space.

    HINTS:
         - You will need to use the global variable current_process when
           setting the frame table entry.

    ----------------------------------------------------------------------------------
 */
void page_fault(vaddr_t address) {
    /* First, split the faulting address and locate the page table entry.
       Remember to keep a pointer to the entry so you can modify it later. */
      stats.page_fault = stats.page_fault + 1;
      vpn_t vpn = vaddr_vpn(address);
      uint16_t offset = vaddr_offset(address);
      pte_t *pointer = (pte_t*) (mem + (PAGE_SIZE * PTBR) + (vpn * sizeof(pte_t))); 

    /* It's a page fault, so the entry obviously won't be valid. Grab
       a frame to use by calling free_frame(). */
      pfn_t pfn = free_frame();

    /* Update the page table entry. Make sure you set any relevant values. */
      (*pointer).pfn = pfn;
      (*pointer).valid = 1;
      (*pointer).dirty = 0;

    /* Update the frame table. Make sure you set any relevant values. */
      fte_t *frame_entry = (fte_t*) &frame_table[pfn];
      (*frame_entry).process = current_process;
      (*frame_entry).mapped = 1;
      (*frame_entry).referenced = 1;
      (*frame_entry).vpn = vpn;
      (*frame_entry).protected = 0;
    /* Initialize the page's memory. On a page fault, it is not enough
     * just to allocate a new frame. We must load in the old data from
     * disk into the frame. If there was no old data on disk, then
     * we need to clear out the memory (why?).
     *
     * 1) Get a pointer to the new frame in memory.
     * 2) If the page has swap set, then we need to load in data from memory
     *    using swap_read().
     * 3) Else, read a zeroed-out page from disk space and update the diskmap
     *    for the corresponding page.
     */
     void *frame = mem + pfn * PAGE_SIZE;
     if ((*frame).swap > 0) {
        swap_read(frame_entry, frame);
     } else {
        for (int i = 0 ; i < PAGE_SIZE ; i++) {
           frame[i] = 0;
        }
     }

}

#pragma GCC diagnostic pop
