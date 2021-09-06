#include "paging.h"
#include "pagesim.h"
#include "page_splitting.h"
#include "swapops.h"
#include "stats.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

 /* The frame table pointer. You will set this up in system_init. */
fte_t *frame_table;

/*  --------------------------------- PROBLEM 2 --------------------------------------
    Checkout PDF section 4 for this problem

    In this problem, you will initialize frame_table. This is the pointer to the first
    frame table entry.

    The frame table will be located at physical address 0 in our simulated
    memory. You will first assign the frame_table global variable to point to
    this location in memory. You should zero out this first frame table entry
    (remember, size of a frame table entry is PAGESIZE), in case for
    any reason physical memory is not clean.

    You should then mark the first entry in the frame table as protected. We do
    this because we do not want our free frame allocator to give out the frame
    used by the frame table.

    HINTS:
        You will need to use the following global variables:
        - mem: Simulated physical memory already allocated for you.
        - PAGE_SIZE: The size of one page.
        You will need to initialize (set) the following global variable:
        - frame_table: a pointer to the first entry of the frame table

    -----------------------------------------------------------------------------------
*/
void system_init(void) {
    /*
     * 1. Set the frame table pointer to point to the first frame in physical
     * memory. Zero out the memory used by the frame table.
     *
     * Address "0" in memory will be used for the frame table. This table will
     * contain n frame table entries (fte_t), where n is the number of
     * frames in memory. The frame table will be useful later if we need to
     * evict pages during page faults.
     */
    frame_table = &mem[0];
    for (int i = 0; i < PAGE_SIZE; i++) {
        frame_table[i] = 0;
    }
    /*
     * 2. Mark the first frame table entry as protected.
     *
     * The frame table contains entries for all of physical memory,
     * however, there are some frames we never want to evict.
     * We mark these special pages as "protected" to indicate this.
     */
     frame_table.protected = 1;

}

/*  --------------------------------- PROBLEM 3 --------------------------------------
    Checkout PDF section 4 for this problem

    This function gets called every time a new process is created.
    You will need to allocate a new page table for the process in memory using the
    free_frame function so that the process can store its page mappings. Then, you
    will need to store the PFN of this page table in the process's PCB. Finally,
    you will need to initialize the diskmap (the mapping of the process' VPNs
    to swap entries).

    HINTS:
        - Look at the pcb_t struct defined in pagesim.h to know what to set inside.
        - You are not guaranteed that the memory returned by the free frame allocator
        is empty - an existing frame could have been evicted for our new page table.
        - As in the previous problem, think about whether we need to mark any entries
        in the frame_table as protected after we allocate memory for our page table.
    -----------------------------------------------------------------------------------
*/
void proc_init(pcb_t *proc) {
    /*
     * 1. Call the free frame allocator (free_frame) to return a free frame for
     * this process's page table. You should zero-out the memory.
     */
     pfn_t process_frame = free_frame();
     fte_t * process = &frame_table[process_frame];
     pte_t * page_table = (pte_t *) (PAGE_SIZE * process_frame + mem);
     for (int i = 0; i < PAGE_SIZE; i++) {
        (*page_table + i) = 0;
    }

    /*
     * 2. Update the process's PCB with the frame number
     * of the newly allocated page table.
     *
     * Additionally, mark the frame's frame table entry as protected. You do not
     * want your page table to be accidentally evicted.
     */
     (*proc).saved_ptbr = process_frame;
     (*process_frame).protected = 1;
     (*process_frame).mapped = 1;
     (*process_frame).process = proc;


    /*
     * 3. Initialize the process' diskmap
     */
     (*proc).diskmap = diskmap_init();

}

/*  --------------------------------- PROBLEM 4 --------------------------------------
    Checkout PDF section 5 for this problem

    Swaps the currently running process on the CPU to another process.

    Every process has its own page table, as you allocated in proc_init. You will
    need to tell the processor to use the new process's page table.

    HINTS:
        - Look at the global variables defined in pagesim.h. You may be interested in
        the definition of pcb_t as well.
    -----------------------------------------------------------------------------------
 */
void context_switch(pcb_t *proc) {
    PTBR = (*proc).saved_ptbr;
}

