function abs(x) {
    if (x < 0) {
        return 0 - x;
    } else {
        return x;
    }
}

function run() {
    assert(abs(0 - 7) == 7);
    assert(abs(7) == 7);
}

run();
