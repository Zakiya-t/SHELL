#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/*
 * CPU-Intensive Workload for Shell Scheduling Experiments
 * Usage: ./workload <task_name> <iterations_in_millions>
 */

static unsigned long is_prime(unsigned long n)
{
    if (n <= 1) return 0;
    if (n <= 3) return 1;
    if (n % 2 == 0 || n % 3 == 0) return 0;
    for (unsigned long i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 0;
    }
    return 1;
}

int main(int argc, char *argv[])
{
    const char *name = (argc > 1) ? argv[1] : "Task";
    unsigned long iterations = (argc > 2) ? strtoul(argv[2], NULL, 10) : 30000000UL;

    printf("[Workload %s] Started (PID: %d, Target: %lu iterations)\n", 
           name, (int)getpid(), iterations);
    fflush(stdout);

    unsigned long prime_count = 0;
    for (unsigned long i = 2; i < iterations; i++) {
        if (is_prime(i)) {
            prime_count++;
        }
    }

    printf("[Workload %s] Completed (Found %lu primes)\n", name, prime_count);
    fflush(stdout);
    return 0;
}
