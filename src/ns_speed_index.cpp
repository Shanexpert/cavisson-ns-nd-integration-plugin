#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include <stdio.h> 
#include <string.h> 
#include <sys/types.h> 
#include <dirent.h> 
#include <errno.h> 

extern "C"
{
using namespace cv;
extern double *speed_index_val;
extern int get_clip_time_index(char *clip);
int ns_get_speed_index(char *path_snap_shot , struct dirent **snap_shot_dirent , int num_snap_shots)
{
  int i=0, max_time = 0; 
  double intervalScore = 0.0, sum_speed_index = 0.0;
  int rebase_flag = 0;
  int retry_count = 3;
  int rebase_index = 0;
  int speed_index = 0;
  /*Atul: below function sets the number of threads used by OpenCV in parallel OpenMP regions
  If threads == 1, OpenCV(2.4) will disable threading optimizations and run it’s functions sequentially.
  we was facing the issue (Bug 28966) when opencv was creating multiple threads (default threads are number of virtual process) 
  and control return back from the function but thread were alive and NS-Childs were got stuck even parent send SIGTERM to them, resulting
  test got stuck. To resolve this issue we strict the opencv to dissable threading optimzation.
  Note: This will not impact on our performance as we are processing one image at a time */
  setNumThreads(1);

  //openCV API

  if(num_snap_shots == 1)
  {
    return 0;	
  }

  do{
  rebase_flag = 0;
  Mat src_first, hsv_first; 
  Mat src_last, hsv_last;  
  // Histograms 
  MatND hist_first; 
  MatND hist_last;

  char clip_name[256 + 1] = {0};
  snprintf(clip_name,256,"%s/%s",path_snap_shot,snap_shot_dirent[0]->d_name);
  src_first = imread(clip_name,1);
  //fprintf(stderr, "Atul: Thread count %d", getNumThreads()); 
  if(!src_first.data)
  {
    fprintf(stderr, "Error: ns_speed_index.cpp, could not open or find the image.\n");
    return -1;
  }

  snprintf(clip_name,256,"%s/%s",path_snap_shot,snap_shot_dirent[num_snap_shots-1]->d_name);
  src_last = imread(clip_name, 1 );
  if(!src_last.data)
  {
    fprintf(stderr, "Error: ns_speed_index.cpp, could not open or find the image.\n");
    return -1;
  }

  cvtColor( src_first, hsv_first, COLOR_BGR2HSV );
  cvtColor( src_last, hsv_last, COLOR_BGR2HSV );

  // Using 50 bins for hue and 60 for saturation
  int h_bins = 50; int s_bins = 60;
  int histSize[] = { h_bins, s_bins };

  // hue varies from 0 to 179, saturation from 0 to 255
  float h_ranges[] = { 0, 180 };
  float s_ranges[] = { 0, 256 };

  const float* ranges[] = { h_ranges, s_ranges };

  // Use the o-th and 1-st channels
  int channels[] = { 0, 1 };


  // Calculate the histograms for the HSV images
  calcHist( &hsv_first, 1, channels, Mat(), hist_first, 2, histSize, ranges, true, false );
  normalize( hist_first, hist_first, 0, 1, NORM_MINMAX, -1, Mat() );

  calcHist( &hsv_last, 1, channels, Mat(), hist_last, 2, histSize, ranges, true, false );
  normalize( hist_last, hist_last, 0, 1, NORM_MINMAX, -1, Mat() );

  int compare_method = 3;
  double base = compareHist( hist_last, hist_first, compare_method );
  
  for(i = 0; base > 0  && i < num_snap_shots; i++)
  {
    Mat src_test, hsv_test;
    MatND hist_test;
    snprintf(clip_name,256,"%s/%s",path_snap_shot,snap_shot_dirent[i]->d_name);
    src_test = imread(clip_name, 1);
    if(!src_test.data)
    {
      fprintf(stderr, "Error: ns_speed_index.cpp, could not open or find the image.\n");
      return -1;  
    }
    cvtColor( src_test, hsv_test, COLOR_BGR2HSV );

    calcHist( &hsv_test, 1, channels, Mat(), hist_test, 2, histSize, ranges, true, false );
    normalize( hist_test, hist_test, 0, 1, NORM_MINMAX, -1, Mat() );

    // Apply the histogram comparison methods
    double test = compareHist( hist_last, hist_test, compare_method );
    double factor = 100.00/base;
    double distance = test * factor;
    if(distance > 100.00 && retry_count)
    {
      rebase_flag = 1;
      retry_count--;
      rebase_index++;
    }
    double prog = 100.00 - distance;
    int index = get_clip_time_index(snap_shot_dirent[i]->d_name);
    speed_index_val[index] = prog;
   }
  }while(rebase_flag && retry_count);
  max_time = get_clip_time_index(snap_shot_dirent[num_snap_shots-1]->d_name) + 1;
  for(i = 1 ; i < max_time-1 ; i++)
  {
    if(speed_index_val[i] < 0.000001 || speed_index_val[i] > 100.00)
    {
      speed_index_val[i] = speed_index_val[i-1];
    }
  }

  for(i = 0; i < max_time; i++) {
    intervalScore =  (1.00 - (speed_index_val[i]/100.00))*100.00; 
    sum_speed_index += intervalScore;
  }

  speed_index = (int)round(sum_speed_index);

  return speed_index;
}
}
