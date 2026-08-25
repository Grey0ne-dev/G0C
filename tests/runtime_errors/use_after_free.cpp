int main() {
    int* value = new int;
    *value = 5;
    delete value;
    print(*value);
    return 0;
}
