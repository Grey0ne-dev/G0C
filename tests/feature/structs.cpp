typedef struct Point Point;

struct Point {
    int x;
    int y;
};

typedef struct {
    int value;
} Box;

int sum(Point* point) {
    return point->x + point->y;
}

int main() {
    Box box = {6};
    print(box.value);

    Point original = {3, 4};
    print(sum(&original));

    Point copy = original;
    original.x = 9;
    print(copy.x);

    Point* pointer = &original;
    pointer->y = 5;
    print(sum(pointer));
    return 0;
}
