function clampNonNeg(x) {
    if (x < 0) {
        x = 0;
    }
    return x + 1;
}

let result = clampNonNeg(0 - 5);
