## OpenCV: Open Source Computer Vision Library

> **本项目构建说明**: 此 OpenCV 4.7.0 源码在 PaddleOCR-MinGW 项目中使用 MinGW GCC 11.2.0 编译。预编译版本位于 `libs/opencv_install_gcc/`。构建命令：
> ```batch
> cd src\opencv-4.7.0
> mkdir build && cd build
> cmake .. -G "MinGW Makefiles" ^
>   -DCMAKE_C_COMPILER=..\..\..\toolchain\mingw\bin\gcc.exe ^
>   -DCMAKE_CXX_COMPILER=..\..\..\toolchain\mingw\bin\g++.exe ^
>   -DCMAKE_INSTALL_PREFIX=..\..\..\libs\opencv_install_gcc ^
>   -DBUILD_SHARED_LIBS=ON
> ..\..\..\toolchain\mingw\bin\mingw32-make.exe -j12
> ..\..\..\toolchain\mingw\bin\mingw32-make.exe install
> ```

### Resources

* Homepage: <https://opencv.org>
  * Courses: <https://opencv.org/courses>
* Docs: <https://docs.opencv.org/4.x/>
* Q&A forum: <https://forum.opencv.org>
  * previous forum (read only): <http://answers.opencv.org>
* Issue tracking: <https://github.com/opencv/opencv/issues>
* Additional OpenCV functionality: <https://github.com/opencv/opencv_contrib> 


### Contributing

Please read the [contribution guidelines](https://github.com/opencv/opencv/wiki/How_to_contribute) before starting work on a pull request.

#### Summary of the guidelines:

* One pull request per issue;
* Choose the right base branch;
* Include tests and documentation;
* Clean up "oops" commits before submitting;
* Follow the [coding style guide](https://github.com/opencv/opencv/wiki/Coding_Style_Guide).
