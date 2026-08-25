struct Sample {
    int id;
    float value;
};

int main() {
    struct Sample sample = {2, 1.5};
    print(sample.id);
    print(sample.value);
    sample.value = 3.25;
    print(sample.value);
    return 0;
}
