#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>

typedef struct {
    uint32_t numInstances;
    uint32_t numTanks;
    uint32_t numHealers;
    uint32_t numDPS;
    uint32_t totalParties;
    sem_t partyReady;
    sem_t freeInstance;
    u_int8_t minTime;
    u_int8_t maxTime;
    uint32_t* partyCount;
    uint32_t totalTime;
    bool exitFlag;
    bool* activeInstances;
    pthread_mutex_t instanceLock;
} GameState;

typedef struct {
    GameState *state;
    int instanceID;
} ThreadData;

void *run_instance(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    GameState *state = data->state;
    int instanceID = data->instanceID;
    sem_post(&state->freeInstance);

    while (1) {
        sem_wait(&state->partyReady);

        if (state->exitFlag) break;

        pthread_mutex_lock(&state->instanceLock);
        state->activeInstances[instanceID] = true;

        for (int i = 0; i < state->numInstances; i++) {
            printf("[Instance %d: %s]\n", i, state->activeInstances[i] ? "Active" : "Empty");
        }
        printf("\n\n");
        pthread_mutex_unlock(&state->instanceLock);

        pthread_t tid = pthread_self();
        int clearTime = rand() % (state->maxTime - state->minTime + 1) + state->minTime;

        printf("[Thread %lu | Instance %d] Instance active (clearing in %d seconds)\n", tid, instanceID, clearTime);
        sleep(clearTime);
        printf("[Thread %lu | Instance %d] Instance finished\n", tid, instanceID);

        pthread_mutex_lock(&state->instanceLock);
        state->partyCount[instanceID]++;
        state->totalTime += clearTime;
        state->activeInstances[instanceID] = false;
        for (int i = 0; i < state->numInstances; i++) {
            printf("[Instance %d: %s]\n", i, state->activeInstances[i] ? "Active" : "Empty");
        }
        printf("\n\n");
        pthread_mutex_unlock(&state->instanceLock);

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

bool is_valid_number(const char *str) {
    if ((!str || *str == '\0') || (str[0] == '0' && str[1] == '\0')) return false;
    for (int i = 0; str[i] != '\0'; i++) {
        if (!isdigit((unsigned char)str[i])) return false;
    }
    return true;
}

size_t get_max_threads(size_t stack) {
    struct sysinfo info;
    struct rlimit limit;
    size_t ramThreads;
    size_t limThreads;

    // RAM
    sysinfo(&info);
    ramThreads = info.freeram / stack;

    // Max processes
    getrlimit(RLIMIT_NPROC, &limit);
    limThreads = limit.rlim_cur;
    // return 10000;
    return (ramThreads < limThreads) ? ramThreads : limThreads;
}

int read_config(const char *filename, GameState *state, size_t stack) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return 0;
    }
    size_t maxThreads = get_max_threads(stack);

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
            if (value > maxThreads) {
                fprintf(stderr, "Error: Too much instances (max %ld)\n", maxThreads);
                inputGood = false;
            }
            state->numInstances = (uint32_t)value;
        } else if (strcmp(key, "tanks") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: Too much tanks (max %d)\n", UINT32_MAX);
                inputGood = false;
            }
            state->numTanks = (uint32_t)value;
        } else if (strcmp(key, "healers") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: Too much healers (max %d)\n", UINT32_MAX);
                inputGood = false;
            }
            state->numHealers = (uint32_t)value;
        } else if (strcmp(key, "dps") == 0) {
            if (value > UINT32_MAX) {
                fprintf(stderr, "Error: Too much DPS plauers (max %d)\n", UINT32_MAX);
                inputGood = false;
            }
            state->numDPS = (uint32_t)value;
        } else if (strcmp(key, "min time") == 0) {
            if (value > 15) {
                fprintf(stderr, "Error: Minimum time is too large (max 15)\n");
                inputGood = false;
            }
            state->minTime = (uint8_t)value;
        } else if (strcmp(key, "max time") == 0) {
            if (value > 15) {
                fprintf(stderr, "Error: Maxmimum time is too large (max 15)\n");
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
    size_t stack_size;
    pthread_attr_t attr;
    
    pthread_attr_init(&attr);
    pthread_attr_getstacksize(&attr, &stack_size);
    pthread_attr_destroy(&attr);

    if (!read_config("config.txt", &state, stack_size)) {
        return 1;
    }

    state.totalParties = 0;
    sem_init(&state.partyReady, 0, 0);
    sem_init(&state.freeInstance, 0, state.numInstances);

    state.partyCount = (uint32_t*)calloc(state.numInstances, sizeof(uint32_t));
    state.activeInstances = (bool*)calloc(state.numInstances, sizeof(uint32_t));

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
    printf("Total time served: %d\n", state.totalTime);
    printf("DPS players remaining: %d\n", state.numDPS);
    printf("Healers remaining: %d\n", state.numHealers);
    printf("Tanks remaining: %d\n", state.numTanks);

    return 0;
}
