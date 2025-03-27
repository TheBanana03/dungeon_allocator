#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>

typedef struct {
    u_int32_t numInstances;
    u_int32_t numTanks;
    u_int32_t numHealers;
    u_int32_t numDPS;
    u_int32_t totalParties;
    sem_t partyReady;
    sem_t freeInstance;
    u_int8_t minTime;
    u_int8_t maxTime;
    u_int32_t *partyCount;
    bool exitFlag;
} GameState;

typedef struct {
    GameState *state;
    int instanceID; // Unique instance ID
} ThreadData;

void *run_instance(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    GameState *state = data->state;
    int instanceID = data->instanceID;

    while (1) {
        sem_wait(&state->partyReady);

        if (state->exitFlag) break;

        pthread_t tid = pthread_self();
        int clearTime = rand() % (state->maxTime - state->minTime + 1) + state->minTime;

        printf("[Thread %lu | Instance %d] Instance active (clearing in %d seconds)\n", tid, instanceID, clearTime);
        sleep(clearTime);
        printf("[Thread %lu | Instance %d] Instance finished\n", tid, instanceID);

        state->partyCount[instanceID]++;

        sem_post(&state->freeInstance);
    }

    printf("[Thread %lu | Instance %d] Exiting...\n", pthread_self(), instanceID);
    return NULL;
}

void queue_manager(GameState *state) {
    pthread_t threads[state->numInstances];
    ThreadData threadData[state->numInstances];

    for (int i = 0; i < state->numInstances; i++) {
        threadData[i].state = state;
        threadData[i].instanceID = i;
        pthread_create(&threads[i], NULL, run_instance, &threadData[i]);
    }

    while (state->numTanks >= 1 && state->numHealers >= 1 && state->numDPS >= 3) {
        sem_wait(&state->freeInstance);

        state->numTanks--;
        state->numHealers--;
        state->numDPS -= 3;
        state->totalParties++;

        sem_post(&state->partyReady);
        sleep(1);
    }

    state->exitFlag = true;
    for (int i = 0; i < state->numInstances; i++) {
        sem_post(&state->partyReady);
    }

    for (int i = 0; i < state->numInstances; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&state->partyReady);
}

int is_valid_number(const char *str) {
    if (!str || *str == '\0') return 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i])) return 0;
    }
    return 1;
}

int read_config(const char *filename, GameState *state) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return 0;
    }

    bool inputGood = true;
    char line[50];

    while (fgets(line, sizeof(line), file)) {
        char key[50];
        char valueStr[50];

        if (sscanf(line, "%49[^:]: %49s", key, valueStr) != 2) {
            fprintf(stderr, "Invalid format in config: %s\n", line);
            inputGood = false;
            continue;
        }

        if (!is_valid_number(valueStr)) {
            fprintf(stderr, "Invalid value for %s: %s (must be a positive number)\n", key, valueStr);
            inputGood = false;
            continue;
        }

        unsigned long value = strtoul(valueStr, NULL, 10);

        if (strcmp(key, "instances") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: instances value too large\n");
                inputGood = false;
            }
            state->numInstances = (uint32_t)value;
        } else if (strcmp(key, "tanks") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: tanks value too large\n");
                inputGood = false;
            }
            state->numTanks = (uint32_t)value;
        } else if (strcmp(key, "healers") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: healers value too large\n");
                inputGood = false;
            }
            state->numHealers = (uint32_t)value;
        } else if (strcmp(key, "dps") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: dps value too large\n");
                inputGood = false;
            }
            state->numDPS = (uint32_t)value;
        } else if (strcmp(key, "min time") == 0) {
            if (value > UINT8_MAX) {
                fprintf(stderr, "Error: min time value too large (max %u)\n", UINT8_MAX);
                inputGood = false;
            }
            state->minTime = (uint8_t)value;
        } else if (strcmp(key, "max time") == 0) {
            if (value > UINT8_MAX) {
                fprintf(stderr, "Error: max time value too large (max %u)\n", UINT8_MAX);
                inputGood = false;
            }
            state->maxTime = (uint8_t)value;
        } else {
            fprintf(stderr, "Unknown config key: %s\n", key);
            inputGood = false;
        }
    }

    fclose(file);

    if (!inputGood) {
        exit(EXIT_FAILURE);
    }

    return 1;
}

int main() {
    GameState state = {0};

    if (!read_config("config.txt", &state)) {
        return 1;
    }

    state.totalParties = 0;
    sem_init(&state.partyReady, 0, 0);
    sem_init(&state.freeInstance, 0, state.numInstances);

    state.partyCount = (u_int32_t *)calloc(state.numInstances, sizeof(u_int32_t));

    srand(time(NULL));

    queue_manager(&state);

    // Print per-instance party count
    for (int i = 0; i < state.numInstances; i++) {
        printf("Instance %d handled %d parties.\n", i, state.partyCount[i]);
    }

    // Cleanup allocated memory
    free(state.partyCount);

    printf("\n=== Summary ===\n");
    printf("Total parties served: %d\n", state.totalParties);
    printf("DPS players remaining: %d\n", state.numDPS);
    printf("Healers remaining: %d\n", state.numHealers);
    printf("Tanks remaining: %d\n", state.numTanks);

    return 0;
}
