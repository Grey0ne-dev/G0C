int main() {
    int total = 0;
    for (int i = 1; i <= 4; i = i + 1) {
        total = total + i;
    }
    int j = 0;
    while (j < 3) {
        total = total + 2;
        j = j + 1;
    }
    print(total);
    return 0;
}
