#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

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

int read_config(const char *filename, GameState *state) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening config file");
        return 0;
    }

    char line[50];
    while (fgets(line, 50, file)) {
        char key[50];
        int value;

        if (sscanf(line, "%[^:]: %d", key, &value) == 2) {
            if (strcmp(key, "instances") == 0) state->numInstances = value;
            else if (strcmp(key, "tanks") == 0) state->numTanks = value;
            else if (strcmp(key, "healers") == 0) state->numHealers = value;
            else if (strcmp(key, "dps") == 0) state->numDPS = value;
            else if (strcmp(key, "min time") == 0) state->minTime = value;
            else if (strcmp(key, "max time") == 0) state->maxTime = value;
        }
    }

    fclose(file);
    return 1;
}

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

    // 🔹 Signal threads to exit
    state->exitFlag = true;
    for (int i = 0; i < state->numInstances; i++) {
        sem_post(&state->partyReady);
    }

    for (int i = 0; i < state->numInstances; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&state->partyReady);
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
