#include <stdio.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    int framerate;
} VideoFormat;

void swap(VideoFormat* xp, VideoFormat* yp) 
{ 
    VideoFormat temp = *xp; 
    *xp = *yp; 
    *yp = temp; 
} 
  
// Function to perform Selection Sort 
void selectionSort(VideoFormat arr[], int n) 
{ 
    int i, j, min_idx; 
  
    // One by one move boundary of 
    // unsorted subarray 
    for (i = 0; i < n - 1; i++) { 
        // Find the minimum element in 
        // unsorted array 
        min_idx = i; 
        for (j = i + 1; j < n; j++) 
            if (arr[j].width > arr[min_idx].width) 
                min_idx = j; 
  
        // Swap the found minimum element 
        // with the first element 
        swap(&arr[min_idx], &arr[i]); 
    } 
}


int main(void) {
    VideoFormat arr[3];
    arr[0].width = 640;
    arr[0].height = 360;
    arr[0].framerate = 5;

    arr[1].width = 1280;
    arr[1].height = 720;
    arr[1].framerate = 30;

    arr[2].width = 1920;
    arr[2].height = 1080;
    arr[2].framerate = 30;

    selectionSort(arr, 3);

    printf("%d, %d, %d\n", arr[0].width, arr[0].height, arr[0].framerate);
    printf("%d, %d, %d\n", arr[1].width, arr[1].height, arr[1].framerate);
    printf("%d, %d, %d", arr[2].width, arr[2].height, arr[2].framerate);

    return 0;
}
