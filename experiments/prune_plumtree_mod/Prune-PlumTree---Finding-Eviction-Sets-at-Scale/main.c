#define _GNU_SOURCE
#include "main.h"
#include "utils.h"

// Global Variabels.
void *CandAddressesPool, *RepAddressesPool, *PruneGarbage;
void ***Mapping;
int *EvictionSetSize;
void **GarbageCands, **GarbageReps;
int GarbageCandsIDX, GarbageRepsIDX, PruneGarbageSize, Stride, S, MappingIdx = 0;
int timeIDX;
int mappingSize_array[BufferSize] = {0};
clock_t start_time, time_array[BufferSize] = {0};

void Prime(void *address, int direction)
{
    memoryaccess(address, direction);
    memoryaccess(address, direction);
    memoryaccess(address, direction);
    // memoryaccess(address,direction);
    // memoryaccess(address,direction);
    gaurd();
}

Probe_Args probe(void *p, int N_c, char *MissHit)
{
    Probe_Args ret;
    int cnt = N_c, removed = 0, i = 0;
    void *head = p;
    void **removed_addresses = (void **)malloc(N_c * sizeof(void *));
    do
    {
        if (MissHit[i++] != 0)
        {
            removed_addresses[removed] = p;
            removed++;
            LNEXT(LNEXT(NEXTPTR(p))) = LNEXT(p);
            LNEXT(OFFSET(LNEXT(p), sizeof(void *))) = LNEXT(NEXTPTR(p));
            if (p == head)
                head = LNEXT(p);
        }
        p = LNEXT(p);
        cnt--;
    } while (cnt);

    for (int i = 0; i < removed; i++)
    {
        LNEXT(removed_addresses[i]) = removed_addresses[(i + 1) % removed];
        LNEXT(OFFSET(removed_addresses[i], sizeof(void *))) = removed_addresses[(i + removed - 1) % removed];
    }

    ret.first = head;
    ret.N1 = N_c - removed;
    ret.sec = removed_addresses[0];
    ret.N2 = removed;
    free(removed_addresses);
    return ret;
}

void External_Voting(void *p, char *MissHit, int direction, int size)
{
    int accsesTime, i;
    void *pp = p;
    if (direction)
    {
        i = 0;
        do
        {
            accsesTime = memaccesstime((void *)p);
            if (accsesTime >= THRESHOLD)
                MissHit[i++]++;
            else
                i++;
            p = LNEXT(p);
        } while (p != (void *)pp);
    }
    else
    {
        i = size - 1;
        do
        {
            accsesTime = memaccesstime((void *)p);
            if (accsesTime >= THRESHOLD)
                MissHit[i--]++;
            else
                i--;
            p = LNEXT(NEXTPTR(p));
        } while (p != (void *)pp);
    }
}

void ProbeInfo(void *head, void *Rhead, void *tail, void *Rtail, char *MissHit, int size)
{
    // Clean the relvent sets.
    // Prime(head,FW);
    // memoryaccess(head,FW);
    flush(head);
    // Prime(Rhead,FW);
    // memoryaccess(Rhead,FW);
    flush(Rhead);

    Prime(head, FW);
    Prime(Rhead, FW);
    External_Voting(head, MissHit, FW, size);

    flush(head);
    flush(Rhead);

    Prime(tail, !FW);
    Prime(Rtail, FW);
    External_Voting(tail, MissHit, !FW, size);
}

Struct Probe(Struct addresses)
{
    Struct ret;
    Probe_Args probe_args;
    void *head, *Rhead, *tail, *Rtail;
    char *MissHit = (char *)calloc(addresses.N_c, sizeof(char));

    Rhead = addresses.Representatives;
    head = addresses.candidates;
    tail = LNEXT(NEXTPTR(head));
    Rtail = LNEXT(NEXTPTR(Rhead));

    // PROBING
    ProbeInfo(head, Rhead, tail, Rtail, MissHit, addresses.N_c);

    probe_args = probe(head, addresses.N_c, MissHit);

    ret.candidates = probe_args.first;
    ret.N_c = probe_args.N1;
    ret.Representatives = addresses.Representatives;
    ret.N_R = addresses.N_R;
    ret.SeconedHalf = probe_args.sec;
    ret.N_Sec = probe_args.N2;

    free(MissHit);
    // flush(ret.candidates);
    // if(ret.N_Sec) flush(ret.SeconedHalf);
    // flush(ret.Representatives);

    return ret;
}

