#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_COMMANDS 128

typedef enum { CMD_INSERT, CMD_DELETE, CMD_SEARCH, CMD_PRINT } CommandType;

typedef struct {
    CommandType type;
    char name[50];
    uint32_t salary;
} Command;

Command commands[MAX_COMMANDS];
int command_count = 0;

// Parses a single command line
void parse_command(const char *line) {
    char name[64];
    uint32_t salary;

    if (strncmp(line, "insert", 6) == 0) {
        sscanf(line, "insert,%[^,],%u", name, &salary);
        commands[command_count++] = (Command){CMD_INSERT, "", salary};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "delete", 6) == 0) {
        sscanf(line, "delete,%[^,],%*u", name);
        commands[command_count++] = (Command){CMD_DELETE, "", 0};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "search", 6) == 0) {
        sscanf(line, "search,%[^,],%*u", name);
        commands[command_count++] = (Command){CMD_SEARCH, "", 0};
        strcpy(commands[command_count - 1].name, name);
    } else if (strncmp(line, "print", 5) == 0) {
        commands[command_count++] = (Command){CMD_PRINT, "", 0};
    }
}

const char* command_type_to_str(CommandType type) {
    switch (type) {
        case CMD_INSERT: return "INSERT";
        case CMD_DELETE: return "DELETE";
        case CMD_SEARCH: return "SEARCH";
        case CMD_PRINT:  return "PRINT";
        default:         return "UNKNOWN";
    }
}

int main() {
    FILE *fp = fopen("commands.txt", "r");
    if (!fp) {
        perror("commands.txt");
        return 1;
    }

    char line[128];
    fgets(line, sizeof(line), fp); // Skip threads line

    while (fgets(line, sizeof(line), fp)) {
        parse_command(line);
    }

    fclose(fp);

    for (int i = 0; i < command_count; i++) {
        printf("Command %d: %s", i + 1, command_type_to_str(commands[i].type));
        if (commands[i].type != CMD_PRINT) {
            printf(" | Name: %s", commands[i].name);
        }
        if (commands[i].type == CMD_INSERT) {
            printf(" | Salary: %u", commands[i].salary);
        }
        printf("\n");
    }

    return 0;
}
