﻿// AUO_N3D_Player.cpp : 此檔案包含 'main' 函式。程式會於該處開始執行及結束執行。
//

#include <iostream>
#include "./GL_player.h"
#include "./cuda_test.cuh" 

#define VSYNC_ON 1

int main()
{
    bool IsFullScreen = true;
    
	/*CUDA TEST
    double A[3], B[3], C[3];

    // Populate arrays A and B.
    A[0] = 5; A[1] = 8; A[2] = 3;
    B[0] = 7; B[1] = 6; B[2] = 4;

    // Sum array elements across ( C[0] = A[0] + B[0] ) into array C using CUDA.
    kernel(A, B, C, 3);

    // Print out result.
    std::cout << "C = " << C[0] << ", " << C[1] << ", " << C[2] << std::endl;
    */
  // Decoder video_decoder;
   //const int video_id1 =video_decoder.add_video_by_path("sample_2.mp4");
   //video_decoder.parse(video_id1);
   //video_decoder. show_video_list();
    gl_player my_player(VSYNC_ON);
    my_player.load_video("sample_2.mp4");
    my_player.play();
    my_player.end();


   //system("pause");
   //video_decoder.delete_video_by_id(5);
   //video_decoder.show_video_list();
    //gl_player player(IsFullScreen);
   



}


