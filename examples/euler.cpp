#include <iostream>

int main() {
    int iterations = 15;
    float e = 1.0f;     // sum starts with 1/0! = 1
    float term = 1.0f;  // current term = 1/0!

    for (int i = 1; i <= iterations; ++i) {
        term = term / i; // recurrence: term_k = term_{k-1} / k -> 1/k!
        e = e + term;
    }

    std::cout << e << std::endl;
    return 0;
}
