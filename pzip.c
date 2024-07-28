#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/sysinfo.h>

#define CHUNK_SIZE (1024 * 1024)

// Structure to hold data for each thread
typedef struct {
    char *input;    // Pointer to the input data
    size_t start;   // Start position of the chunk
    size_t end;     // End position of the chunk
    size_t out_size; // Size of the compressed output
    char *output;   // Pointer to the compressed output data
} thread_data;

// Function prototypes
void *compress_chunk(void *arg); // Function to compress a chunk of data
void write_output(thread_data *data, int threads_count, FILE *output_file); // Function to write the compressed output
void process_file(const char *input_file); // Function to process the input file

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "pzip: file1 [file2 ...]\n"); // Print usage if no file is provided
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        process_file(argv[i]); // Process each file provided as an argument
    }

    return 0;
}

// Function to compress a chunk of data
void *compress_chunk(void *arg) {
    thread_data *data = (thread_data *)arg;
    char *input = data->input + data->start;
    size_t len = data->end - data->start;

    char *output = (char *)malloc(5 * len); // Allocate memory for the output (4 bytes for count + 1 byte for character)
    if (!output) {
        fprintf(stderr, "pzip: malloc failed\n"); // Print error if memory allocation fails
        pthread_exit(NULL);
    }

    char current = input[0];
    int count = 1;
    size_t out_idx = 0;

    for (size_t i = 1; i < len; i++) {
        if (input[i] == current) {
            count++; // Increment count if current character is the same as previous character
        } else {
            memcpy(output + out_idx, &count, 4); // Write the count to output
            out_idx += 4;
            output[out_idx++] = current; // Write the character to output
            current = input[i]; // Update current character
            count = 1; // Reset count
        }
    }

    memcpy(output + out_idx, &count, 4); // Write the last count to output
    out_idx += 4;
    output[out_idx++] = current; // Write the last character to output

    data->output = output;
    data->out_size = out_idx; // Update the output size
    pthread_exit(NULL);
}

// Function to write the compressed output
void write_output(thread_data *data, int threads_count, FILE *output_file) {
    for (int i = 0; i < threads_count; i++) {
        fwrite(data[i].output, 1, data[i].out_size, output_file); // Write each thread's output to the file
        free(data[i].output); // Free the allocated memory for output
    }
}

// Function to process the input file
void process_file(const char *input_file) {
    int fd = open(input_file, O_RDONLY); // Open the file
    if (fd == -1) {
        fprintf(stderr, "pzip: cannot open file\n"); // Print error if file cannot be opened
        exit(1);
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        fprintf(stderr, "pzip: cannot stat file\n"); // Print error if file stats cannot be obtained
        exit(1);
    }

    char *input = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0); // Memory-map the file
    if (input == MAP_FAILED) {
        fprintf(stderr, "pzip: cannot mmap file\n"); // Print error if memory-mapping fails
        exit(1);
    }

    int threads_count = get_nprocs(); // Get the number of available processors
    pthread_t threads[threads_count];
    thread_data data[threads_count];
    size_t chunk_size = st.st_size / threads_count;

    for (int i = 0; i < threads_count; i++) {
        data[i].input = input;
        data[i].start = i * chunk_size;
        data[i].end = (i == threads_count - 1) ? st.st_size : (i + 1) * chunk_size;
        pthread_create(&threads[i], NULL, compress_chunk, &data[i]); // Create a thread to compress a chunk of data
    }

    for (int i = 0; i < threads_count; i++) {
        pthread_join(threads[i], NULL); // Wait for all threads to finish
    }

    write_output(data, threads_count, stdout); // Write the compressed output to stdout

    if (munmap(input, st.st_size) == -1) {
        fprintf(stderr, "pzip: cannot munmap file\n"); // Print error if unmapping fails
        exit(1);
    }

    close(fd); // Close the file
}
