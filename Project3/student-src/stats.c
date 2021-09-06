#include "paging.h"
#include "stats.h"

/* The stats. See the definition in stats.h. */
stats_t stats;

/*  --------------------------------- PROBLEM 10 --------------------------------------
    Checkout PDF section 10 for this problem

    Calculate any remaining statistics to print out.

    You will need to include code to increment many of these stats in
    the functions you have written for other parts of the project.

    Use this function to calculate any remaining stats, such as the
    average access time (AAT).

    You may find the #defines in the stats.h file useful.
    -----------------------------------------------------------------------------------
*/
void compute_stats() {
    double access_time = stats.access * MEMORY_READ_TIME;
    double read_time = stats.read * DISK_PAGE_READ_TIME;
    double write_time = stats.write * DISK_PAGE_WRITE_TIME;
    double total_time = access_time + read_time + write_time;
    double aat = total_time / stats.access;
    stats.aat = aat;
}
