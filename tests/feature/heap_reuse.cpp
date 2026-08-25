int main() {
    int* first = new int[10];
    int* neighbor = new int[10];
    neighbor[0] = 2;
    delete first;
    int* second = new int[5];
    second[0] = 7;
    int* tail = new int[20];
    tail[0] = 9;
    print(second[0]);
    print(neighbor[0]);
    print(tail[0]);
    delete second;
    delete neighbor;
    delete tail;
    return 0;
}