int reduction_iterative(Struct addresses)
{
    Struct cur, stack[100];
    int top = -1;
    Struct tmp, First, Second;
    int oldN_R, i;
    void *Rhead, *Rtail, *middleHead;
    int cnt = 0;
    void *prev = NULL;
    int size = -1;

    // push initial cur to stack
    stack[++top] = addresses;

    int result = 0;

    while (top >= 0)
    {
        // pop the top address from the stack
        cur = stack[top--];

        // No eviction set.
        if ((cur.N_c < W) || (cur.N_R == 0))
        {
            if (cur.N_R)
                collectReps(cur.Representatives);
            if (cur.N_c)
                collectCands(cur.candidates);
            continue;
        }
        // Second stop condition
        if (cur.N_R == 1)
        {
            // Perfect eviction set of size W
            if ((cur.N_c == W))
            {
                if (checkEviction(cur.candidates, cur.Representatives, cur.candidates))
                {
                    collectEvictionSet(cur);
                    result++;
                }
                else
                {
                    collectReps(cur.Representatives);
                    collectCands(cur.candidates);
                }
                continue;
            }

            // In case of too much repetitions on the same group of cur
            if (cnt >= 10)
            {
                // In case the group is not too large and form an eviction set -> collect it as eviction set
                if ((cur.N_c < (3 * W / 2)) && checkEviction(cur.candidates, cur.Representatives, cur.candidates))
                { // We can allow eviction sets with more then W! (cur.N_c < (3*W/2))
                    // printf("Cought: %d\n",cur.N_c);
                    collectEvictionSet(cur);
                    size = -1;
                    result++;
                    continue;
                }
                // Otherwise collect that group to the garbage collector
                else
                {
                    collectReps(cur.Representatives);
                    collectCands(cur.candidates);
                    size = -1;
                    continue;
                }
            }
            if ((prev == cur.Representatives) && (size == cur.N_c))
                cnt++;
            else
            {
                prev = cur.Representatives;
                size = cur.N_c;
                cnt = 0;
            }
        }

        // Split the Representatives to two halves.
        Rhead = cur.Representatives;
        Rtail = LNEXT(NEXTPTR(Rhead));
        middleHead = Rhead;
        for (i = 0; i < ceil((float)cur.N_R / 2); i++)
            middleHead = LNEXT(middleHead);
        LNEXT(LNEXT(NEXTPTR(middleHead))) = Rhead;
        LNEXT(Rtail) = middleHead;
        LNEXT(OFFSET(Rhead, sizeof(void *))) = LNEXT(NEXTPTR(middleHead));
        LNEXT(OFFSET(middleHead, sizeof(void *))) = Rtail;
        oldN_R = cur.N_R;
        cur.N_R = ceil((float)cur.N_R / 2);
        // flush(middleHead);

        // Perform the probe stage.
        tmp = Probe(cur);

        // Hits
        First.candidates = tmp.candidates;
        First.N_c = tmp.N_c;
        First.Representatives = middleHead;
        First.N_R = oldN_R - tmp.N_R;

        // Misses
        Second.candidates = tmp.SeconedHalf;
        Second.N_c = tmp.N_Sec;
        Second.Representatives = tmp.Representatives;
        Second.N_R = tmp.N_R;

        // push the cur to the stack
        stack[++top] = Second;
        stack[++top] = First;
    }

    return result;
}

void PruneInfo(void *head, void *tail, char *MissHit, int NumExp, int size, void *mapping_head)
{
    void *middle = getPointer(head, size / 2);
    // flush(head);
    Prune_memoryaccess(head, head); // -> new
    Prune_memoryaccess(head, head); // -> new
    for (int iterations = 0; iterations < NumExp; iterations++)
    {
        Prune_memoryaccess(head, head);
        Prune_memoryaccess(head, head);
        // Prune_memoryaccess(head,head);
        // Prune_memoryaccess(head,head);
        Prune_memoryaccess(middle, head);

        if (mapping_head != NULL)
            Prime(mapping_head, FW);

        External_Voting(tail, MissHit, !FW, size);
    }
    for (int j = 0; j < size; j++)
    {
        if (MissHit[j] == NumExp)
            MissHit[j] = 1;
        else
            MissHit[j] = 0;
    }
}

Struct Prune(Struct addresses)
{
    Probe_Args probe_args;
    void *head, *tail;
    char *MissHit1;
    char *MissHit2;
    int NumExp = 4;
    printf("PRUNE started with %d addresses\n", addresses.N_c);

    PruneGarbageSize = 0;
    PruneGarbage = NULL;

    for (int i = 0; i < 2; i++)
    {
        MissHit1 = (char *)calloc(addresses.N_c, sizeof(char));
        MissHit2 = (char *)calloc(addresses.N_c, sizeof(char));
        head = addresses.candidates;
        tail = LNEXT(NEXTPTR(head));
        PruneInfo(head, tail, MissHit1, NumExp, addresses.N_c, NULL);
        PruneInfo(head, tail, MissHit2, NumExp, addresses.N_c, NULL);
        for (int j = 0; j < addresses.N_c; j++)
            if (MissHit1[j] != MissHit2[j])
                MissHit1[j] = 0; // if(MissHit1[j] != MissHit2[j]) MissHit1[j] = 0;
        probe_args = probe(head, addresses.N_c, MissHit1);

        addresses.N_c = probe_args.N1;
        addresses.candidates = probe_args.first;
        PruneGarbageSize += probe_args.N2;

        if (probe_args.N2 != 0)
        {
            if (PruneGarbage == NULL)
                PruneGarbage = probe_args.sec;
            else
                mergeLists(PruneGarbage, probe_args.sec);
            // flush(PruneGarbage);
        }
        // if(addresses.N_c != 0) flush(addresses.candidates);
        free(MissHit1);
        free(MissHit2);
    }
    printf("PRUNE finished with %d addresses\n", addresses.N_c);
    printf("PruneGarbage size: %d\n", PruneGarbageSize);
    return addresses;
}

