clice 充分吸取了 clangd 教训，用一些全新的设计来解决问题。


- 默认索引所有文件 
- 对于只读文件，使用 LSIF 返回请求结果，speed speed speed !!!!
- 对于可变文件，为它们构建 preamble，以加速后续的构建，然后跑代码补全之类的行为

- clice 如何区分只读文件和可变文件？一般来说，当您第一次尝试编辑这个文件的时候，clice 就会认为这是一个可变的文件，并为其构建 preamble。但是很多情况下可能只是不小心碰到了键盘，并不是真的想要修改，可以在设置里面配置选项，使得第一次更改并报错之后才会让 clice 认为这是一个活跃文件。另外，使用 code action 也会触发。

# non self contained 

clice 解决了一个 clangd 里面长期存在的问题，non self contained 的头文件，这种技巧常见于 glibc 或者 libstdc++ 这样的代码库中。non self-contained file 指的是那些不能单独编译的源文件。只有把他们一同拼接到一个文件中，才能编译通过，而且对于这种文件，往往 include 顺序也十分重要，建议关闭头文件顺序重排。

对于 non self contained 的头文件来说，只要生成的编译数据库中，有包含它的源文件，并且能正常编译，那么 clice 就可以正确为它生成索引，从而支持转到定义等等等操作。

那么 non self contained 的文件对于可变操作如何呢？TODO: ... 初步设想，手动通过字符串拼接的方式来补全缺少的头文件 ...