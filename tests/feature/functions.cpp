int add(int a, int b) {
    return a + b;
}

int muladd(int x, int y, int z) {
    return add(x * y, z);
}

int main() {
    print(muladd(6, 7, 8));
    return 0;
}
