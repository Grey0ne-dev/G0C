typedef struct Inner {
    int value;
} Inner;

typedef struct Outer {
    Inner inner;
    int other;
} Outer;

typedef struct Node {
    int value;
    struct Node* next;
} Node;

int main() {
    Outer outer;
    outer.inner.value = 7;
    outer.other = 2;
    print(outer.inner.value + outer.other);

    Node first = {1};
    Node second = {8};
    first.next = &second;
    print(first.next->value);
    return 0;
}
