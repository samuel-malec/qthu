function clampNonNeg(x) {
    if (x < 0) {
        x = 0;
    }
    return x + 1;
}

function run() {
    assert(clampNonNeg(0 - 5) == 1);
    assert(clampNonNeg(5) == 6);
}

run();