/*  --------------------------------- PROBLEM 5 --------------------------------------
    Checkout PDF section 6 for this problem

    Takes an input virtual address and returns the data from the corresponding
    physical memory address. The steps to do this are:

    1) Translate the virtual address into a physical address using the page table.
    2) Go into the memory and read/write the data at the translated address.

    Parameters:
        1) address     - The virtual address to be translated.
        2) rw          - 'r' if the access is a read, 'w' if a write
        3) data        - One byte of data to write to our memory, if the access is a write.
                         This byte will be NULL on read accesses.

    Return:
        The data at the address if the access is a read, or
        the data we just wrote if the access is a write.

    HINTS:
        - You will need to use the macros we defined in Problem 1 in this function.
        - You will need to access the global PTBR value. This will tell you where to
        find the page table. Be very careful when you think about what this register holds!
        - On a page fault, simply call the page_fault function defined in page_fault.c.
        You may assume that the pagefault handler allocated a page for your address
        in the page table after it returns.
        - Make sure to set the dirty bit in the page table entry if it's a write.
        - Make sure to update the stats variables correctly (see stats.h)
        - Make sure to update the referenced field of the physical frame that's being accessed.
    -----------------------------------------------------------------------------------
 */
uint8_t mem_access(vaddr_t address, char rw, uint8_t data) {


    /* Split the address and find the page table entry.
       Remember to keep a pointer to the entry so you can modify it later. */
    vpn_t vpn = vaddr_vpn(address);
    uint16_t offset = vaddr_offset(address);
    pte_t *pointer = (pte_t*) (mem + (PAGE_SIZE * PTBR) + (vpn * sizeof(pte_t))); 

    /* If an entry is invalid, just page fault to allocate a page for the page table. */
    if (pointer.valid == 0) {
        page_fault(address);
    }  

    /* Update the referenced bit of the appropriate frame table entry. */


    /*
        The physical address will be constructed like this:
        -------------------------------------
        |     PFN    |      Offset          |
        -------------------------------------
        where PFN is the value stored in the page table entry.
        We need to calculate the number of bits are in the offset.

        Create the physical address using your offset and the page
        table entry.

        Also, find the frame table entry corresponding to the VPN,
        and make sure set any relevant values.
    */
    pfn_t pfn = (*pointer).pfn;
    fte_t *fte = (fte_t*) &frame_table[pfn];
    (*fte).referenced = 1;
    paddr_t addr = (pfn * PAGE_SIZE) + offset;


    /* Either read or write the data to the physical address
       depending on 'rw' */
    stats.access = stats.access + 1;
    if (rw == 'r') {
        stats.read = stats.read + 1;
    } else {
        stats.write = stats.write + 1;
        (*pointer).dirty = 1;
        mem[addr] = data;
    }

    /* Return the data read/written */
    return mem[addr];
}

/*  --------------------------------- PROBLEM 8 --------------------------------------
    Checkout PDF section 8 for this problem

    When a process exits, you need to free any pages previously occupied by the
    process. Otherwise, every time you closed and re-opened Microsoft Word, it
    would gradually eat more and more of your computer's usable memory.

    To free a process, you must clear the "mapped" bit on every page the process
    has mapped. If the process has swapped any pages to disk, you must call
    swap_free() using the page table entry pointer as a parameter.

    Each terminated process must free its diskmap as well.

    You must also clear the "protected" bits for the page table itself.
    -----------------------------------------------------------------------------------
*/
void proc_cleanup(pcb_t *proc) {
    /* Look up the process's page table */
    pte_t *page_table_entry = (((*proc).saved_ptbr) * PAGE_SIZE + mem);
    /* Iterate the page table and clean up each valid page */
    for (size_t i = 0; i < NUM_PAGES; i++) {
        pte_t *page_pointer = &page_table_entry[i];
        if ((*page_pointer).valid > 0) {
            (*page_pointer).valid = 0;
            (&frame_table[(*page_pointer).pfn]).mapped = 0;
        }
        if ((*page_pointer).swap > 0) {
            swap_free(page_pointer);
        }
    }

    /* Free the diskmap */
    diskmap_free(*proc);
    /* Free the page table itself in the frame table */
    &frame_table[(*proc).saved_ptbr].mapped = 0;
    &frame_table[(*proc).saved_ptbr].protected = 0;

}

#pragma GCC diagnostic pop