function sumTo(n) {
    let i = 0;
    let s = 0;
    while (i < n) {
        s = s + i;
        i = i + 1;
    }
    return s;
}

function run() {
    assert(sumTo(5) == 10);
}

run();
