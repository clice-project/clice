// This is an example file that shows the usage of semantic tokens.
// clang-format off


    char character = 'a';
//    ^               ^
//  keyword       character

     int number = 1;
//    ^           ^
//  keyword     number

    const char* string = "Hello, World!";
//    ^                     ^    
//  keyword               string

    #pragma      once
//     ^           ^
// directive     pragma

    #include     <cstdio>
//     ^           ^
// directive     header

    #define      MAX 100
//     ^          ^
// directive    macro

