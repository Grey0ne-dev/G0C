typedef struct Point {
    int x;
} Point;

int main() {
    Point value = {1};
    Point* pointer = &value;
    return pointer.x;
}
