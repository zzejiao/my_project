#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h> // Include the header for fstat
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

typedef struct myRecord{
    int key;
    char *value;
}record;

typedef struct thr_data {
  int start;//start of the current thread's working range
  int end; //end of the current thread's working range
  int mid;
} thr_data;

record *records;//global static pointer to the array of records


//function prototypes
void *merge_sort(void *arg);
void merge_sort_recursion(thr_data* data, int left, int right);
void merge_sorted_arrays(thr_data* data, int left, int middle, int right);
void mergeSort(thr_data thr1, thr_data thr2);


int main (int argc, char *argv[]){
    //take arguments
    char *input = argv[1];
    char *output = argv[2];
    int numThreads = atoi(argv[3]);

    //mapping input file to memory
    FILE* inputFile = fopen(input, "r");
    if (inputFile == NULL) {
        printf("Could not open file");
        return 0;
    }

    //size variable to get the number of bytes in file
    struct stat st;
    stat(input, &st);
    uint size = st.st_size;
    uint recordCount =size/100;
    
    //need fd for mmap
    int fd = fileno(inputFile);

    //map file to memory, addr is starting address
    char *addr = mmap(NULL, size, 1, 1, fd, 0);

    //Copy input file data to struct to sorts
    records = malloc(sizeof(record) * recordCount);
    for(uint i=0;i<recordCount;i++){
        records[i].value= malloc(sizeof(char)*100);
        //copy 100 bytes into memory
        memcpy(records[i].value, addr, 100);
        records[i].key=*(int*)records[i].value;
        addr=addr+100;
    }

    //If more threads than recordCnt, chose to execute in (recordCnt-1)threads
    // since we don't need that much threads to handle records, if each threads sort 1 record, it means not sorting
    if(numThreads>=recordCount){
        numThreads = recordCount-1;
    }
    
    //Create array of threads
    pthread_t threads[numThreads];
    thr_data threadData[numThreads];
    int currentStart = 0;
    int range = recordCount/numThreads;
    int additionalIndex=recordCount%numThreads;
    int rc;//for pthread create

    
    //distributing each thread's workload
    for (int i =0; i < numThreads; i++)
    {
        //storing work range's index????
        threadData[i].start =currentStart;
        threadData[i].end = currentStart+range-1;
        currentStart = currentStart+range;
    }

    if(additionalIndex) //if there's extra records, put them in the final thread's range
        threadData[numThreads-1].end = threadData[numThreads-1].end +additionalIndex;

    //start sorting, calculate time here

    struct timeval start_time, end_time;
    long int execution_time;

    gettimeofday(&start_time, NULL);


    /* create threads */
    for (int i = 0; i < numThreads; ++i) {
        if ((rc = pthread_create(&threads[i], NULL, merge_sort, &threadData[i]))) {
            fprintf(stderr, "error: pthread_create, rc: %d\n", rc);
            return -1;
    }
  }

    //wait for threads to finish
    for (int i =0; i< numThreads; i++)
    {
        pthread_join(threads[i],NULL);
    }

    int thrToMerge = numThreads;
    int scale = 1;//the length that is needed to jump from thr1 to thr2, it will grow by scale = scale*2

    //merge in 1 thread
    while(thrToMerge>1)//number of threads to merge
    {
        //the length that is needed to jump from thr1 to thr2
        
        if(thrToMerge%2)
        {//if thread number are odd, merge the last thread's range to the second last thread
            thrToMerge--;//make it into even number
            int last = thrToMerge*scale;//the odd thread
            int secondLast =last -scale;
            mergeSort(threadData[secondLast],threadData[last]);
            threadData[secondLast].end = threadData[last].end;
        }

        //the thread number are even now
        int thr1=0;
        int thr2 = thr1+scale;
        for(int i =0; i < thrToMerge/2; i ++ ){
            mergeSort(threadData[thr1],threadData[thr2]);
            threadData[thr1].end=threadData[thr2].end;
            thr1=thr2 + scale;
            thr2=thr1 + scale;
        }

        thrToMerge=thrToMerge/2;
        scale = scale*2;
    }
    //done merge

    //calculate end time here and print
    gettimeofday(&end_time, NULL);

    execution_time = ((end_time.tv_sec * 1000000 + end_time.tv_usec) - 
                     (start_time.tv_sec * 1000000 + start_time.tv_usec))/1000.0;

    printf("Execution time: %ld milliseconds\n", execution_time);



    //Add code here that takes all the threads and merges their data, should be similar the merge arrays method below.  
    //This code may need to go before the join statement, not sure

    
    //writed sorted record to output
    FILE* outputFile = fopen(output, "w");
    if (outputFile == NULL) {
        printf("Failed to open output file.\n");
        return 1;
    }

    // write to output
    for(uint i=0;i<recordCount;i++){
    fwrite(records[i].value, sizeof(char), 100, outputFile);
    free(records[i].value);
    }

    //write output to disk
    fsync(fileno(outputFile));

    // Close the output file
    fclose(outputFile);
    
    //Close and unmap input file from memory
    fclose(inputFile);
    munmap(addr, size);
    free(records);

    return 0;
}

