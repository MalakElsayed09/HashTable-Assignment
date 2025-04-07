#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "hashTable.h"

#define MAX_COMMANDS 128

typedef enum { CMD_INSERT, CMD_DELETE, CMD_SEARCH, CMD_PRINT } CommandType;

typedef struct {
    CommandType type;
    char name[50];
    uint32_t salary;
} Command;

Command commands[MAX_COMMANDS];
int command_count = 0;

// Parses a single command line (except the first thread line)
void parse_command(const char *line) {
    char name[64];
    uint32_t salary;

    if (strncmp(line, "insert", 6) == 0) {
        // Expected format: insert,<name>,<salary>
        sscanf(line, "insert,%[^,],%u", name, &salary);
        commands[command_count++] = (Command){CMD_INSERT, "", salary};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "delete", 6) == 0) {
        // Expected format: delete,<name>
        sscanf(line, "delete,%[^,\n]", name);
        commands[command_count++] = (Command){CMD_DELETE, "", 0};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "search", 6) == 0) {
        // Expected format: search,<name>
        sscanf(line, "search,%[^,\n]", name);
        commands[command_count++] = (Command){CMD_SEARCH, "", 0};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "print", 5) == 0) {
        commands[command_count++] = (Command){CMD_PRINT, "", 0};
    }
}

int main() {
    FILE *fp = fopen("commands.txt", "r");
    if (!fp) {
        perror("Error opening commands.txt");
        return 1;
    }

    char line[128];

    // Read the first line to get thread information.
    // Expected format: threads,<thread_count>,<something>
    int thread_count = 0;
    if (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "threads,%d", &thread_count) != 1) {
            fprintf(stderr, "Failed to parse thread count\n");
            fclose(fp);
            return 1;
        }
    } else {
        fclose(fp);
        return 1;
    }

    // Read the rest of the commands.
    while (fgets(line, sizeof(line), fp)) {
        // Ignore blank lines if any.
        if (strlen(line) > 1)
            parse_command(line);
    }
    fclose(fp);

    // Initialize hash table; this opens output.txt in write mode.
    HashTable *table = init_table("output.txt");

    // Write the "Running X threads" header as the first line.
    fprintf(table->output_file, "Running %d threads\n", thread_count);

    // Process each command.
    for (int i = 0; i < command_count; i++) {
        switch (commands[i].type) {
            case CMD_INSERT:
                insert(table, commands[i].name, commands[i].salary);
                break;
            case CMD_DELETE:
                delete(table, commands[i].name);
                break;
            case CMD_SEARCH:
                search(table, commands[i].name);
                break;
            case CMD_PRINT:
                print_table(table);
                break;
            default:
                break;
        }
    }

    // Print the final summary and sorted hash table output.
    print_summary(table);

    // Free resources (this also closes output.txt).
    free_table(table);

    return 0;
}