Struct BuildTrees(Struct addresses)
{
    int prev = 0, iter = 0, improveFactor;
    static int NumberOfEvictionSetsFound = 0;
    if (MappingIdx == 0)
    {
        NumberOfEvictionSetsFound = 0;
        improveFactor = 50;
    }
    else
        improveFactor = 100;
    if (addresses.N_c == 0 || addresses.N_R == 0)
        return addresses;

    do
    {
        iter++;
        prev = NumberOfEvictionSetsFound;
        NumberOfEvictionSetsFound += reduction_iterative(addresses);
        time_array[timeIDX] += clock() - start_time;               // For ploting figure;
        mappingSize_array[timeIDX++] += NumberOfEvictionSetsFound; // For ploting figure;
        printf("Number of eviction sets found: %d\n", NumberOfEvictionSetsFound);
        addresses = logsGarbege();
        if (timeIDX >= BufferSize)
            break; // For ploting figure;
    } while (((NumberOfEvictionSetsFound - prev) > floor((float)prev / improveFactor)) || iter < 2); //|| iter < 3 to make sure that the bulid tree function has at least 2 iterations
    printf("BuildTrees Iterations: %d\n", iter);
    return addresses;
}

Struct map_LLC(float LLC_Cover, Struct addresses)
{
    int N_c = addresses.N_c, N_R = addresses.N_R, iter = 0;
    Probe_Args tmp;
    while (((float)MappingIdx / S) * 100 < LLC_Cover)
    {
        printf("%%%% Round %d %%%%\n", ++iter);
        addresses = Prune(addresses);
        addresses = BuildTrees(addresses);
        if (timeIDX >= BufferSize)
            break; // For ploting figure;
        if (PruneGarbageSize > 1)
        {
            // Remove addresses from prune garbage
            if (iter < 3)
            {
                tmp = remove_congrunt_addresses(PruneGarbage, PruneGarbageSize);
                PruneGarbage = tmp.first;
                PruneGarbageSize = tmp.N1;
            }
            mergeLists(addresses.candidates, PruneGarbage);
            addresses.N_c += PruneGarbageSize;
        }
        if ((addresses.N_R == 0) || (addresses.N_c == 0))
            break;
    }
    addresses.N_c = N_c;
    addresses.N_c = N_R;
    return addresses;
}

void menu()
{
    int option = 2;
    // printf("===========================================================================\n");
    // printf("Choose one of the following options:\n");
    // printf("1. Map the whole LLC sets !independently!\n");
    // printf("2. Map the whole LLC !page heads! sets\n");
    // printf("===========================================================================\n");

    // if (scanf("%d",&option) != 1){
    // 	printf("Failed to read integer.\n");
    // 	exit(0);
    // }

    switch (option)
    {
    case 1:
        Stride = BlockSize;
        S = SetsLLC;
        break;
    case 2:
        Stride = PageSize;
        S = SetsLLC / (PageSize / BlockSize);
        break;
    default:
        printf("Error! wrong operation selected\n");
    }
}

int main(int argc, char const *argv[])
{

    if (argc != 2)
    {
        fprintf(stderr, "Usage: ./main $PAGE_OFFSET\n");
        exit(0);
    }

    int offset = atoi(argv[1]);
    printf("Page Offset: %d\n", offset);

    Struct addresses, tmp;
    int NumExp = 1, AVGmappingSize = 0, WarmUp = 1;
    float LLC_Cover = 99;
    double AVGtime = 0;
    // set_cpu();
    menu();

    for (int i = 0; i < NumExp; i++)
    {
        printf("\n\n========== Experiment - %d ===============\n", i);
        timeIDX = 0;
        addresses = prepareForMapping(offset);
        clock_t clocker = clock();

        start_time = clock();
        tmp = map_LLC(LLC_Cover, addresses);
        start_time = clock() - start_time;

        if (i > WarmUp - 1)
        { // First experiment for warming the system.
            AVGmappingSize += MappingIdx;
            AVGtime += (double)start_time / CLOCKS_PER_SEC;
        }
        else
        {
            for (int k = 0; k < BufferSize; k++)
            {
                mappingSize_array[k] = 0;
                time_array[k] = 0;
            }
        }
        clocker = clock() - clocker;
        printf("CheckResult finished with %d mistakes and the mapping took %f seconds (%f seconds).\n", CheckResult(), (double)start_time / CLOCKS_PER_SEC, (double)clocker / CLOCKS_PER_SEC);

        free(Mapping);
        free(EvictionSetSize);
        free(GarbageCands);
        free(GarbageReps);
        munmap(CandAddressesPool, tmp.N_c * Stride);
        munmap(RepAddressesPool, tmp.N_R * Stride);
    }

    statistics(NumExp, AVGmappingSize, AVGtime);
    return 0;
}
