# Own implementation of stdio.h

  ## [EN]
  This project aims to deepen the concepts of: 
  - I / O buffering 
  - Creating process and running executable files 
  - Redirecting standard inputs and outputs
  - Generating dynamic libraries

  In this project, a minimal implementation of the stdio library is made, which allows working with files. The library will implement an internal SO_FILE structure (similar to the FILE in the standard C library), along with read / write functions. It will also provide buffering functionality.

  Finally, a dynamic library called libso_stdio.so/so_stdio.dll will be generated using Makefile.

  ## [RO]
  Scopul acestui proiect este aprofundarea conceptelor de:
    - I/O buffering
    - Crearea de procese și rularea de fișiere executabile
    - Redirectearea intrărilor și ieșirilor standard
    - Generarea de biblioteci dinamice

  In acest proiect se realizeza o implementare minimala a bibliotecii stdio, care permite lucrul cu fisiere. 
  Biblioteca va implementa o structura interna SO_FILE (similar cu FILE din biblioteca standard C), 
impreuna cu functiile de citire/scriere. De asemenea, va oferi functionalitatea de buffering.

  In final se va genera folosing Makefile o biblioteca dinamica numita libso_stdio.so/so_stdio.dll.
