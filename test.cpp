template<typename T>
struct X{
    void foo(){};
};

template<>
void X<int>::foo(){

}