void* merge_sort(void *arg){
    //struct record* temp, int length
    struct thr_data* data = (struct thr_data*)arg;
    int start = data->start;
    int end = data->end;
    merge_sort_recursion(data,start,end);
    return NULL;
}

void merge_sort_recursion(thr_data* data, int left, int right){

    if(left < right){
        //get Middle index
        int middle=left + (right-left) / 2;

        //Recurse on smaller struct indexes
        merge_sort_recursion(data,left,middle);
        merge_sort_recursion(data,middle+1,right);

        merge_sorted_arrays(data,left,middle,right);
    }
}
//Office hours questions
//How to write to file: create temp variable when writing and write that instead of writing directly from temp
//Whats going on with the -1: fixed
//How to convert key back to record: unsure
//After chunks sorted, how many threads do I create? IF I have 3 threads originally, do I create two threads and pass it in with more size of what
//Can I just pass it in to my merge method?: Always act liek you have even number of threads, so If I have 5 threads.  Set one aside merge 4 threads then 2 then I have 1.  Merge this one with the thread set aside

void merge_sorted_arrays(thr_data* data, int left, int middle, int right){
    int leftLength=middle - left + 1;
    int rightLength=right - middle;

    // record tempLeft[leftLength];
    // record tempRight[rightLength];
    record *tempLeft = malloc(sizeof(record)*leftLength);
    record *tempRight = malloc(sizeof(record)*rightLength);
   

    for(int i=0;i<leftLength;i++){
        tempLeft[i]=records[left + i];
    }

    //fill in right temp
    for(int i=0;i<rightLength;i++){
        tempRight[i]=records[middle + 1 + i];
    }
    
    //merge sorted arrays
    for(int i=0,j=0,k=left;k<=right;k++){
        if((i<leftLength) && (j>=rightLength || tempLeft[i].key<=tempRight[j].key)){
            records[k] = tempLeft[i];
            i++;
        }
        else{
            records[k] = tempRight[j];
            j++;
        }
    }
    free(tempLeft);
    free(tempRight);    
}

void mergeSort(thr_data thr1, thr_data thr2){
    //get the size of the array to sort in two threads
    int size1 = thr1.end - thr1.start +1;
    int size2 = thr2.end - thr2.start +1;

    //new a temp array to store the sorted elements
    record *temp = malloc(sizeof(record)*(size1+size2));

    //-------------------start sorting, compare the element of 2 threads-------------------
    int i = thr1.start;//for thr1 array
    int j = thr2.start;//for thr2 array

    int k = 0;//for the arraytemp
    while (i < size1+thr1.start && j < thr2.start + size2) {
        if (records[i].key <= records[j].key) {//if record in thr 1 < than in thr2
            temp[k++] = records[i++];
        } else {
            temp[k++] = records[j++];
        }
    }

    // Copy the remaining elements of thr1 to temp array, if any
    while (i < size1+thr1.start) {
        temp[k++] = records[i++];
    }
    // Copy the remaining elements of right[], if any
    while (j < thr2.start + size2) {
        temp[k++] = records[j++];
    }
    //-------------------finish sorting-------------------

    //copy the temp into original array
    for (int i =0; i <size1+size2; i++)
    {
        records[i+thr1.start]= temp[i];
    }
    free(temp);
}

