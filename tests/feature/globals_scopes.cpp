int value = 7;

int main() {
    print(value);
    {
        int value = 2;
        print(value);
    }
    print(value);
    return 0;
}
