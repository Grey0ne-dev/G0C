int main() {
    int x = 10;
    int* p = &x;
    *p = *p + 32;
    print(x);
    return 0;
}
